# Project: F1 Arduino Notifications

# Claude Instructions
- Only analyse files in /src and /include
- Do not look at .pio or library source files
- This is a PlatformIO ESP32 project

## Overview
An ESP32-based device that tracks the upcoming F1 race calendar and displays session times in the user's local timezone. It fetches the race schedule JSON from GitHub hourly, shows a track circuit image (or session timetable during race week) on a TFT display, and sends a Telegram notification 6 days before each race weekend. Configuration (WiFi credentials, timezone, Telegram bot token/chat ID) is handled via a WiFiManager captive portal, persisted to SPIFFS as JSON. Supports two display backends: the "Cheap Yellow Display" (CYD) ILI9341 TFT, and an ESP32 Trinity HUB75 LED matrix.

## Hardware
- MCU: ESP32 (esp32dev)
- Display option A: ILI9341 320×240 TFT — "Cheap Yellow Display" (CYD / CYD2USB variants)
- Display option B: HUB75 64×64 RGB LED Matrix — ESP32 Trinity board
- Power: USB

## Build Environment
- Framework: Arduino
- Platform: espressif32 @ 6.5.0
- Key Libraries:
  - TFT_eSPI ^2.5.33 (CYD only)
  - PNGdec 1.0.2 exactly pinned (CYD only) — `^1.0.2` was resolving to v1.1.6 which has multi-IDAT chunk corruption and other bugs; pinned to `1.0.2` without caret
  - ESP32 HUB75 LED MATRIX PANEL DMA Display ^3.0.9 (Trinity only)
  - Adafruit GFX Library ^1.11.9 (Trinity only)
  - WiFiManager v2.0.17
  - ESP_DoubleResetDetector ^1.3.2
  - ArduinoJson ^6.21.3 — **uses v6 API** (`DynamicJsonDocument`, `StaticJsonDocument`, `.containsKey()`); upgrading to v7 would require API changes
  - ezTime ^0.8.3
  - UniversalTelegramBot ^1.3.0
  - file-fetcher-arduino (GitHub, unpinned — no version tag)

## Project Structure
```
F1-Notifications/
├── F1-Notifications.ino   # Main sketch — setup/loop, wires all modules together
├── debug.h                # Leveled debug macros (DBG_ERROR/WARN/INFO/VERBOSE) — include in every file using DBG_*
├── secrets.h              # GITIGNORED — dev Telegram credentials and timezone (SECRET_BOT_TOKEN, SECRET_CHAT_ID, SECRET_TIME_ZONE)
├── config.h               # F1Config class — SPIFFS JSON config read/write [has pragma once]
├── display.h              # Abstract F1Display base class (pure virtual interface) [has include guard]
├── cheapYellowLCD.h       # CYD/ILI9341 concrete display implementation
├── matrixDisplay.h        # HUB75 LED matrix concrete display implementation
├── raceLogic.h            # Race JSON fetch, session parsing, Telegram send, event scheduling [has include guard]
├── getImage.h             # Track image download from Imgur + SPIFFS cache; contains its own copy of USERTrust RSA CA cert
├── wifiManagerHandler.h   # WiFiManager captive portal setup + DoubleResetDetector
├── util.h                 # convertRaceName() — long name abbreviations (Imola, Barcelona-Catalunya)
├── githubCert.h           # Let's Encrypt R12 intermediate CA cert for raw.githubusercontent.com (valid 2024-03-13 to 2027-03-12)
├── f1_logo.h              # PROGMEM F1 logo RGB565 bitmap (118×64 px) for schedule screen header
└── races.h                # UNUSED — legacy 2023 season data as a C string literal, kept for debugging only
```

**Debug system:** `debug.h` defines `debugLevel` (runtime-adjustable, default = `DEBUG_LEVEL` build flag) and `DBG_ERROR` / `DBG_WARN` / `DBG_INFO` / `DBG_VERBOSE` macros. Include it at the top of any header that uses these macros.

**Secrets:** `secrets.h` is gitignored. It must be created manually on each machine. Defines `SECRET_BOT_TOKEN`, `SECRET_CHAT_ID`, `SECRET_TIME_ZONE`. The `F1_FORCE_DEV_DEFAULTS` flag in `config.h` (comment out for production) uses these to override any SPIFFS-saved credentials.

**Note:** Only `display.h`, `raceLogic.h`, and `config.h` have include guards / pragma once. `cheapYellowLCD.h`, `matrixDisplay.h`, `getImage.h`, `util.h`, and `wifiManagerHandler.h` still rely on single-include discipline.

## Pin Mapping
| Function      | GPIO | Notes                              |
|---------------|------|------------------------------------|
| TFT MISO      | 12   | HSPI                               |
| TFT MOSI      | 13   | HSPI                               |
| TFT SCLK      | 14   | HSPI                               |
| TFT CS        | 15   |                                    |
| TFT DC        | 2    |                                    |
| TFT RST       | -1   | Tied to EN pin                     |
| TFT Backlight | 21   | Active HIGH; SPI write at 55 MHz   |
| HUB75 E pin   | 18   | Trinity only, required for 64×64 panels |

## Configuration
- Config file: `/f1_notification_config.json` on SPIFFS
- Key settings:
  - `timeZone` — ezTime-compatible timezone string (default: `Europe/London`)
  - `timeFormat` — ezTime format string (default: `D, H:i`)
  - `botToken` — Telegram bot token from BotFather
  - `chatId` — Telegram chat/user ID
  - `roundOffset` — tracks which race round is current (persisted to detect round changes)
  - `currentRaceNotification` — bool, whether notification has been sent for current race

- SPIFFS runtime files:
  - `/races.json` — full season schedule, fetched from GitHub, overwritten hourly
  - `/current_races.json` — current/next race entry, extracted from races.json
  - `/track.png` — circuit image fetched from Imgur, overwritten when race changes

- Race schedule URL: `https://raw.githubusercontent.com/sportstimes/f1/main/_db/f1/2026.json`
- WiFiManager AP SSID: `f1Thing` / password: `nomikey1` (hardcoded in two places)
- Double-reset within 10 seconds forces config portal
- **Dev override active**: `F1_FORCE_DEV_DEFAULTS` is currently uncommented in `config.h` — SPIFFS timezone and Telegram credentials are always overridden by dev values. Must be commented out before production flashing.

## Current State
Active for the 2026 season on both CYD and Trinity display targets. Core features (schedule display, timezone conversion, Telegram notification, circuit image) are working. `RACE_JSON_URL` updated to 2026.json. Several rough edges remain in notification timing, image mapping, and code hygiene (see Known Issues).

## Architecture Notes
- **Polymorphic display** — `F1Display*` abstract base; `main.ino` is display-agnostic. Adding a new display type only requires implementing four virtual methods: `displaySetup()`, `displayPlaceHolder()`, `displayRaceWeek()`, `drawWifiManagerMessage()`.
- **State tracking on display** — `F1DisplaySate` enum (`unset`, `placeholder`, `raceweek`) + `isSameRace()` prevent unnecessary redraws on each minute tick.
- **Event-driven Telegram** — `ezTime`'s `setEvent()` schedules `sendNotification()` to fire at a computed UTC epoch (6 days before the GP); `events()` is polled each loop iteration.
- **Hourly JSON refresh** — `minuteCounter` is incremented on each `minuteChanged()` tick and triggers re-fetch from GitHub when it reaches 60. It is initialised to `60` so a fetch fires immediately on the first loop iteration (see Known Issues — this causes a double-fetch on startup since `setup()` also calls `fetchRaceJson()`).
- **Two display modes** — `isRaceWeek()` returns true once `UTC.now()` passes `(GP_time - 3 days)`. Before that threshold: `displayPlaceHolder()` shows circuit image + race date. After: `displayRaceWeek()` shows the full session timetable. The threshold `DaysBeforeRace = 3` is a `#define` in `raceLogic.h`.
- **Two separate CA certs** — `githubCert.h` holds the Let's Encrypt R12 intermediate (used for `raw.githubusercontent.com`, expires 2027-03-12). `getImage.h` holds the USERTrust RSA CA (used for `i.imgur.com`, expires 2038). These are now distinct; the previous "duplicate cert" issue is resolved.
- **Global headers** — `getImage.h` and `wifiManagerHandler.h` directly reference globals (`secured_client`, `fileFetcher`, `drd`) defined in the main `.ino`. This works due to Arduino's unified compilation model but creates implicit coupling.

## Known Issues

### Image mapping
- **Hungarian GP image is wrong** — `getImage.h` maps "Hungarian" to the British GP Imgur URL (copy-paste bug; line 97–98).
- **"Barcelona-Catalunya" missing from `getImageUrlForRace()`** — 2026 Spanish GP falls through to the generic default image. `convertRaceName()` correctly abbreviates to "Barcelona" for display, but `getImage.h` has no matching entry for either name.
- **Stale image map entries** — `getImage.h` still has "Spanish" and "Emilia Romagna Grand Prix" entries that no longer exist in the 2026 calendar.

### Notification
- **Notification fires without time-of-day guard** — `getNotifyTime()` subtracts exactly 6 days from GP start time; could fire at 3am local time. The code comments acknowledge this.
- **Telegram notification uses a generic image** — `sendNotificationOfNextRace()` sends a hardcoded generic Imgur PNG rather than the race-specific circuit image.

### WiFi / config
- **WiFiManager AP password hardcoded** — `"nomikey1"` appears in both `wifiManagerHandler.h:70,77` and `cheapYellowLCD.h:207` (displayed on screen during config mode).
- **`F1_FORCE_DEV_DEFAULTS` uncommented** — `config.h:14` always overrides SPIFFS timezone and Telegram credentials with dev defaults. Must be commented out for production build.

### Startup / reliability
- **Double-fetch on startup** — `setup()` calls `fetchRaceJson()`, then `loop()` immediately calls it again because `minuteCounter` is initialised to `60`.
- **Blocking fetch in loop()** — failed `fetchRaceJson()` calls in `loop()` spin in a `while` + 10-second `delay()`, blocking `drd->loop()` and preventing double-reset detection during network failures.
- **`file-fetcher-arduino` unpinned** — no version tag in `platformio.ini`; upstream changes could silently break the build.

### Code hygiene
- **Raw `Serial.println()` in `config.h:107`** — `Serial.println("Timezone: " + timeZone)` bypasses the debug system; should be `DBG_INFO`.
- **Typo: `notificaitonEventRaised`** — variable at `F1-Notifications.ino:286` (and usages) should be `notificationEventRaised`.
- **Typo: `WM_F1_NOTIFCATION_LABEL`** — label at `wifiManagerHandler.h:18` should be `WM_F1_NOTIFICATION_LABEL`.
- **Duplicate certificate** — the USERTrust RSA CA cert is copy-pasted into both `githubCert.h` and `getImage.h`; only one copy needs to exist.
- **Missing include guards** — `cheapYellowLCD.h`, `matrixDisplay.h`, `getImage.h`, `util.h`, `wifiManagerHandler.h`, and `config.h` have no include guards.

### Matrix display
- **Matrix `displayImage()` is a stub** — returns 0 immediately; PNG image display is not implemented for the HUB75 target.
- **Missing `flipDMABuffer()` calls** — `matrixDisplay.h` never calls `dma_display->flipDMABuffer()` after drawing; required by HUB75 DMA library to commit the frame.

## TODO

### Before next production flash
- [ ] Comment out `F1_FORCE_DEV_DEFAULTS` in `config.h` for production build
- [ ] Fix Hungarian GP image URL in `getImage.h` (line 97–98 — copy of British URL)
- [ ] Add "Barcelona-Catalunya" entry to `getImageUrlForRace()` in `getImage.h`
- [ ] Remove stale "Spanish" and "Emilia Romagna Grand Prix" entries from `getImage.h`

### Notification improvements
- [ ] Add time-of-day guard in `getNotifyTime()` to avoid middle-of-night notifications
- [ ] Send race-specific track image in the Telegram notification instead of the generic placeholder

### Reliability
- [ ] Fix double-fetch on startup — initialise `minuteCounter` to `0` (let `setup()` fetch be the first)
- [ ] Replace blocking `while + delay(10s)` retry loop in `loop()` with non-blocking retry via `millis()`
- [ ] Pin `file-fetcher-arduino` to a specific commit or tag in `platformio.ini`

### Code hygiene
- [ ] Replace `Serial.println("Timezone: " + timeZone)` in `config.h:107` with `DBG_INFO`
- [ ] Fix typo `notificaitonEventRaised` → `notificationEventRaised` in `F1-Notifications.ino`
- [ ] Fix typo `WM_F1_NOTIFCATION_LABEL` → `WM_F1_NOTIFICATION_LABEL` in `wifiManagerHandler.h`
- [ ] Add include guards to all headers (`cheapYellowLCD.h`, `matrixDisplay.h`, `getImage.h`, `util.h`, `wifiManagerHandler.h`, `config.h`)
- [ ] Replace hardcoded AP password `"nomikey1"` with a configurable or generated value

### Matrix display
- [ ] Add `flipDMABuffer()` calls after each draw operation in `matrixDisplay.h`
- [ ] Implement or formally stub out PNG image display for the matrix display target

### Done ✓
- [x] Fix SPIFFS/DRD initialisation order — `SPIFFS.begin()` must run before `DoubleResetDetector` init when `ESP_DRD_USE_SPIFFS true`; wrong order caused SPIFFS reformat on every boot, wiping the config file and forcing the WiFi portal on every reboot
- [x] Update `RACE_JSON_URL` in `raceLogic.h` to `2026.json`
- [x] Add dev defaults to `config.h` (`F1_DEV_TIME_ZONE`, `F1_FORCE_DEV_DEFAULTS`)
- [x] Improve serial schedule table output format in `raceLogic.h`
- [x] Elevate PNG rendering debug logging to `DBG_INFO`
- [x] Remove `Serial.println("prts")` debug leftover from `cheapYellowLCD.h`
- [x] Add "Barcelona-Catalunya" to `convertRaceName()` in `util.h`
- [x] Extract debug system to `debug.h`; add `#include "debug.h"` to all headers
- [x] Extract dev credentials to `secrets.h` (gitignored); remove `F1_DEV_*` defines from `config.h`
- [x] Replace `Serial.println("Timezone: " + timeZone)` in `config.h` with `DBG_INFO`
- [x] Fix typo `notificaitonEventRaised` → `notificationEventRaised` in `F1-Notifications.ino`
- [x] Fix typo `WM_F1_NOTIFCATION_LABEL` → `WM_F1_NOTIFICATION_LABEL` in `wifiManagerHandler.h`
- [x] Update `.gitignore` with `secrets.h`, `*.bin`, `*.elf`, `node_modules/`
- [x] Create `CHANGELOG.md`
- [x] Update `githubCert.h` with Let's Encrypt R12 intermediate CA (replaces outdated USERTrust cert); restore `setCACert(github_server_cert)` in both `setup()` and `loop()` in `F1-Notifications.ino`
- [x] Confirm USERTrust cert in `getImage.h` is still correct for Imgur (openssl chain analysis); restore `setCACert(IMGUR_CERTIFICATE_ROOT)` in `getImage.h` (reverts setInsecure workaround)
- [x] Pin `bitbank2/PNGdec` to exactly `1.0.2` in `platformio.ini` (remove `^` caret) — `^1.0.2` was installing v1.1.6 which has: `decode()` propagating last callback return value (1) instead of PNG_SUCCESS, off-by-one row count on RGBA images, and multi-IDAT chunk corruption causing bottom half of image to render as garbage pixels. v1.0.2 has none of these issues. Also reverted `PNGDraw` callback to `void` return and removed all row-counter workarounds introduced in v2.3.4.

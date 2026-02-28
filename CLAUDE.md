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
  - PNGdec ^1.0.2 (CYD only)
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
├── config.h               # F1Config class — SPIFFS JSON config read/write
├── display.h              # Abstract F1Display base class (pure virtual interface) [has include guard]
├── cheapYellowLCD.h       # CYD/ILI9341 concrete display implementation
├── matrixDisplay.h        # HUB75 LED matrix concrete display implementation
├── raceLogic.h            # Race JSON fetch, session parsing, Telegram send, event scheduling [has include guard]
├── getImage.h             # Track image download from Imgur + SPIFFS cache; contains its own copy of USERTrust RSA CA cert
├── wifiManagerHandler.h   # WiFiManager captive portal setup + DoubleResetDetector
├── util.h                 # convertRaceName() — long name abbreviations
├── githubCert.h           # USERTrust RSA Certification Authority cert for raw.githubusercontent.com (updated 21/05/2025; expires 2038)
└── races.h                # UNUSED — legacy 2023 season data as a C string literal, kept for debugging only
```

**Note:** Only `display.h` and `raceLogic.h` have include guards. All other headers rely on single-include discipline.

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

- Race schedule URL: `https://raw.githubusercontent.com/sportstimes/f1/main/_db/f1/2025.json`
- WiFiManager AP SSID: `f1Thing` / password: `nomikey1` (hardcoded in two places)
- Double-reset within 10 seconds forces config portal

## Current State
Functional for the 2025 season on both CYD and Trinity display targets. Core features (schedule display, timezone conversion, Telegram notification, circuit image) are working. Has several rough edges in notification timing, image mapping, and code hygiene (see Known Issues).

## Architecture Notes
- **Polymorphic display** — `F1Display*` abstract base; `main.ino` is display-agnostic. Adding a new display type only requires implementing four virtual methods: `displaySetup()`, `displayPlaceHolder()`, `displayRaceWeek()`, `drawWifiManagerMessage()`.
- **State tracking on display** — `F1DisplaySate` enum (`unset`, `placeholder`, `raceweek`) + `isSameRace()` prevent unnecessary redraws on each minute tick.
- **Event-driven Telegram** — `ezTime`'s `setEvent()` schedules `sendNotification()` to fire at a computed UTC epoch (6 days before the GP); `events()` is polled each loop iteration.
- **Hourly JSON refresh** — `minuteCounter` is incremented on each `minuteChanged()` tick and triggers re-fetch from GitHub when it reaches 60. It is initialised to `60` so a fetch fires immediately on the first loop iteration (see Known Issues — this causes a double-fetch on startup since `setup()` also calls `fetchRaceJson()`).
- **Two display modes** — `isRaceWeek()` returns true once `UTC.now()` passes `(GP_time - 3 days)`. Before that threshold: `displayPlaceHolder()` shows circuit image + race date. After: `displayRaceWeek()` shows the full session timetable. The threshold `DaysBeforeRace = 3` is a `#define` in `raceLogic.h`.
- **Shared root CA** — GitHub (`raw.githubusercontent.com`) and Imgur both use the USERTrust RSA Certification Authority. The same certificate is hardcoded twice: once in `githubCert.h` (as `github_server_cert`) and once in `getImage.h` (as `IMGUR_CERTIFICATE_ROOT`).
- **Global headers** — `getImage.h` and `wifiManagerHandler.h` directly reference globals (`secured_client`, `fileFetcher`, `drd`) defined in the main `.ino`. This works due to Arduino's unified compilation model but creates implicit coupling.

## Known Issues
- **Hungarian GP image is wrong** — `getImage.h` maps "Hungarian" to the British GP Imgur URL (copy-paste bug).
- **Notification fires without time-of-day guard** — `getNotifyTime()` subtracts exactly 6 days from GP start time; could fire at 3am local time. The code comments acknowledge this.
- **WiFiManager AP password hardcoded** — `"nomikey1"` appears in both `wifiManagerHandler.h` and `cheapYellowLCD.h` (displayed on screen during config mode).
- **Telegram notification uses a generic image** — `sendNotificationOfNextRace()` sends a hardcoded generic Imgur PNG rather than the race-specific circuit image.
- **Matrix `displayImage()` is a stub** — returns 0 immediately; PNG image display is not implemented for the HUB75 target.
- **Double-fetch on startup** — `setup()` calls `fetchRaceJson()`, then `loop()` immediately calls it again because `minuteCounter` is initialised to `60`.
- **Blocking fetch in loop()** — failed `fetchRaceJson()` calls in `loop()` spin in a `while` + 10-second `delay()`, blocking `drd->loop()` and preventing double-reset detection during network failures.
- **`file-fetcher-arduino` unpinned** — no version tag in `platformio.ini`; upstream changes could silently break the build.
- **`convertRaceName()` only handles Imola** — other long race names may overflow the CYD display without truncation.
- **Duplicate certificate** — the USERTrust RSA CA cert is copy-pasted into both `githubCert.h` and `getImage.h`; only one copy needs to exist.
- **Missing include guards** — `cheapYellowLCD.h`, `matrixDisplay.h`, `getImage.h`, `util.h`, `wifiManagerHandler.h`, and `config.h` have no include guards.
- **Debug leftover** — `Serial.println("prts")` at `cheapYellowLCD.h:112` inside `displayRaceWeek()`.

## TODO
- [ ] Fix Hungarian GP image URL in `getImage.h`
- [ ] Update `RACE_JSON_URL` in `raceLogic.h` to `2026.json` for the new season
- [ ] Add time-of-day guard in `getNotifyTime()` to avoid middle-of-night notifications
- [ ] Replace hardcoded AP password `"nomikey1"` with a configurable or generated value
- [ ] Send race-specific track image in the Telegram notification instead of the generic placeholder
- [ ] Pin `file-fetcher-arduino` to a specific commit or tag in `platformio.ini`
- [ ] Fix double-fetch on startup (remove `fetchRaceJson()` from `setup()` or initialise `minuteCounter` to `0`)
- [ ] Deduplicate USERTrust RSA CA cert — consolidate into one shared header
- [ ] Add include guards to all headers
- [ ] Remove `Serial.println("prts")` debug leftover in `cheapYellowLCD.h`
- [ ] Expand `convertRaceName()` to handle other long/awkward race names
- [ ] Implement or formally stub out image display for the matrix display target

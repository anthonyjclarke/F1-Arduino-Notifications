# Changelog

This is a personal fork by [Anthony Clarke](https://github.com/anthonyjclarke) of the original project by [Brian Lough (witnessmenow)](https://github.com/witnessmenow/arduino-f1-notifications).

**Changes from v2.2.0 onward are Anthony Clarke's work.** Versions v1.0.0 – v2.1.0 represent the upstream history prior to forking and are included for context only.

Format: `## [version] YYYY-MM-DD` with `### Added / Changed / Fixed` subsections.

---

## [2.3.5] 2026-04-26

### Fixed
- `platformio.ini` — pinned `bitbank2/PNGdec` to exactly `1.0.2` (removed `^` caret). The semver range `^1.0.2` was resolving to v1.1.6, which has multiple bugs: `decode()` propagates the last callback return value instead of returning `PNG_SUCCESS`, stops one row short on RGBA images, and corrupts the decompressed output for PNGs with multiple IDAT chunks (bottom half of image rendered with garbage pixels). v1.0.2 has none of these issues.
- `cheapYellowLCD.h` — reverted `PNGDraw` callback from `int` back to `void` (v1.0.2 API), removed row-counter workarounds (`g_pngRowsDrawn`, `g_lastPNGRow`, +1 row replication), and restored plain `if (rc != PNG_SUCCESS)` success detection — all workarounds introduced in v2.3.4 are superseded by the library downgrade.

---

## [2.3.4] 2026-04-26

### Fixed
- `cheapYellowLCD.h` — PNG image now displays correctly. Two issues fixed:
  1. **`decode()` return value**: PNGdec v1.1.6's `decode(void *pUser, int iOptions)` takes user data as first arg (not a callback). It propagates the last callback return value (1) rather than `PNG_SUCCESS` (0) on success, so `if (rc != PNG_SUCCESS)` always fell through to the schedule screen, overwriting the freshly decoded image after ~79ms. Fixed by counting rows drawn in the callback (`g_pngRowsDrawn`) and checking `g_pngRowsDrawn == expectedRows`.
  2. **Off-by-one (239/240 rows)**: PNGdec v1.1.6 stops one row short on RGBA 8bpp images — it appears to encounter end-of-IDAT / CRC handling before calling the final row's callback. Fixed by caching the last drawn row in `g_lastPNGRow` and replaying it for the missing final row when exactly `expectedRows - 1` rows were drawn.
  3. **`mySeek` return value**: was returning `bool` (0/1) from `myfile.seek()` instead of the actual file position. Fixed to call `seek()` then return `myfile.position()`.

---

## [2.3.3] 2026-04-26

### Fixed
- `githubCert.h` — replaced outdated USERTrust RSA CA cert with Let's Encrypt R12 intermediate CA (valid 2024-03-13 to 2027-03-12, issued by ISRG Root X1). `raw.githubusercontent.com` migrated from USERTrust to Let's Encrypt; the old cert caused `-9984` TLS verification failures. Update before 2027-03-12 using `openssl s_client -connect raw.githubusercontent.com:443 -showcerts` and copying cert #1 (the intermediate).
- `F1-Notifications.ino` — reverted both `setInsecure()` workarounds (setup and loop) back to `setCACert(github_server_cert)` now that `githubCert.h` carries the correct cert.
- `getImage.h` — reverted `setInsecure()` workaround back to `setCACert(IMGUR_CERTIFICATE_ROOT)`; the existing USERTrust cert is still the correct trust anchor for `i.imgur.com` (confirmed via openssl chain analysis).

---

## [2.3.2] 2026-04-26

### Fixed
- GitHub and Imgur TLS connections — `raw.githubusercontent.com` no longer uses the USERTrust RSA CA hardcoded in `githubCert.h`; cert verification now fails at boot, leaving the display blank in a fetch retry loop. Replaced `secured_client.setCACert(github_server_cert)` with `setInsecure()` for both the GitHub race-schedule fetch and the Imgur image fetch (public data, acceptable). Proper root CA cert to be rederived and committed once confirmed (see Known Issues in CLAUDE.md).

---

## [2.3.1] 2026-04-26

### Fixed
- `F1-Notifications.ino` — moved `SPIFFS.begin()` before `DoubleResetDetector` initialisation. `ESP_DRD_USE_SPIFFS true` requires SPIFFS to be mounted before DRD is created; initialising DRD first caused DRD's SPIFFS access to fail and left SPIFFS in a state where `SPIFFS.begin(false)` would fall through to `SPIFFS.begin(true)` on the next boot, reformatting SPIFFS and wiping the saved config file. Symptom: WiFi portal re-opened on every reboot.

---

## [2.3.0] 2026-04-26

### Added
- `debug.h` — leveled debug logging system extracted from main sketch into a standalone header; include this in any file using `DBG_*` macros
- `secrets.h` — gitignored file for dev credentials (Telegram bot token, chat ID, timezone override); never committed

### Changed
- `F1-Notifications.ino` — removed inline debug system; replaced with `#include "debug.h"`
- `config.h` — removed hardcoded `F1_DEV_*` credential defines; class defaults now read from `secrets.h` (`SECRET_BOT_TOKEN`, `SECRET_CHAT_ID`, `SECRET_TIME_ZONE`); added `#include "debug.h"` and `#include "secrets.h"`; added `#pragma once`
- All headers using `DBG_*` macros (`cheapYellowLCD.h`, `matrixDisplay.h`, `raceLogic.h`, `getImage.h`, `wifiManagerHandler.h`) — added `#include "debug.h"` for correct dependency declaration
- `.gitignore` — added `secrets.h`, `*.bin`, `*.elf`, `node_modules/`

### Fixed
- Typo `notificaitonEventRaised` → `notificationEventRaised` in `F1-Notifications.ino`
- Typo `WM_F1_NOTIFCATION_LABEL` → `WM_F1_NOTIFICATION_LABEL` in `wifiManagerHandler.h`
- `config.h` — replaced raw `Serial.println("Timezone: " + timeZone)` with `DBG_INFO` to use the leveled debug system consistently

---

## [2.2.0] 2026-02-28

### Added
- Dev timezone and Telegram credential defaults via `F1_FORCE_DEV_DEFAULTS` in `config.h`
- `convertRaceName()` in `util.h` now handles "Barcelona-Catalunya" → "Barcelona" for 2026 calendar
- F1 logo PROGMEM bitmap (`f1_logo.h`) rendered in schedule screen header
- Alternating slide display: track image ↔ session schedule every 10 seconds (CYD only)
- `F1DisplaySate` enum (`unset`, `placeholder`, `raceweek`) with `isSameRace()` for redraw suppression
- Formatted serial schedule output with Unicode box drawing in `raceLogic.h`
- `getImage.h` — added Chinese GP Imgur URL

### Changed
- `RACE_JSON_URL` updated to `2026.json` for the 2026 F1 season
- PNG rendering debug logging elevated to `DBG_INFO` level
- Schedule table column layout updated to fit "Sprint Qualifying" (longest session name)

### Fixed
- `Serial.println("prts")` debug leftover removed from `cheapYellowLCD.h`
- PNGdec v1.1.6 callback signature — `PNGDraw` now returns `int` (must return non-zero to continue decoding)
- PNG byte order confirmed correct: `PNG_RGB565_BIG_ENDIAN` + `u32Bkgd = 0x00FFFFFF` (white alpha composite)

---

<!-- ============================================================ -->
<!-- Upstream history — Brian Lough (witnessmenow)                -->
<!-- Changes below this line are NOT Anthony Clarke's work.       -->
<!-- Source: github.com/witnessmenow/arduino-f1-notifications     -->
<!-- ============================================================ -->

## [2.1.0] 2025-05-22

### Added
- `DaysBeforeRace` configurable `#define` in `raceLogic.h` (default 3 days)
- USERTrust RSA CA certificate updated in `githubCert.h` and `getImage.h` (expires 2038)

### Changed
- Race schedule URL updated to `2025.json`
- Added missing 2025 circuit images to `getImageUrlForRace()`

---

## [2.0.0] 2024-05-06

### Added
- Polymorphic display system: `F1Display` abstract base class supporting CYD ILI9341 and ESP32 Trinity HUB75 64×64 LED matrix
- WiFiManager captive portal for timezone, Telegram bot token, and chat ID configuration
- SPIFFS JSON config persistence (`/f1_notification_config.json`)
- ESP_DoubleResetDetector: double-reset within 10 s forces config portal
- Circuit track image download from Imgur, cached to `/track.png` on SPIFFS
- Leveled debug logging: `DBG_ERROR` / `DBG_WARN` / `DBG_INFO` / `DBG_VERBOSE` with compile-time `DEBUG_LEVEL`
- Telegram notification rescheduled on send failure
- `util.h` — `convertRaceName()` for long race name abbreviations (Imola)

### Changed
- Updated race schedule to 2024 season

### Fixed
- Matrix display `getTextBounds` + right-align time text
- PNG rendering byte order for TFT_eSPI (`PNG_RGB565_BIG_ENDIAN`)

---

## [1.0.0] 2023-07-04 — upstream origin

**Original project by Brian Lough (witnessmenow).**
Source: https://github.com/witnessmenow/arduino-f1-notifications

- Race schedule JSON fetched from GitHub hourly
- Session times displayed in local timezone via ezTime
- Telegram notification 6 days before each race weekend
- CYD ILI9341 TFT display support

/*******************************************************************
    F1 Arduino Notifications

    Displays upcoming F1 session times in your local timezone on
    either a Cheap Yellow Display (CYD / ILI9341 TFT) or an
    ESP32 Trinity HUB75 LED matrix panel.

    Original work by Brian Lough (witnessmenow)
    https://github.com/witnessmenow/arduino-f1-notifications
    YouTube: https://www.youtube.com/brianlough

    If you find the original work useful, consider sponsoring:
    https://github.com/sponsors/witnessmenow/

    Tested on ESP32 (esp32dev).

    --- Changes from original ---
    - Dual display support via F1Display polymorphic base class
      (CYD ILI9341 TFT and HUB75 64x64 LED matrix)
    - WiFiManager captive portal for timezone and Telegram
      configuration, persisted to SPIFFS as JSON
    - Double-reset-to-config-portal (ESP_DoubleResetDetector)
    - Circuit track image fetched from Imgur, cached to SPIFFS
    - Race week detection: switches between circuit image and
      full session timetable N days before the GP
    - Leveled debug logging (DBG_ERROR / WARN / INFO / VERBOSE)
    - Formatted serial output table for race schedule
    - Telegram notification rescheduled on send failure
    - Fixed PNG rendering byte order for TFT_eSPI (RGB565 big-endian)
 *******************************************************************/
// ----------------------------
// Display type
// ----------------------------

// This project currently supports the following displays
// (Uncomment the required #define)

// 1. Cheap yellow display (Using TFT-eSPI library)
// #define YELLOW_DISPLAY

// 2. Matrix Displays (Like the ESP32 Trinity)
// #define MATRIX_DISPLAY

// If no defines are set, it will default to CYD
#if !defined(YELLOW_DISPLAY) && !defined(MATRIX_DISPLAY)
#define YELLOW_DISPLAY // Default to Yellow Display for display type
#endif

// ----------------------------
// Library Defines - Need to be defined before library import
// ----------------------------

#define ESP_DRD_USE_SPIFFS true

// ----------------------------
// Debug system — levels, macros, runtime control
// ----------------------------
#include "debug.h"

// ----------------------------
// Standard Libraries
// ----------------------------

#include <WiFi.h>

#include <WiFiClientSecure.h>

#include <FS.h>
#include "SPIFFS.h"

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <WiFiManager.h>
// Captive portal for configuring the WiFi

// If installing from the library manager (Search for "WifiManager")
// https://github.com/tzapu/WiFiManager

#include <ESP_DoubleResetDetector.h>
// A library for checking if the reset button has been pressed twice
// Can be used to enable config mode
// Can be installed from the library manager (Search for "ESP_DoubleResetDetector")
// https://github.com/khoih-prog/ESP_DoubleResetDetector

#include <ArduinoJson.h>
// Library used for parsing Json from the API responses

// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

#include <ezTime.h>
// Library used for getting the time and converting session time
// to users timezone

// Search for "ezTime" in the Arduino Library manager
// https://github.com/ropg/ezTime

#include <UniversalTelegramBot.h>
// Library used to send Telegram Message

// Search for "Universal Telegram" in the Arduino Library manager
// https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot

#include <FileFetcher.h>
// Library used to get files or images

// Not on library manager yet
// https://github.com/witnessmenow/file-fetcher-arduino

// ----------------------------
// Internal includes
// ----------------------------

#include "githubCert.h"

#include "display.h"

#include "config.h"

#include "raceLogic.h"

#include "wifiManagerHandler.h"

#include "f1_logo.h"


WiFiClientSecure secured_client;

FileFetcher fileFetcher(secured_client);

// ----------------------------
// Display Handling Code
// ----------------------------

#if defined YELLOW_DISPLAY

#include "cheapYellowLCD.h"
CheapYellowDisplay cyd;
F1Display *f1Display = &cyd;

#elif defined MATRIX_DISPLAY

#include "matrixDisplay.h"
MatrixDisplay matrixDisplay;
F1Display *f1Display = &matrixDisplay;

#endif
// ----------------------------

UniversalTelegramBot bot("", secured_client);

F1Config f1Config;

void setup()
{
  // put your setup code here, to run once:

  Serial.begin(115200);
  DBG_INFO("Boot started");
  DBG_INFO("Debug level set to %u", debugLevel);

  f1Display->displaySetup();
  DBG_INFO("Display setup complete");

  bool forceConfig = false;

  // Mount SPIFFS before DRD — ESP_DRD_USE_SPIFFS requires SPIFFS to be
  // mounted first or DRD's SPIFFS access fails and leaves SPIFFS in an
  // inconsistent state, causing SPIFFS.begin(true) to reformat on every boot
  // and wipe the saved config file.
  bool spiffsInitSuccess = SPIFFS.begin(false) || SPIFFS.begin(true);
  if (!spiffsInitSuccess)
  {
    DBG_ERROR("SPIFFS initialization failed");
    while (1)
      yield();
  }
  DBG_INFO("SPIFFS initialization done");

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset())
  {
    DBG_WARN("Forcing config mode due to double reset");
    forceConfig = true;
  }

  if (!f1Config.fetchConfigFile())
  {
    // Failed to fetch config file, need to launch Wifi Manager
    DBG_WARN("Config file load failed, forcing WiFiManager config portal");
    forceConfig = true;
  }

  setupWiFiManager(forceConfig, f1Config, f1Display);
  DBG_INFO("WiFiManager setup complete");
  raceLogicSetup(f1Config);
  bot.updateToken(f1Config.botToken);
  DBG_INFO("Race logic initialized");

  while (WiFi.status() != WL_CONNECTED)
  {
    DBG_VERBOSE("Waiting for WiFi connection...");
    delay(500);
  }

  DBG_INFO("WiFi connected");
  DBG_INFO("IP address: %s", WiFi.localIP().toString().c_str());

  secured_client.setCACert(github_server_cert);
  DBG_INFO("Fetching races.json from remote source");
  while (fetchRaceJson(fileFetcher) != 1)
  {
    DBG_WARN("Failed to fetch races.json, retrying in 10 seconds");
    delay(1000 * 10);
  }

  DBG_INFO("Fetched races.json successfully");

  DBG_INFO("Waiting for time sync");

  waitForSync();

  DBG_INFO("Time sync complete");
  DBG_INFO("UTC: %s", UTC.dateTime().c_str());

  myTZ.setLocation(f1Config.timeZone);
  DBG_INFO("%s: %s", f1Config.timeZone.c_str(), myTZ.dateTime().c_str());
  DBG_INFO("Setup complete");

  // sendNotificationOfNextRace(&bot, f1Config.roundOffset);
}

bool notificationEventRaised = false;

void sendNotification()
{
  // Cause it could be set to the image one
  if (f1Config.isTelegramConfigured())
  {
    secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    DBG_INFO("Sending Telegram notification");
    f1Config.currentRaceNotification = sendNotificationOfNextRace(&bot);
    if (!f1Config.currentRaceNotification)
    {
      // Notificaiton failed, raise event again
      DBG_WARN("Notification failed, event will be rescheduled");
      setEvent(sendNotification, getNotifyTime());
    }
    else
    {
      notificationEventRaised = false;
      DBG_INFO("Notification sent successfully");
      f1Config.saveConfigFile();
    }
  }
  else
  {

    DBG_WARN("Notification skipped, Telegram is not configured");

    notificationEventRaised = false;
    f1Config.currentRaceNotification = true;
    f1Config.saveConfigFile();
  }
}

bool first = true;

int minuteCounter = 60; // kick off fetch first time

void loop()
{
  drd->loop();

  // Every hour we will refresh the Race JSON from Github
  if (minuteCounter >= 60)
  {
    secured_client.setCACert(github_server_cert);
    DBG_INFO("Refreshing races.json");
    while (fetchRaceJson(fileFetcher) != 1)
    {
      DBG_WARN("Failed to refresh races.json, retrying in 10 seconds");
      delay(1000 * 10);
    }
    minuteCounter = 0;
  }

  if (first || minuteChanged())
  {
    minuteCounter++;
    bool newRace = getNextRace(f1Config.roundOffset, f1Config.currentRaceNotification, f1Display, first);
    if (newRace)
    {
      f1Config.saveConfigFile();
    }
    if (!f1Config.currentRaceNotification && !notificationEventRaised)
    {
      // we have never notified about this race yet, so we'll raise an event
      setEvent(sendNotification, getNotifyTime());
      notificationEventRaised = true;
      DBG_INFO("Notification event raised for: %s", myTZ.dateTime(getNotifyTime(), UTC_TIME, f1Config.timeFormat).c_str());
    }
    first = false;
  }

  events();
  f1Display->tickDisplay();
}

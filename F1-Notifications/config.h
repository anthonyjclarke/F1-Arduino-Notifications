// ----------------------------
// config.h
// F1Config class — reads and writes /f1_notification_config.json on SPIFFS.
// ----------------------------

#pragma once

#include "debug.h"
#include "secrets.h"

#define F1_CONFIG_JSON "/f1_notification_config.json"

// Uncomment to force dev defaults from secrets.h, overriding any values saved in SPIFFS config.
// Must be commented out for production flashing.
#define F1_FORCE_DEV_DEFAULTS

// Format for the race date header in the serial schedule table (ezTime format string)
#define F1_RACE_DATE_FORMAT "l, d M Y - H:i"

#define F1_TIME_ZONE_LABEL "timeZone"
#define F1_TIME_FORMAT_LABEL "timeFormat"
#define F1_BOT_TOKEN_LABEL "botToken"
#define F1_CHAT_ID_LABEL "chatId"
#define F1_ROUND_OFFSET_LABEL "roundOffset"
#define F1_CURRENT_RACE_NOTIFICATION_LABEL "currentRaceNotification"

class F1Config
{
public:
  // How the time will be displayed, see here for more info: https://github.com/ropg/ezTime#datetime
  String timeFormat = "D, H:i";      // Fri, 00:30
  String timeZone = SECRET_TIME_ZONE;

  // Telegram BOT Token (Get from Botfather)
  String botToken = SECRET_BOT_TOKEN;

  // Use @myidbot (IDBot) to find out the chat ID of an individual or a group
  // Also note that you need to click "start" on a bot before it can
  // message you
  String chatId = SECRET_CHAT_ID;

  int roundOffset = 0;

  bool currentRaceNotification = false;

  bool isTelegramConfigured()
  {
    return (botToken != "") && (chatId != "");
  }

  bool fetchConfigFile()
  {
    if (SPIFFS.exists(F1_CONFIG_JSON))
    {
      DBG_INFO("Reading config file");
      File configFile = SPIFFS.open(F1_CONFIG_JSON, "r");
      if (configFile)
      {
        DBG_VERBOSE("Opened config file");
        StaticJsonDocument<1024> json;
        DeserializationError error = deserializeJson(json, configFile);
        if (debugLevel >= DBG_LEVEL_VERBOSE)
        {
          serializeJsonPretty(json, Serial);
          Serial.println();
        }
        if (!error)
        {
          DBG_INFO("Parsed config json");

          if (json.containsKey(F1_TIME_ZONE_LABEL))
          {
            timeZone = String(json[F1_TIME_ZONE_LABEL].as<String>());
          }

          if (json.containsKey(F1_TIME_FORMAT_LABEL))
          {
            timeFormat = String(json[F1_TIME_FORMAT_LABEL].as<String>());
          }

          if (json.containsKey(F1_BOT_TOKEN_LABEL))
          {
            botToken = String(json[F1_BOT_TOKEN_LABEL].as<String>());
          }

          if (json.containsKey(F1_CHAT_ID_LABEL))
          {
            chatId = String(json[F1_CHAT_ID_LABEL].as<String>());
          }

          if (json.containsKey(F1_ROUND_OFFSET_LABEL))
          {
            roundOffset = json[F1_ROUND_OFFSET_LABEL].as<int>();
          }

          if (json.containsKey(F1_CURRENT_RACE_NOTIFICATION_LABEL))
          {
            currentRaceNotification = json[F1_CURRENT_RACE_NOTIFICATION_LABEL].as<bool>();
          }

#ifdef F1_FORCE_DEV_DEFAULTS
          timeZone = SECRET_TIME_ZONE;
          botToken = SECRET_BOT_TOKEN;
          chatId   = SECRET_CHAT_ID;
          DBG_WARN("Dev defaults forced — SPIFFS timezone/credentials overridden");
#endif

          DBG_INFO("Timezone: %s", timeZone.c_str());
          return true;
        }
        else
        {
          DBG_ERROR("Failed to parse config json");
          return false;
        }
      }
    }

    DBG_WARN("Config file does not exist");
    return false;
  }

  bool saveConfigFile()
  {
    DBG_INFO("Saving config");
    StaticJsonDocument<1024> json;
    json[F1_TIME_ZONE_LABEL] = timeZone;
    json[F1_TIME_FORMAT_LABEL] = timeFormat;
    json[F1_BOT_TOKEN_LABEL] = botToken;
    json[F1_CHAT_ID_LABEL] = chatId;
    json[F1_ROUND_OFFSET_LABEL] = roundOffset;
    json[F1_CURRENT_RACE_NOTIFICATION_LABEL] = currentRaceNotification;

    File configFile = SPIFFS.open(F1_CONFIG_JSON, "w");
    if (!configFile)
    {
      DBG_ERROR("Failed to open config file for writing");
      return false;
    }

    if (debugLevel >= DBG_LEVEL_VERBOSE)
    {
      serializeJsonPretty(json, Serial);
      Serial.println();
    }
    if (serializeJson(json, configFile) == 0)
    {
      DBG_ERROR("Failed to write config file");
      return false;
    }
    configFile.close();
    return true;
  }
};

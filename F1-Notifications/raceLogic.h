// ----------------------------
// raceLogic.h
// Race schedule fetch, session parsing, display routing, and Telegram dispatch.
// ----------------------------

#ifndef RACELOGIC_H
#define RACELOGIC_H

#define RACE_FILE_NAME "/races.json"
#define CURRENT_RACE_FILE_NAME "/current_races.json"

// path to the races schedule, needs to be updated each year
#define RACE_JSON_URL "https://raw.githubusercontent.com/sportstimes/f1/main/_db/f1/2026.json"
// Number of days before the race to display circuit image rather than sessions schedule
#define DaysBeforeRace 3

time_t nextRaceStartUtc;

Timezone myTZ;
F1Config rl_f1Config;

void raceLogicSetup(F1Config f1Config)
{
  rl_f1Config = f1Config;
}

bool isSessionInFuture(const char *sessionStartTime)
{
  struct tm tm = {0};
  // Parse date from UTC and convert to an epoch
  strptime(sessionStartTime, "%Y-%m-%dT%H:%M:%S", &tm);
  time_t sessionEpoch = mktime(&tm);

  return UTC.now() < sessionEpoch;
}

bool isRaceWeek(const char *sessionStartTime)
{
  struct tm tm = {0};
  // Parse date from UTC and convert to an epoch
  strptime(sessionStartTime, "%Y-%m-%dT%H:%M:%S", &tm);

  time_t DaysBeforeRaceEpoch = mktime(&tm) - (DaysBeforeRace * SECS_PER_DAY); 
  return UTC.now() > DaysBeforeRaceEpoch;
}

String getConvertedTime(const char *sessionStartTime, const char *timeFormat = "")
{
  struct tm tm = {0};
  // Parse date from UTC and convert to an epoch
  strptime(sessionStartTime, "%Y-%m-%dT%H:%M:%S", &tm);
  time_t sessionEpoch = mktime(&tm);

  String timeFormatStr = rl_f1Config.timeFormat;
  if (timeFormat[0] != 0)
  {
    timeFormatStr = String(timeFormat);
  }
  return myTZ.dateTime(sessionEpoch, UTC_TIME, timeFormatStr);
}

void printConvertedTime(const char *sessionName, const char *sessionStartTime)
{

  String timeStr = getConvertedTime(sessionStartTime, "");
  DBG_INFO("%s: %s", sessionName, timeStr.c_str());
}

const char *sessionCodeToString(const char *sessionCode)
{
  if (strcmp(sessionCode, "fp1") == 0)
  {
    return "FP1: ";
  }
  else if (strcmp(sessionCode, "fp2") == 0)
  {
    return "FP2: ";
  }
  else if (strcmp(sessionCode, "fp3") == 0)
  {
    return "FP3: ";
  }
  else if (strcmp(sessionCode, "qualifying") == 0)
  {
    return "Qualifying: ";
  }
  else if (strcmp(sessionCode, "sprint") == 0)
  {
    return "Sprint: ";
  }
  else if (strcmp(sessionCode, "sprintQualifying") == 0)
  {
    return "Sprint Quali: ";
  }
  else if (strcmp(sessionCode, "gp") == 0)
  {
    return "Race: ";
  }

  return "UNKNOWN";
}

// Full display names for box output, max 17 chars to fit session column
const char *sessionCodeToFullString(const char *sessionCode)
{
  if (strcmp(sessionCode, "fp1") == 0)            return "Free Practice 1";
  if (strcmp(sessionCode, "fp2") == 0)            return "Free Practice 2";
  if (strcmp(sessionCode, "fp3") == 0)            return "Free Practice 3";
  if (strcmp(sessionCode, "qualifying") == 0)     return "Qualifying";
  if (strcmp(sessionCode, "sprint") == 0)         return "Sprint";
  if (strcmp(sessionCode, "sprintQualifying") == 0) return "Sprint Qualifying";
  if (strcmp(sessionCode, "gp") == 0)             return "Race";
  return "Unknown";
}

void printRaceTimes(const char *raceName, JsonObject races_sessions)
{
  if (debugLevel < DBG_LEVEL_INFO) return;

  // Box: 41 chars wide (39 inner)
  // Columns (inner widths): Session=21, Day=8, Time=8
  // "Sprint Qualifying" (17 chars) is the longest session name: 2+17+2 = 21 col ✓
  String nowStr = myTZ.dateTime("Y-m-d D H:i");

  Serial.println(F("┌───────────────────────────────────────┐"));
  Serial.println(F("│           F1 RACE SCHEDULE            │"));
  Serial.printf(   "│  %-37s│\n", nowStr.c_str());
  Serial.println(F("├───────────────────────────────────────┤"));
  Serial.printf(   "│  Next Race: %-26s│\n", raceName);
  Serial.println(F("├─────────────────────┬────────┬────────┤"));
  Serial.println(F("│  SESSION            │  DAY   │  TIME  │"));
  Serial.println(F("├─────────────────────┼────────┼────────┤"));

  for (JsonPair kv : races_sessions)
  {
    const char *sessionName = sessionCodeToFullString(kv.key().c_str());
    String dayStr  = getConvertedTime(kv.value().as<const char *>(), "D");
    String timeStr = getConvertedTime(kv.value().as<const char *>(), "H:i");
    Serial.printf("│  %-19s│  %-6s│  %-6s│\n",
                  sessionName, dayStr.c_str(), timeStr.c_str());
  }

  Serial.println(F("└─────────────────────┴────────┴────────┘"));
}

String createTelegramMessageString(const char *raceName, JsonObject races_sessions)
{
  String message = "Next Race: ";
  message += raceName;
  message += "\n";
  message += "---------------------\n";

  for (JsonPair kv : races_sessions)
  {
    String sessionName = String(sessionCodeToString(kv.key().c_str()));
    message += sessionName;
    message += getConvertedTime(kv.value().as<const char *>(), "");
    message += "\n";
  }
  return message;
}

bool sendNotificationOfNextRace(UniversalTelegramBot *bot)
{

  StaticJsonDocument<112> filter;
  filter["name"] = true;
  filter["location"] = true;
  filter["round"] = true;
  filter["sessions"] = true;

  File racesJson = SPIFFS.open(CURRENT_RACE_FILE_NAME);
  DynamicJsonDocument race(1000);

  DeserializationError error = deserializeJson(race, racesJson, DeserializationOption::Filter(filter));

  if (error)
  {
    DBG_ERROR("deserializeJson() failed: %s", error.c_str());
    racesJson.close();
    return false;
  }

  const char *races_name = race["name"];
  JsonObject races_sessions = race["sessions"];

  printRaceTimes(races_name, races_sessions);

  DBG_INFO("Sending message to %s", rl_f1Config.chatId.c_str());
  racesJson.close();
  return bot->sendPhoto(rl_f1Config.chatId, "https://i.imgur.com/q3qsfSi.png", createTelegramMessageString(races_name, races_sessions));
}

int fetchRaceJson(FileFetcher fileFetcher)
{
  if (SPIFFS.exists(RACE_FILE_NAME) == true)
  {
    DBG_VERBOSE("Removing existing races.json");
    SPIFFS.remove(RACE_FILE_NAME);
  }

  fs::File f = SPIFFS.open(RACE_FILE_NAME, "w+");
  if (!f)
  {
    DBG_ERROR("Opening races.json file for write failed");
    return -1;
  }

  bool gotFile = fileFetcher.getFile(RACE_JSON_URL, &f);

  f.close();

  return gotFile;
}

bool saveCurrentRaceToFile(const JsonObject &raceJson)
{

  if (raceJson.isNull())
  {
    DBG_WARN("Race data is null, nothing to save");
    return false;
  }

  File currentRaceFile = SPIFFS.open(CURRENT_RACE_FILE_NAME, "w");
  if (!currentRaceFile)
  {
    DBG_ERROR("Failed to open current race file for writing");
    return false;
  }

  DBG_INFO("Saving current race json");
  if (debugLevel >= DBG_LEVEL_VERBOSE)
  {
    serializeJsonPretty(raceJson, Serial);
    Serial.println();
  }
  if (serializeJson(raceJson, currentRaceFile) == 0)
  {
    DBG_ERROR("Failed to write current race file");
    return false;
  }
  currentRaceFile.close();
  return true;
}

bool getNextRace(int &offset, bool &notificationSent, F1Display *f1Display, bool forceRaceFileSave)
{

  StaticJsonDocument<112> filter;

  JsonObject filter_races_0 = filter["races"].createNestedObject();
  filter_races_0["name"] = true;
  filter_races_0["location"] = true;
  filter_races_0["round"] = true;
  filter_races_0["sessions"] = true;
  filter_races_0["canceled"] = true;

  File racesJson = SPIFFS.open(RACE_FILE_NAME);
  DynamicJsonDocument doc(12288);

  DeserializationError error = deserializeJson(doc, racesJson, DeserializationOption::Filter(filter));

  if (error)
  {
    DBG_ERROR("deserializeJson() failed: %s", error.c_str());
    racesJson.close();
    return false;
  }
  JsonArray races = doc["races"];

  int racesAmount = races.size();
  time_t timeNow = UTC.now();
  DBG_VERBOSE("UTC: %s", UTC.dateTime().c_str());
  for (int i = 0; i < racesAmount; i++)
  {

    const char *races_name = races[i]["name"];
    JsonObject races_sessions = races[i]["sessions"];
    const char *race_sessions_gp = races_sessions["gp"]; // "2023-03-05T15:00:00Z"
    bool raceCanceled = races[i]["canceled"].as<bool>();

    struct tm tm = {0};

    // Convert to tm struct
    // Sample format: 2023-03-17T13:30:00Z
    strptime(race_sessions_gp, "%Y-%m-%dT%H:%M:%S", &tm);

    nextRaceStartUtc = mktime(&tm);
    if (!raceCanceled && timeNow < nextRaceStartUtc)
    {
      bool newRace = false;
      int roundNumber = races[i]["round"];
      if (roundNumber != offset)
      {
        if (saveCurrentRaceToFile(races[i]))
        {
          offset = roundNumber;
          notificationSent = false;
          DBG_INFO("Detected new race (round %d)", roundNumber);
          newRace = true;
        }
        else
        {
          DBG_ERROR("Detected new race but failed to save json");
        }
      }
      else
      {
        DBG_VERBOSE("Same race as before (round %d)", roundNumber);
        if (forceRaceFileSave)
        {
          if (saveCurrentRaceToFile(races[i]))
          {
            DBG_INFO("(Forced Save) Saved race to file");
          }
          else
          {
            DBG_ERROR("(Forced Save) Failed to save race file");
          }
        }
      }

      if (isRaceWeek(race_sessions_gp))
      {
        f1Display->displayRaceWeek(races_name, races_sessions);
      }
      else
      {
        f1Display->displayPlaceHolder(races_name, races_sessions);
      }

      printRaceTimes(races_name, races_sessions);
      racesJson.close();
      return newRace;
    }
  }
  racesJson.close();
  return false;
}

time_t getNotifyTime()
{

  time_t t = nextRaceStartUtc - (6 * SECS_PER_DAY);
  // Probably should make this smarter so it's not sending notifications in the middle of the night!
  return t;
}

#endif

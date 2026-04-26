// ----------------------------
// cheapYellowLCD.h
// CYD (ILI9341 / TFT_eSPI) concrete display implementation.
// ----------------------------

#include "debug.h"
#include "display.h"
#include "getImage.h"
#include "util.h"

#include "f1_logo.h"

#include <TFT_eSPI.h>
// A library for interfacing with LCD displays
//
// Can be installed from the library manager (Search for "TFT_eSPI")
// https://github.com/Bodmer/TFT_eSPI

#include <PNGdec.h>
// For decoding png files
//
// Can be installed from the library manager (Search for "PNGdec")
// https://github.com/bitbank2/PNGdec

// ----------------------------
// PNGdec callbacks — must be free functions, not class members
// ----------------------------

#define SESSION_TEXT_SIZE 4

// Alternating slide interval (milliseconds)
#define SLIDE_INTERVAL_MS    10000UL

// Schedule screen layout constants (320×240 landscape)
#define SCHED_DIVIDER_Y      74    // y of red divider between header and table
#define SCHED_HDR_Y          76    // y of table header row
#define SCHED_HDR_H          20    // height of table header row
#define SCHED_ROWS_START_Y   96    // y of first session row
#define SCHED_ROW_H          20    // height of each session row
#define SCHED_COL_SESSION    10    // x of session name text
#define SCHED_COL_DAY        208   // x of day text
#define SCHED_COL_TIME       264   // x of time text
#define SCHED_MAX_SESSIONS   8     // max sessions to cache

TFT_eSPI tft = TFT_eSPI();
PNG png;

fs::File myfile;

void *myOpen(const char *filename, int32_t *size)
{
  myfile = SPIFFS.open(filename);
  *size = myfile.size();
  DBG_INFO("PNG myOpen: file=%s size=%d bytes", filename, *size);
  return &myfile;
}
void myClose(void *handle)
{
  if (myfile)
    myfile.close();
}
int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length)
{
  if (!myfile)
    return 0;
  return myfile.read(buffer, length);
}
int32_t mySeek(PNGFILE *handle, int32_t position)
{
  if (!myfile)
    return 0;
  myfile.seek(position);
  return (int32_t)myfile.position();
}

void PNGDraw(PNGDRAW *pDraw)
{
  uint16_t usPixels[320];
  // u32Bkgd: 0x00FFFFFF = white background in 0x00BBGGRR format.
  // Enables proper alpha blending so transparent areas appear white, not black.
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, 0x00FFFFFF);
  tft.pushImage(0, pDraw->y, pDraw->iWidth, 1, usPixels);
}

struct SessionRow
{
  char name[20];
  char day[6];
  char time[7];
  bool future;
};

class CheapYellowDisplay : public F1Display
{
public:
  void displaySetup()
  {
    DBG_INFO("CYD display setup");
    setWidth(320);
    setHeight(240);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    _lastSlideChange = 0;
    _showingImage = false;
    _hasData = false;
    _sessionCount = 0;
    _nextSessionIdx = -1;

    state = unset;
  }

  void displayPlaceHolder(const char *raceName, JsonObject races_sessions)
  {
    // Re-cache every call so future/past session status stays current
    cacheSessionData(raceName, races_sessions);

    if (isSameRace(raceName) && state == placeholder)
    {
      // Content unchanged — tickDisplay() handles alternation
      DBG_VERBOSE("No display update needed");
      return;
    }

    setRaceName(raceName);
    _lastSlideChange = millis();

    int imageFileStatus = getImage(raceName);
    if (imageFileStatus)
    {
      int imageDisplayStatus = displayImage(TRACK_IMAGE);
      if (imageDisplayStatus == PNG_SUCCESS)
      {
        drawImageOverlay();
        _showingImage = true;
        state = placeholder;
        return;
      }
    }

    // Image unavailable — fall back to schedule screen
    drawScheduleScreen();
    _showingImage = false;
    state = placeholder;
  }

  void displayRaceWeek(const char *raceName, JsonObject races_sessions)
  {
    // Re-cache every call so future/past session status stays current
    cacheSessionData(raceName, races_sessions);

    if (isSameRace(raceName) && state == raceweek)
    {
      // Content unchanged — tickDisplay() handles alternation
      return;
    }

    DBG_INFO("Rendering race week display for %s", raceName);
    setRaceName(raceName);
    _lastSlideChange = millis();

    drawScheduleScreen();
    _showingImage = false;
    state = raceweek;
  }

  int displayImage(char *imageFileUri)
  {
    tft.fillScreen(TFT_BLACK);
    unsigned long lTime = millis();
    DBG_INFO("Displaying image: %s", imageFileUri);

    int rc = png.open((const char *)imageFileUri, myOpen, myClose, myRead, mySeek, PNGDraw);
    if (rc == PNG_SUCCESS)
    {
      DBG_INFO("Image specs: (%d x %d), %d bpp, pixel type: %d", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
      rc = png.decode(NULL, 0);
      png.close();
      if (rc != PNG_SUCCESS)
      {
        DBG_ERROR("PNG decode failed with error code: %d", rc);
      }
      else
      {
        DBG_INFO("PNG decode OK");
      }
    }
    else
    {
      DBG_ERROR("PNG open failed with error code: %d", rc);
    }

    DBG_INFO("Image decode time (ms): %lu", millis() - lTime);

    return rc;
  }

  void drawWifiManagerMessage(WiFiManager *myWiFiManager)
  {
    DBG_WARN("Display entered config mode");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Entered Conf Mode:", screenCenterX, 5, 2);
    tft.drawString("Connect to the following WIFI AP:", 5, 28, 2);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString(myWiFiManager->getConfigPortalSSID(), 20, 48, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Password:", 5, 64, 2);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString("nomikey1", 20, 82, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.drawString("If it doesn't AutoConnect, use this IP:", 5, 110, 2);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString(WiFi.softAPIP().toString(), 20, 128, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }

  // Called each loop() — switches between track image and schedule screen every SLIDE_INTERVAL_MS.
  void tickDisplay()
  {
    if (!_hasData) return;
    if (millis() - _lastSlideChange < SLIDE_INTERVAL_MS) return;

    _lastSlideChange = millis();
    _showingImage = !_showingImage;

    if (_showingImage)
    {
      if (SPIFFS.exists(TRACK_IMAGE) && displayImage(TRACK_IMAGE) == PNG_SUCCESS)
      {
        drawImageOverlay();
      }
      else
      {
        // Image unavailable — stay on schedule
        drawScheduleScreen();
        _showingImage = false;
      }
    }
    else
    {
      drawScheduleScreen();
    }
  }

private:
  unsigned long _lastSlideChange;
  bool _showingImage;
  bool _hasData;
  SessionRow _sessions[SCHED_MAX_SESSIONS];
  int _sessionCount;
  int _nextSessionIdx;
  char _schedRaceName[50];
  char _schedDateStr[30];
  char _imageOverlayStr[50];

  // Populate cached session data used by drawScheduleScreen() and drawImageOverlay().
  void cacheSessionData(const char *raceName, JsonObject races_sessions)
  {
    _sessionCount = 0;
    _nextSessionIdx = -1;

    strlcpy(_schedRaceName, convertRaceName(raceName), sizeof(_schedRaceName));

    String dateStr = getConvertedTime(races_sessions["gp"], "D, j M Y");
    strlcpy(_schedDateStr, dateStr.c_str(), sizeof(_schedDateStr));

    String gpDateShort = getConvertedTime(races_sessions["gp"], "M d");
    String overlayStr = String(convertRaceName(raceName)) + " | " + gpDateShort;
    strlcpy(_imageOverlayStr, overlayStr.c_str(), sizeof(_imageOverlayStr));

    for (JsonPair kv : races_sessions)
    {
      if (_sessionCount >= SCHED_MAX_SESSIONS) break;
      SessionRow &row = _sessions[_sessionCount];
      strlcpy(row.name, sessionCodeToFullString(kv.key().c_str()), sizeof(row.name));
      String dayStr  = getConvertedTime(kv.value().as<const char *>(), "D");
      String timeStr = getConvertedTime(kv.value().as<const char *>(), "H:i");
      strlcpy(row.day,  dayStr.c_str(),  sizeof(row.day));
      strlcpy(row.time, timeStr.c_str(), sizeof(row.time));
      row.future = isSessionInFuture(kv.value().as<const char *>());
      if (row.future && _nextSessionIdx == -1)
      {
        _nextSessionIdx = _sessionCount;
      }
      _sessionCount++;
    }

    _hasData = true;
  }

  // Draw the F1 logo PROGMEM bitmap — requires swap bytes toggle around pushImage.
  void drawF1Logo(int x, int y)
  {
    tft.setSwapBytes(true);
    tft.pushImage(x, y, F1_LOGO_WIDTH, F1_LOGO_HEIGHT, f1_logo);
    tft.setSwapBytes(false);
  }

  // Render the full schedule screen: header with F1 logo, red divider, session table.
  void drawScheduleScreen()
  {
    tft.fillScreen(TFT_BLACK);

    // Header: red left accent bar
    tft.fillRect(0, 0, 5, SCHED_DIVIDER_Y, TFT_RED);

    // Header: F1 logo (top-right corner)
    drawF1Logo(200, 4);

    // Header: "NEXT RACE" label
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("NEXT RACE", SCHED_COL_SESSION, 6, 2);

    // Header: race name — font 4 for ≤13 chars, font 2 for longer names
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (strlen(_schedRaceName) > 13)
    {
      tft.drawString(_schedRaceName, SCHED_COL_SESSION, 26, 2);
    }
    else
    {
      tft.drawString(_schedRaceName, SCHED_COL_SESSION, 22, 4);
    }

    // Header: date
    tft.setTextColor(0xBDD7, TFT_BLACK);
    tft.drawString(_schedDateStr, SCHED_COL_SESSION, 52, 2);

    // Red divider
    tft.fillRect(0, SCHED_DIVIDER_Y, 320, 2, TFT_RED);

    // Table header row
    tft.fillRect(0, SCHED_HDR_Y, 320, SCHED_HDR_H, 0x2104);
    tft.setTextColor(TFT_RED, 0x2104);
    tft.drawString("SESSION", SCHED_COL_SESSION, SCHED_HDR_Y + 2, 2);
    tft.drawString("DAY",     SCHED_COL_DAY,     SCHED_HDR_Y + 2, 2);
    tft.drawString("TIME",    SCHED_COL_TIME,    SCHED_HDR_Y + 2, 2);

    // Session rows
    for (int i = 0; i < _sessionCount; i++)
    {
      int rowY = SCHED_ROWS_START_Y + i * SCHED_ROW_H;
      uint16_t rowBg = (i % 2 == 0) ? (uint16_t)TFT_BLACK : (uint16_t)0x1082;
      tft.fillRect(0, rowY, 320, SCHED_ROW_H, rowBg);

      // Red left marker for the next upcoming session
      if (i == _nextSessionIdx)
      {
        tft.fillRect(0, rowY, 4, SCHED_ROW_H, TFT_RED);
      }

      // Future sessions: white; past: dim gray
      uint16_t textColor = _sessions[i].future ? (uint16_t)TFT_WHITE : (uint16_t)0x4208;
      tft.setTextColor(textColor, rowBg);
      tft.drawString(_sessions[i].name, SCHED_COL_SESSION, rowY + 3, 2);
      tft.drawString(_sessions[i].day,  SCHED_COL_DAY,     rowY + 3, 2);
      tft.drawString(_sessions[i].time, SCHED_COL_TIME,    rowY + 3, 2);
    }
  }

  // Draw race name + date overlay on top of the track image.
  void drawImageOverlay()
  {
    tft.fillRect(0, 205, 320, 35, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(_imageOverlayStr, screenCenterX, 212, 4);
  }
};

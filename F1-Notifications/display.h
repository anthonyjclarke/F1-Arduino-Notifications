
// ----------------------------
// display.h
// Abstract F1Display base class — defines the interface for all display types.
// ----------------------------

#ifndef F1DISPLAY_H
#define F1DISPLAY_H

enum F1DisplaySate
{
  unset,
  placeholder,
  raceweek
};

class F1Display {
  public:
    virtual void displaySetup() = 0;

    // Shows circuit image; called when it's more than DaysBeforeRace days out
    virtual void displayPlaceHolder(const char* raceName, JsonObject races_sessions) = 0;
    virtual void displayRaceWeek(const char* raceName, JsonObject races_sessions) = 0;
    virtual void drawWifiManagerMessage(WiFiManager *myWiFiManager) = 0;

    // Called each loop() iteration — handles timed slide alternation.
    // Default no-op; override in display implementations that support it.
    virtual void tickDisplay() {}

    F1DisplaySate state;

    void setRaceName(const char* raceName){
      strcpy(_currentRaceName, raceName);
    }

    bool isSameRace(const char* raceName){
      return strcmp(_currentRaceName, raceName) == 0;
    }

    void setWidth(int w) {
      screenWidth = w;
      screenCenterX = screenWidth / 2;
    }

    void setHeight(int h) {
      screenHeight = h;
    }

  protected:
    int screenWidth;
    int screenHeight;
    int screenCenterX;
    char _currentRaceName[200];
};
#endif

// ----------------------------
// debug.h
// Leveled debug logging system. Include this in every file that uses DBG_* macros.
// Set DEBUG_LEVEL via build flag in platformio.ini: -DDEBUG_LEVEL=3
// ----------------------------

#pragma once

#include <Arduino.h>
#include <stdarg.h>

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3  // Default: Info
#endif

#define DBG_LEVEL_OFF     0
#define DBG_LEVEL_ERROR   1
#define DBG_LEVEL_WARN    2
#define DBG_LEVEL_INFO    3
#define DBG_LEVEL_VERBOSE 4

// Runtime control — can be changed via web API
static uint8_t debugLevel = DEBUG_LEVEL;

static void debugLogf(uint8_t level, const char *label, const char *fmt, ...)
{
  if (debugLevel < level) return;
  char logMessage[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(logMessage, sizeof(logMessage), fmt, args);
  va_end(args);
  Serial.printf("[%s] %s\n", label, logMessage);
}

#define DBG_ERROR(...)   debugLogf(DBG_LEVEL_ERROR,   "ERROR",   __VA_ARGS__)
#define DBG_WARN(...)    debugLogf(DBG_LEVEL_WARN,    "WARN",    __VA_ARGS__)
#define DBG_INFO(...)    debugLogf(DBG_LEVEL_INFO,    "INFO",    __VA_ARGS__)
#define DBG_VERBOSE(...) debugLogf(DBG_LEVEL_VERBOSE, "VERBOSE", __VA_ARGS__)

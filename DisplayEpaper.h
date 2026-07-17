#ifndef DISPLAY_EPAPER_H
#define DISPLAY_EPAPER_H

#include <Arduino.h>
#include "config.h"

#if EPAPER_ENABLE

void displayBegin();
void displayShowSplash();
void displayUpdate(float temperatureC, float batteryVoltage, int batteryPercent);
void displayUpdateError(float batteryVoltage, int batteryPercent);
void displayHibernate();
void displayWake();

#else

static inline void displayBegin() {}
static inline void displayShowSplash() {}
static inline void displayUpdate(float, float, int) {}
static inline void displayUpdateError(float, int) {}
static inline void displayHibernate() {}
static inline void displayWake() {}

#endif

#endif

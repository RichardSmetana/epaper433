#include "DisplayEpaper.h"

#if EPAPER_ENABLE

#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "epaper433_logo.h"

// LilyGO / TTGO T5 V2.3.1 2.13" ePaper
// Pins: CS=5, DC=17, RST=16, BUSY=4 — SPI: SCK=18, MOSI=23
static GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
  GxEPD2_213_BN(EPAPER_CS_PIN, EPAPER_DC_PIN, EPAPER_RST_PIN, EPAPER_BUSY_PIN));

static void drawTemperature(float temperature) {
  char number[10];
  snprintf(number, sizeof(number), "%.2f", temperature);

  display.setFont(&FreeSansBold24pt7b);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK);

  int16_t x1, y1;
  uint16_t numberWidth, numberHeight;
  display.getTextBounds(number, 0, 0, &x1, &y1, &numberWidth, &numberHeight);

  const int degreeSpace = 24;
  const int totalWidth = numberWidth + degreeSpace;
  const int numberX = (display.width() - totalWidth) / 2;
  const int baselineY = (display.height() - numberHeight) / 2 - y1;

  display.setCursor(numberX, baselineY);
  display.print(number);

  const int degreeCenterX = numberX + numberWidth + 15;
  const int degreeCenterY = baselineY - numberHeight + 12;

  for (char r = 5; r <= 9; r++) {
    display.drawCircle(degreeCenterX, degreeCenterY, r, GxEPD_BLACK);
  }
}

static void drawError() {
  const char *text = "Error";

  display.setFont(&FreeSansBold24pt7b);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK);

  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  display.getTextBounds(text, 0, 0, &x1, &y1, &textWidth, &textHeight);

  const int textX = (display.width() - (int)textWidth) / 2;
  const int baselineY = (display.height() - (int)textHeight) / 2 - y1;

  display.setCursor(textX, baselineY);
  display.print(text);
}

static void drawVoltage(float voltage, int percent) {
  char number[20];
  snprintf(number, sizeof(number), "%.2f V  %d%%", voltage, percent);

  display.setFont(&FreeSansBold9pt7b);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);

  int16_t x1, y1;
  uint16_t numberWidth, numberHeight;
  display.getTextBounds(number, 0, 0, &x1, &y1, &numberWidth, &numberHeight);

  const int numberX = (display.width() - (int)numberWidth) / 2;
  const int baselineY = display.height() - 1;

  display.setCursor(numberX, baselineY);
  display.print(number);
}

void displayBegin() {
  display.init(0, true, 2, false);
  display.setRotation(EPAPER_ROTATION);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold9pt7b);
}

void displayShowSplash() {
  const int16_t x = (display.width() - EPAPER433_LOGO_WIDTH) / 2;
  const int16_t y = (display.height() - EPAPER433_LOGO_HEIGHT) / 2;

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(x, y, epaper433_logo, EPAPER433_LOGO_WIDTH, EPAPER433_LOGO_HEIGHT, GxEPD_BLACK);
  } while (display.nextPage());
}

void displayUpdate(float temperatureC, float batteryVoltage, int batteryPercent) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawTemperature(temperatureC);
    drawVoltage(batteryVoltage, batteryPercent);
  } while (display.nextPage());
}

void displayUpdateError(float batteryVoltage, int batteryPercent) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawError();
    drawVoltage(batteryVoltage, batteryPercent);
  } while (display.nextPage());
}

void displayHibernate() {
  display.hibernate();
}

void displayWake() {
  display.init(0, false, 2, false);
  display.setRotation(EPAPER_ROTATION);
  display.setTextColor(GxEPD_BLACK);
}

#endif

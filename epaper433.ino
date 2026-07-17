/*
 * WT450-TH transmitter with DS18B20 + ePaper for LilyGO T5 / ESP32
 *
 * Pins and timing: see config.h
 * DS18B20 : 1-Wire (RMT / OneWireESP32)
 * ePaper  : GxEPD2 2.13" (T5 V2.3.1)
 * TX      : 433 MHz ASK
 * Power   : light sleep by default; deep sleep with ESP32_DEEP_SLEEP=1
 */

#include "config.h"
#include "Dbg.h"
#include "Ds18b20OneWire.h"
#include "Wt450Send.h"
#include "LowPowerSleep.h"
#include "DisplayEpaper.h"

#include <WiFi.h>
#include <math.h>
#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_sleep.h>

Ds18b20OneWire ds18b20(DS18B20_PIN);
Wt450Sender wt450(TX_PIN);
static bool sensorReady = false;

// Survive deep sleep — used to skip ePaper refresh when values barely change
RTC_DATA_ATTR static float lastEpaperTempC = -999.0f;
RTC_DATA_ATTR static int lastEpaperPercent = -1;
RTC_DATA_ATTR static bool lastEpaperWasError = false;
RTC_DATA_ATTR static bool lastEpaperValid = false;

static void printWakeupCause() {
#if SERIAL_DEBUG
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  DBG_PRINT(F("Wake cause: "));
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER: DBG_PRINTLN(F("timer")); break;
    case ESP_SLEEP_WAKEUP_EXT0: DBG_PRINTLN(F("button")); break;
    case ESP_SLEEP_WAKEUP_UNDEFINED: DBG_PRINTLN(F("reset/power-on")); break;
    default: DBG_PRINTLN((int)cause); break;
  }
#endif
}

static bool epaperNeedsUpdate(float tempC, int percent, bool isError) {
  if (!lastEpaperValid) {
    return true;
  }
  if (isError != lastEpaperWasError) {
    return true;
  }
  if (abs(percent - lastEpaperPercent) >= EPAPER_UPDATE_PERCENT) {
    return true;
  }
  if (!isError && fabsf(tempC - lastEpaperTempC) >= EPAPER_UPDATE_TEMP_C) {
    return true;
  }
  return false;
}

static void epaperMarkUpdated(float tempC, int percent, bool isError) {
  lastEpaperTempC = tempC;
  lastEpaperPercent = percent;
  lastEpaperWasError = isError;
  lastEpaperValid = true;
}

static void epaperShow(float tempC, float batteryVoltage, int batteryPercent, bool isError) {
  if (!epaperNeedsUpdate(tempC, batteryPercent, isError)) {
    DBG_PRINTLN(F("ePaper skip (unchanged)"));
    return;
  }

  displayWake();
  if (isError) {
    displayUpdateError(batteryVoltage, batteryPercent);
  } else {
    displayUpdate(tempC, batteryVoltage, batteryPercent);
  }
  displayHibernate();
  epaperMarkUpdated(tempC, batteryPercent, isError);
}

static void prepareEpaperPinsIdle() {
  // T5 V2.3.1: ohne definierte Pegel schweben CS/RST oft → Display zieht ~0.5–0.9 mA
  pinMode(EPAPER_CS_PIN, OUTPUT);
  digitalWrite(EPAPER_CS_PIN, HIGH);
  pinMode(EPAPER_RST_PIN, OUTPUT);
  digitalWrite(EPAPER_RST_PIN, HIGH);
  pinMode(EPAPER_DC_PIN, OUTPUT);
  digitalWrite(EPAPER_DC_PIN, LOW);
  pinMode(EPAPER_SCK_PIN, OUTPUT);
  digitalWrite(EPAPER_SCK_PIN, LOW);
  pinMode(EPAPER_MOSI_PIN, OUTPUT);
  digitalWrite(EPAPER_MOSI_PIN, LOW);
  pinMode(EPAPER_BUSY_PIN, INPUT);
}

static void prepareForSleep() {
#if SERIAL_DEBUG
  Serial.flush();
#endif
  displayHibernate();
  delay(100);  // SSD1680 Deep-Sleep-Befehl abwarten, bevor GPIOs umkonfiguriert werden

  prepareEpaperPinsIdle();

  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);

  // 1-Wire: externer Pull-up hält DQ; Pin nicht treiben
  pinMode(DS18B20_PIN, INPUT);

#if BATTERY_ADC_PIN >= 0
  pinMode(BATTERY_ADC_PIN, INPUT);
#endif

#if ESP32_DEEP_SLEEP
  // Pegel über Deep Sleep halten (sonst schweben die Leitungen nach Reset der IO-Matrix)
  gpio_hold_en((gpio_num_t)EPAPER_CS_PIN);
  gpio_hold_en((gpio_num_t)EPAPER_RST_PIN);
  gpio_hold_en((gpio_num_t)EPAPER_DC_PIN);
  gpio_hold_en((gpio_num_t)EPAPER_SCK_PIN);
  gpio_hold_en((gpio_num_t)EPAPER_MOSI_PIN);
  gpio_hold_en((gpio_num_t)TX_PIN);
#endif
}

static void wakeFromSleep() {
  if (!ds18b20.init()) {
    sensorReady = false;
    DBG_PRINTLN(F("DS18B20 reinit after sleep FAILED"));
  } else {
    sensorReady = true;
  }
  wt450.begin(TX_INVERT);
  if (lowPowerWokeFromButton()) {
    DBG_PRINTLN(F("button wake -> measure now"));
  }
}

static void sleepMs(uint32_t ms) {
  lowPowerSleepMs(ms, prepareForSleep, wakeFromSleep);
}

// Bus + address only — no conversion (measurement happens once in loop)
static bool initDs18b20() {
  ds18b20.begin();
  if (!ds18b20.init()) {
    DBG_PRINTLN(F("DS18B20 init FAILED (no device)"));
    return false;
  }
  DBG_PRINTLN(F("DS18B20 bus OK"));
  return true;
}

static bool isValidTemperature(float tempC) {
  return tempC >= -55.0f && tempC <= 125.0f;
}

static Wt450Data makeWt450Payload(uint8_t house, uint8_t channel, float tempC, uint8_t humidityPct, bool batteryOk) {
  Wt450Data data;
  data.house = house;
  data.channel = channel;
  data.tempC = tempC;
  data.humidity = humidityPct;
  data.batteryOk = batteryOk;
  return data;
}

void setup() {
  lowPowerReleaseGpioHold();  // ePaper/TX Holds vom vorherigen Deep Sleep lösen
  WiFi.mode(WIFI_OFF);
  btStop();
  setCpuFrequencyMhz(80);

  lowPowerButtonBegin();

  DBG_BEGIN(115200);
#if SERIAL_DEBUG
  for (char iii = 0; iii < 10; iii++) {
    delay(200);
    DBG_PRINTLN();
  }
  DBG_PRINTLN(F("ESP32  WT450-TH + DS18B20 + ePaper + STX882"));
  DBG_PRINTLN(F("----------------------------------------------"));
  DBG_PRINT(F("CPU="));
  DBG_PRINT(getCpuFrequencyMhz());
  DBG_PRINT(F(" MHz "));
  DBG_PRINT(F("DS18B20=GPIO"));
  DBG_PRINT(DS18B20_PIN);
  DBG_PRINTLN(F(" (RMT)"));
  DBG_PRINT(F(" TX=GPIO"));
  DBG_PRINTLN(TX_PIN);
  DBG_PRINT(F(" Button=GPIO"));
  DBG_PRINT(BUTTON_PIN);
  DBG_PRINTLN(F(" (wake)"));
#if EPAPER_ENABLE
  DBG_PRINTLN(F(" ePaper: T5 2.13\" enabled"));
#endif
  DBG_PRINT(F("Sleep: "));
#if ESP32_DEEP_SLEEP
  DBG_PRINTLN(F("deep (battery only, no USB)"));
#else
  DBG_PRINTLN(F("light (USB-safe)"));
#endif
  printWakeupCause();
  DBG_PRINTLN(F("----------------------------------------------"));
#endif

#if BATTERY_ADC_PIN >= 0
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
#endif

  // Splash + displayInit nur bei Power-On — nach Deep-Sleep-Wake kein ePaper bis Update nötig
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    lastEpaperValid = false;
    displayBegin();
    displayShowSplash();
    displayHibernate();
    sleepMs(2000);  // light sleep: weiter hier; deep sleep: Reset → setup ohne Splash
  }

  sensorReady = initDs18b20();
  wt450.begin(TX_INVERT);
}

long readVcc() {
#if BATTERY_ADC_PIN >= 0
  return (long)(analogReadMilliVolts(BATTERY_ADC_PIN) * BATTERY_DIVIDER_RATIO);
#else
  return 3700L;
#endif
}

void loop() {
  uint32_t sleepMsDuration = SEND_INTERVAL_MS;

  float batteryVoltage = readVcc() / 1000.0;
  int batteryPercent = batteryPercentFromVoltage(batteryVoltage);
  DBG_PRINT(F("Battery: "));
  DBG_PRINT(batteryVoltage);
  DBG_PRINT(F(" V  "));
  DBG_PRINT(batteryPercent);
  DBG_PRINTLN(F(" %"));

  if (!sensorReady) {
    DBG_PRINTLN(F("retry DS18B20 init..."));
    sensorReady = initDs18b20();
    if (!sensorReady) {
      epaperShow(0.0f, batteryVoltage, batteryPercent, true);
    }
    sleepMsDuration = SENSOR_RETRY_SLEEP_MS;
    sleepMs(sleepMsDuration);
    return;
  }

  float tempC = 0.0f;
  // WT450 humidity field: 0..99 — map battery charge % into it
  uint8_t humidityPct = (uint8_t)constrain(batteryPercent, 0, 99);
  int sensorError = SENSOR_OK;
  int lastReadErr = 0;

  for (uint8_t attempt = 0; attempt < SENSOR_READ_RETRIES; attempt++) {
    if (!ds18b20.readTemperatureC(tempC, lastReadErr)) {
      sensorError = lastReadErr;
      DBG_PRINT(F("DS18B20 read error: "));
      DBG_PRINT(lastReadErr);
      switch (lastReadErr) {
        case Ds18b20OneWire::NO_DEVICE: DBG_PRINTLN(F(" (no device)")); break;
        case Ds18b20OneWire::CRC_FAIL: DBG_PRINTLN(F(" (crc)")); break;
        case Ds18b20OneWire::INVALID: DBG_PRINTLN(F(" (invalid)")); break;
        default: DBG_PRINTLN(F(" (read fail)")); break;
      }
      if (lastReadErr == Ds18b20OneWire::CRC_FAIL ||
          lastReadErr == Ds18b20OneWire::NO_DEVICE ||
          lastReadErr == Ds18b20OneWire::READ_FAIL) {
        ds18b20.reinitBus();
        ds18b20.init();
      }
      delay(SENSOR_RETRY_MS);
      continue;
    }

    if (!isValidTemperature(tempC)) {
      sensorError = SENSOR_ERR_INVALID;
      DBG_PRINT(F("DS18B20 invalid: "));
      DBG_PRINT(tempC, 2);
      DBG_PRINTLN(F(" C"));
      delay(SENSOR_RETRY_MS);
      continue;
    }

    sensorError = SENSOR_OK;
    if (attempt > 0) {
      DBG_PRINT(F("DS18B20 OK on retry "));
      DBG_PRINTLN(attempt + 1);
    }
    break;
  }

  if (sensorError != SENSOR_OK) {
    DBG_PRINT(F("no TX (sensorError="));
    DBG_PRINT(sensorError);
    DBG_PRINTLN(F(")"));
    epaperShow(0.0f, batteryVoltage, batteryPercent, true);
    sleepMsDuration = SENSOR_RETRY_SLEEP_MS;
  } else {
    epaperShow(tempC, batteryVoltage, batteryPercent, false);

    Wt450Data payload = makeWt450Payload(WT450_HOUSE_CODE, WT450_CHANNEL, tempC, humidityPct, true);

    DBG_PRINT(F("sending: "));
    DBG_PRINT(tempC, 2);
    DBG_PRINT(F(" C  bat="));
    DBG_PRINT(humidityPct);
    DBG_PRINT(F("% "));

#if SERIAL_DEBUG
    uint8_t pkt[5];
    for (uint8_t seq = 0; seq < 3; seq++) {
      if (wt450.buildPreview(payload, seq, pkt)) {
        DBG_PRINT(F("seq "));
        DBG_PRINT(seq);
        DBG_PRINT(F("  pkt="));
        for (uint8_t i = 0; i < 5; i++) {
          if (pkt[i] < 0x10) {
            DBG_PRINT('0');
          }
          DBG_PRINT(pkt[i], HEX);
          DBG_PRINT(' ');
        }
        DBG_PRINT(F(" pulses="));
        DBG_PRINT(Wt450Sender::expectedPulseCount(pkt));
        DBG_PRINT(F(" "));
      }
    }
#endif

    if (wt450.send(payload)) {
      DBG_PRINTLN(F(" sent OK"));
    } else {
      DBG_PRINTLN(F(" SEND FAIL"));
    }
  }

  sleepMs(sleepMsDuration);
}

#include "LowPowerSleep.h"
#include "config.h"

#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

static void waitButtonReleased() {
  const uint8_t idleLevel = BUTTON_WAKE_LEVEL ? LOW : HIGH;
  uint32_t stableSince = millis();
  while (true) {
    if (digitalRead(BUTTON_PIN) == idleLevel) {
      if ((millis() - stableSince) >= BUTTON_DEBOUNCE_MS) {
        break;
      }
    } else {
      stableSince = millis();
    }
    delay(5);
  }
}

static void prepareWakeSources(uint32_t ms) {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (ms > 0) {
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, BUTTON_WAKE_LEVEL);
}

void lowPowerButtonBegin() {
  pinMode(BUTTON_PIN, INPUT);
}

bool lowPowerWokeFromButton() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}

void lowPowerSleepMs(uint32_t ms, LowPowerHookFn beforeSleep, LowPowerHookFn afterWake) {
  if (ms == 0) {
    return;
  }

  waitButtonReleased();

  if (beforeSleep) {
    beforeSleep();
  }

#if ESP32_DEEP_SLEEP
#if SERIAL_DEBUG
  Serial.flush();
  Serial.end();
  delay(50);
#endif

  WiFi.mode(WIFI_OFF);
  gpio_deep_sleep_hold_en();
  prepareWakeSources(ms);
  esp_deep_sleep_start();
#else
  prepareWakeSources(ms);
  esp_light_sleep_start();
  if (afterWake) {
    afterWake();
  }
#endif
}

void lowPowerReleaseGpioHold() {
  gpio_deep_sleep_hold_dis();

  gpio_hold_dis((gpio_num_t)EPAPER_CS_PIN);
  gpio_hold_dis((gpio_num_t)EPAPER_RST_PIN);
  gpio_hold_dis((gpio_num_t)EPAPER_DC_PIN);
  gpio_hold_dis((gpio_num_t)EPAPER_SCK_PIN);
  gpio_hold_dis((gpio_num_t)EPAPER_MOSI_PIN);
  gpio_hold_dis((gpio_num_t)TX_PIN);
}

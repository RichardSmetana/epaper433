#ifndef CONFIG_H
#define CONFIG_H

// ========== GPIO — LilyGO / TTGO T5 V2.3.1 ==========
// ePaper belegt: CS=5, DC=17, RST=16, BUSY=4, SCK=18, MOSI=23
#define TX_PIN                25   // 433 MHz ASK transmitter data (nicht GPIO4 = ePaper BUSY)
#define DS18B20_PIN           21   // DS18B20 1-Wire data (4.7k pull-up to 3.3V)
#define BATTERY_ADC_PIN       35   // Battery ADC input (input-only GPIO). -1 = no ADC

#define EPAPER_CS_PIN          5
#define EPAPER_DC_PIN         17
#define EPAPER_RST_PIN        16
#define EPAPER_BUSY_PIN        4
#define EPAPER_SCK_PIN        18   // SPI SCLK — im Deep Sleep LOW halten
#define EPAPER_MOSI_PIN       23   // SPI MOSI — im Deep Sleep LOW halten

// T5 onboard button — GPIO39 is input-only (external pull-up on board)
#define BUTTON_PIN            39
#define BUTTON_WAKE_LEVEL      0   // 0 = pressed = GND (active low)
#define BUTTON_DEBOUNCE_MS    50

// Measured battery mV = analogReadMilliVolts(BATTERY_ADC_PIN) * divider ratio
#define BATTERY_DIVIDER_RATIO 2.0f
#define BATTERY_FULL_V        4.15f  // ~100 % (etwas unter 4.2 V)
#define BATTERY_EMPTY_V       3.30f  // ~0 %

#define ESP32_DEEP_SLEEP      1
#define SERIAL_DEBUG          0

static inline int batteryPercentFromVoltage(float voltage) {
  if (voltage >= BATTERY_FULL_V) {
    return 100;
  }
  if (voltage <= BATTERY_EMPTY_V) {
    return 0;
  }
  return (int)((voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f);
}

// ========== Sensor / radio ==========
#define TX_INVERT             0
#define WT450_HOUSE_CODE      1
#define WT450_CHANNEL         2    // 1..4 — temperature; humidity field carries battery %

// ========== Display ==========
#ifndef EPAPER_ENABLE
#define EPAPER_ENABLE         1
#endif
#define EPAPER_ROTATION       1
// Skip ePaper refresh if change is below these thresholds (saves most active energy)
#define EPAPER_UPDATE_TEMP_C   0.1f
#define EPAPER_UPDATE_PERCENT  1

// ========== Timing / retries ==========
//#define SEND_INTERVAL_MS 15000
#define SEND_INTERVAL_MS      60000
#define SENSOR_READ_RETRIES   3
#define SENSOR_RETRY_MS       200
#define SENSOR_RETRY_SLEEP_MS 5000

// DS18B20 timing / CRC resilience (RMT / OneWireESP32)
#define DS18B20_CONVERT_MS    850   // 12-bit needs ~750 ms; margin for supply noise
#define DS18B20_CRC_RETRIES   4     // re-read scratchpad before new conversion

// ========== Power / debug ==========
// ESP32_DEEP_SLEEP=0: light sleep — safe with USB serial (no boot loop).
// ESP32_DEEP_SLEEP=1: deep sleep — only for battery operation WITHOUT USB.
#ifndef ESP32_DEEP_SLEEP
#define ESP32_DEEP_SLEEP      0
#endif

#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG          1
#endif

#define SENSOR_OK             0
#define SENSOR_ERR_INVALID    1

#endif

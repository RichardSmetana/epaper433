# epaper433 — WT450 wireless sensor (ESP32 + ePaper)

**Languages:** [English](README.md) · [Deutsch](README_DE.md)

Measure temperature with a **DS18B20**, show it on a **LilyGO T5 2.13" ePaper**, and transmit via **433 MHz** using the **WT450 / WT450H** protocol. Between cycles the ESP32 sleeps (light sleep or deep sleep). The onboard button wakes the board and starts a measurement immediately.

Receivers: e.g. [rtl_433](https://github.com/merbanan/rtl_433).

---

## Features

- **DS18B20** temperature (1-Wire, ESP32 RMT / `OneWireESP32`)
- **ePaper**: large temperature, voltage + **battery %** at the bottom (refresh only if ΔT ≥ 0.1 °C or Δ% ≥ 1)
- **Splash logo** on cold boot, then hibernate until first reading
- **WT450 OOK** via ASK transmitter (e.g. STX882)
- Battery via ADC (optional); **charge %** is sent in the WT450 **humidity** field
- **GPIO 39 button**: wake from sleep + immediate measure/send cycle
- Light sleep (USB-safe) / deep sleep (battery, no USB)
- All pins and timings in `config.h`

---

## Hardware

| Part | Notes |
|------|--------|
| LilyGO / TTGO T5 V2.3.1 | ESP32 + 2.13" ePaper (`GxEPD2_213_BN`) |
| DS18B20 | Temperature sensor |
| 433 MHz TX | e.g. STX882, FS1000A |
| 4.7 kΩ | 1-Wire DQ pull-up to 3.3 V |
| Optional | Voltage divider on ADC for battery |

### Pinout (T5 V2.3.1)

| Function | GPIO | Notes |
|----------|------|--------|
| DS18B20 DQ | **21** | + 4.7 kΩ to 3.3 V |
| 433 MHz TX DATA | **25** | not GPIO 4! |
| Battery ADC | **35** | input-only; `-1` = disabled |
| Button (wake) | **39** | onboard, active low |
| ePaper CS | 5 | onboard |
| ePaper DC | 17 | onboard |
| ePaper RST | 16 | onboard |
| ePaper BUSY | **4** | onboard — do not use for TX |
| SPI SCK / MOSI | 18 / 23 | ESP32 default SPI |

### Wiring

```
ESP32 3.3V ──────── DS18B20 VDD
ESP32 GND  ──────── DS18B20 GND
ESP32 GPIO 21 ───── DS18B20 DQ ─── 4.7 kΩ ─── 3.3V

ESP32 GPIO 25 ───── 433 MHz TX DATA
ESP32 GND     ───── 433 MHz TX GND
3.3V / 5V     ───── 433 MHz TX VCC (per module)

Optional battery:
ESP32 GPIO 35 ───── divider midpoint (ADC)

Onboard:
  ePaper: CS=5, DC=17, RST=16, BUSY=4, SCK=18, MOSI=23
  Button: GPIO 39
```

> GPIO 4 is **ePaper BUSY**. Use **GPIO 25** for the transmitter (`TX_PIN`).

---

## Software

### Requirements

- Arduino IDE 2.x (or PlatformIO)
- ESP32 board support (**Arduino-ESP32 Core 3.x** recommended)
- Libraries:
  - **esp32-ds18b20** — [junkfix/esp32-ds18b20](https://github.com/junkfix/esp32-ds18b20) (`OneWireESP32`)
  - **GxEPD2** (Jean-Marc Zingg)
  - **Adafruit GFX** (GxEPD2 dependency)

### Board / upload

| Setting | Value |
|---------|--------|
| Board | ESP32 Dev Module / LilyGO T5 |
| Upload speed | 921600 (or 115200) |
| Serial monitor | **115200** baud |
| CPU | 80 MHz (set in sketch) |

### Flash

1. Open folder `epaper433` (`epaper433.ino`)
2. Check `config.h`
3. Compile & upload
4. Open Serial Monitor (if `SERIAL_DEBUG 1`)

---

## Configuration (`config.h`)

```cpp
// GPIO
#define TX_PIN                25
#define DS18B20_PIN           21
#define BATTERY_ADC_PIN       35      // -1 = no battery ADC
#define BATTERY_DIVIDER_RATIO 2.0f
#define BATTERY_FULL_V        4.15f   // ~100 %
#define BATTERY_EMPTY_V       3.30f   // ~0 %

#define EPAPER_CS_PIN          5
#define EPAPER_DC_PIN         17
#define EPAPER_RST_PIN        16
#define EPAPER_BUSY_PIN        4
#define EPAPER_SCK_PIN        18
#define EPAPER_MOSI_PIN       23
#define EPAPER_ENABLE          1      // 0 = display off
#define EPAPER_ROTATION        1
#define EPAPER_UPDATE_TEMP_C   0.1f   // skip refresh if smaller change
#define EPAPER_UPDATE_PERCENT  1

#define BUTTON_PIN            39
#define BUTTON_WAKE_LEVEL      0      // 0 = pressed = GND
#define BUTTON_DEBOUNCE_MS    50

// WT450
#define TX_INVERT              0
#define WT450_HOUSE_CODE       1      // 1..15
#define WT450_CHANNEL          2      // 1..4 (humidity field = battery %)

// Timing
#define SEND_INTERVAL_MS   60000
#define SENSOR_READ_RETRIES    3
#define SENSOR_RETRY_MS      200
#define SENSOR_RETRY_SLEEP_MS 5000
#define DS18B20_CONVERT_MS   850
#define DS18B20_CRC_RETRIES    4

// Power / debug
#define ESP32_DEEP_SLEEP       1      // 0=light sleep, 1=deep sleep
#define SERIAL_DEBUG           0
```

Battery % uses `batteryPercentFromVoltage()` (linear between `BATTERY_EMPTY_V` and `BATTERY_FULL_V`). WT450 humidity is clamped to **0–99**.

---

## Operation

```
Cold boot → splash → hibernate → sleep
    ↓
Timer or button wake
    → read DS18B20
    → update ePaper if values changed (or show "Error")
    → send WT450 (temp + battery % as humidity)
    → hibernate / deep sleep until next timer or button
```

Every successful cycle sends temperature and battery charge (0–99 %) in the humidity field. There is no separate battery channel.

### Button (GPIO 39)

- Wakes from light and deep sleep (`ext0`)
- Then: measure → display → send → sleep
- Waits for release before sleep (debounce)

GPIO 39 has no internal pull-up; the T5 board provides an external one.

### Sensor error on ePaper

If the DS18B20 is missing or read fails: display shows **"Error"** plus battery voltage/%. No radio TX in that case.

---

## Power saving

| Mode | `ESP32_DEEP_SLEEP` | Typical (T5) | Use |
|------|--------------------|--------------|-----|
| Light sleep | `0` | ~0.8 mA+ | USB / development |
| Deep sleep | `1` | ~0.5–0.9 mA on T5 | Battery, **no USB** |

Bare ESP32 deep sleep is ~10–150 µA. On **LilyGO T5 V2.3.1**, **500–900 µA** at BAT is common. Well optimized: ~200–400 µA; below ~150 µA usually needs hardware changes.

**Important:** Deep sleep with USB often causes a boot loop (`rst:0x5 DEEPSLEEP_RESET`). Measure current on BAT only, USB disconnected.

### Before deep sleep the sketch

1. `display.hibernate()` (SSD1680 deep sleep)
2. Idle levels: CS/RST **HIGH**, DC/SCK/MOSI **LOW**
3. TX (GPIO 25) **LOW**
4. `gpio_hold_en` + `gpio_deep_sleep_hold_en` so lines do not float

On boot: `lowPowerReleaseGpioHold()` releases all holds.

Also: Wi-Fi/BT off, CPU 80 MHz, ePaper refresh skipped when values unchanged, single DS18B20 conversion per cycle.

### Hardware tips (if still ~0.9 mA)

| Action | Effect |
|--------|--------|
| 100 kΩ pull-up CS (GPIO 5) → 3.3 V | Keeps CS HIGH |
| 100 kΩ pull-up RST (GPIO 16) → 3.3 V | Keeps RST HIGH |
| ADC divider ≥ 100 kΩ+100 kΩ (better 470 k) | 10 k+10 k alone ≈ 200 µA continuous |
| Switch STX882 VCC (P-MOSFET) | Cuts TX idle current |
| Disconnect display for reference | Often ~100–150 µA without panel |

Wake sources: **timer** (`SEND_INTERVAL_MS`) and **button** (GPIO 39).

---

## Serial debug (example)

Enable with `#define SERIAL_DEBUG 1`.

```
ESP32  WT450-TH + DS18B20 + ePaper + STX882
----------------------------------------------
CPU=80 MHz DS18B20=GPIO21 (RMT)
 TX=GPIO25
 Button=GPIO39 (wake)
 ePaper: T5 2.13" enabled
Sleep: deep (battery only, no USB)
Wake cause: timer
----------------------------------------------
Battery: 3.71 V  68 %
sending: 23.50 C  bat=68%  sent OK
```

| Message | Meaning |
|---------|---------|
| `(no device)` | Sensor / pull-up / wiring |
| `(crc)` | Noisy 1-Wire — bus re-init |
| `(invalid)` | Temp outside −55…125 °C |
| `(read fail)` | Other read error |

---

## Receive with rtl_433

```bash
rtl_433 -f 433.92M -s 250k
```

House code and channel must match `WT450_HOUSE_CODE` / `WT450_CHANNEL`. If nothing is received, try `TX_INVERT 1`. Battery % appears as humidity.

---

## Project layout

```
epaper433/
├── epaper433.ino         Main sketch
├── config.h              GPIO, timing, power, display, battery, button
├── DisplayEpaper.h/cpp   ePaper (temp, volt, %, splash, error)
├── Ds18b20OneWire.h/cpp  DS18B20 (OneWireESP32 / RMT)
├── Wt450Send.h/cpp       WT450 protocol
├── LowPowerSleep.h/cpp   Light/deep sleep + button wake + GPIO hold
├── epaper433_logo.h      Splash bitmap (PROGMEM)
├── assets/               Logo source images
├── Dbg.h                 Debug macros
├── LICENSE               GPL-3.0
├── README.md             English
└── README_DE.md          Deutsch
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Boot loop `DEEPSLEEP_RESET` | Unplug USB or `ESP32_DEEP_SLEEP 0` |
| Deep sleep ~0.9 mA | Pin holds? 100 kΩ on CS/RST; check ADC divider; USB off |
| Button does not wake | `BUTTON_PIN 39`, `BUTTON_WAKE_LEVEL 0` |
| ePaper stays white | GxEPD2 + Adafruit GFX; panel `GxEPD2_213_BN` |
| Display/BUSY stuck | Do not put TX on GPIO 4 — use `TX_PIN=25` |
| DS18B20 / `(crc)` | 4.7 kΩ pull-up, GPIO 21, `esp32-ds18b20`, serial shows `(RMT)` |
| No RF reception | Antenna, `TX_PIN`, `TX_INVERT`, house/channel |
| Wrong battery reading | Adjust `BATTERY_DIVIDER_RATIO`; ADC pins 32–39 only |
| Battery % inaccurate | Tune `BATTERY_FULL_V` / `BATTERY_EMPTY_V` |

---

## License

[GPL-3.0](LICENSE)

# epaper433 — WT450-Funksensor (ESP32 + ePaper)

**Sprachen:** [English](README.md) · [Deutsch](README_DE.md)

Temperatur über **DS18B20** messen, auf dem **LilyGO T5 2.13"-ePaper** anzeigen und per **433 MHz** im **WT450/WT450H-Protokoll** senden. Zwischen den Zyklen schläft der ESP32 (Light Sleep oder Deep Sleep). Die Onboard-Taste weckt das Board und startet sofort einen Messzyklus.

Empfänger z. B. [rtl_433](https://github.com/merbanan/rtl_433).

---

## Funktionen

- Temperaturmessung **DS18B20** (1-Wire, ESP32 über RMT / `OneWireESP32`)
- **ePaper-Anzeige**: Temperatur groß, Batteriespannung und **Ladezustand in %** unten (Refresh nur bei Änderung ≥0,1 °C bzw. ≥1 %)
- **Splash-Logo** beim Kaltstart, danach Hibernate bis zur ersten Messung
- **WT450-OOK** über ASK-Sender (z. B. STX882)
- Batteriespannung per ADC (optional); **Ladezustand (%)** wird im WT450-Feld **humidity** mitgesendet
- **Taste GPIO 39**: Wakeup aus Sleep + sofortiger Mess-/Sendezyklus
- Energiesparmodi: Light Sleep (USB-sicher) / Deep Sleep (Batterie, ohne USB)
- Alle Pins und Intervalle in `config.h`

---

## Hardware

| Bauteil | Beschreibung |
|---------|--------------|
| LilyGO / TTGO T5 V2.3.1 | ESP32 + 2.13" ePaper (`GxEPD2_213_BN`) |
| DS18B20 | Temperatursensor |
| 433-MHz-Sender | z. B. STX882, FS1000A |
| 4,7 kΩ | Pull-up für 1-Wire DQ |
| Optional | Spannungsteiler an ADC für Batterie |

### Pinbelegung (T5 V2.3.1)

| Funktion | GPIO | Hinweis |
|----------|------|---------|
| DS18B20 DQ | **21** | + 4,7 kΩ nach 3,3 V |
| 433 MHz TX DATA | **25** | nicht GPIO 4! |
| Batterie ADC | **35** | nur Eingang; `-1` = aus |
| Taste (Wakeup) | **39** | onboard, aktiv low |
| ePaper CS | 5 | onboard |
| ePaper DC | 17 | onboard |
| ePaper RST | 16 | onboard |
| ePaper BUSY | **4** | onboard — nicht für TX nutzen |
| SPI SCK / MOSI | 18 / 23 | Standard-ESP32-SPI |

### Verdrahtung

```
ESP32 3.3V ──────── DS18B20 VDD
ESP32 GND  ──────── DS18B20 GND
ESP32 GPIO 21 ───── DS18B20 DQ ─── 4,7 kΩ ─── 3.3V

ESP32 GPIO 25 ───── 433-MHz-Sender DATA
ESP32 GND     ───── 433-MHz-Sender GND
3.3V / 5V     ───── 433-MHz-Sender VCC (je nach Modul)

Optional Batterie:
ESP32 GPIO 35 ───── Mitte Spannungsteiler (ADC)

Onboard:
  ePaper: CS=5, DC=17, RST=16, BUSY=4, SCK=18, MOSI=23
  Taste:  GPIO 39
```

> GPIO 4 ist **ePaper BUSY**. Der Sender muss an **GPIO 25** (`TX_PIN`).

---

## Software

### Voraussetzungen

- Arduino IDE 2.x (oder PlatformIO)
- ESP32 Board Support (**Arduino-ESP32 Core 3.x** empfohlen)
- Libraries:
  - **esp32-ds18b20** — [junkfix/esp32-ds18b20](https://github.com/junkfix/esp32-ds18b20) (`OneWireESP32`)
  - **GxEPD2** (Jean-Marc Zingg)
  - **Adafruit GFX** (Abhängigkeit von GxEPD2)

### Board / Upload

| Einstellung | Wert |
|-------------|------|
| Board | ESP32 Dev Module / LilyGO T5 |
| Upload Speed | 921600 (sonst 115200) |
| Serial Monitor | **115200** Baud |
| CPU | 80 MHz (im Sketch gesetzt) |

### Flashen

1. Ordner `epaper433` öffnen (`epaper433.ino`)
2. Einstellungen in `config.h` prüfen
3. Kompilieren und hochladen
4. Serial Monitor öffnen (bei `SERIAL_DEBUG 1`)

---

## Konfiguration (`config.h`)

```cpp
// GPIO
#define TX_PIN                25
#define DS18B20_PIN           21
#define BATTERY_ADC_PIN       35      // -1 = keine Batteriemessung
#define BATTERY_DIVIDER_RATIO 2.0f
#define BATTERY_FULL_V        4.15f   // ~100 %
#define BATTERY_EMPTY_V       3.30f   // ~0 %

#define EPAPER_CS_PIN          5
#define EPAPER_DC_PIN         17
#define EPAPER_RST_PIN        16
#define EPAPER_BUSY_PIN        4
#define EPAPER_SCK_PIN        18
#define EPAPER_MOSI_PIN       23
#define EPAPER_ENABLE          1      // 0 = Display aus
#define EPAPER_ROTATION        1
#define EPAPER_UPDATE_TEMP_C   0.1f   // Refresh überspringen bei kleinerer Änderung
#define EPAPER_UPDATE_PERCENT  1

#define BUTTON_PIN            39
#define BUTTON_WAKE_LEVEL      0      // 0 = gedrückt = GND
#define BUTTON_DEBOUNCE_MS    50

// WT450
#define TX_INVERT              0
#define WT450_HOUSE_CODE       1      // 1..15
#define WT450_CHANNEL          2      // 1..4 (humidity = Batterie-%)

// Timing
#define SEND_INTERVAL_MS   60000
#define SENSOR_READ_RETRIES    3
#define SENSOR_RETRY_MS      200
#define SENSOR_RETRY_SLEEP_MS 5000
#define DS18B20_CONVERT_MS   850
#define DS18B20_CRC_RETRIES    4

// Power / Debug
#define ESP32_DEEP_SLEEP       1      // 0=Light Sleep, 1=Deep Sleep
#define SERIAL_DEBUG           0
```

Die Batterie-Prozentanzeige nutzt `batteryPercentFromVoltage()` (linear zwischen `BATTERY_EMPTY_V` und `BATTERY_FULL_V`). WT450-Humidity ist auf **0–99** begrenzt.

---

## Betriebsablauf

```
Kaltstart → Splash → Hibernate → Sleep
    ↓
Timer- oder Tasten-Wakeup
    → DS18B20 lesen
    → ePaper aktualisieren bei Änderung (oder „Error“)
    → WT450 senden (Temp + Batterie-% als humidity)
    → Hibernate / Deep Sleep bis nächster Timer oder Taste
```

Jeder erfolgreiche Zyklus sendet Temperatur und Batterieladung (0–99 %) im humidity-Feld. Es gibt keinen separaten Batterie-Kanal.

### Taste (GPIO 39)

- Wakeup aus Light Sleep und Deep Sleep (`ext0`)
- Danach: messen → anzeigen → senden → schlafen
- Vor dem Sleep: Warten bis Taste losgelassen (Entprellung)

GPIO 39 hat keinen internen Pull-up; das T5-Board bringt einen externen mit.

### Sensorfehler auf dem ePaper

Fehlt der DS18B20 oder schlägt das Lesen fehl: Anzeige **"Error"** plus Batteriespannung/%. Kein Funkversand in dem Fall.

---

## Energiesparmodus

| Modus | `ESP32_DEEP_SLEEP` | Erwartung (T5) | Einsatz |
|-------|--------------------|----------------|---------|
| Light Sleep | `0` | ~0,8 mA+ | USB, Entwicklung |
| Deep Sleep | `1` | typ. 0,5–0,9 mA am T5 | Batterie, **ohne USB** |

Der nackte ESP32 braucht im Deep Sleep nur ~10–150 µA. Am **LilyGO T5 V2.3.1** sind **500–900 µA** am BAT-Anschluss üblich. Gut optimiert: ~200–400 µA; unter ~150 µA meist nur mit Hardware-Eingriff.

**Wichtig:** Deep Sleep mit USB führt oft zu einem Bootloop (`rst:0x5 DEEPSLEEP_RESET`). Strommessung nur über BAT, USB abgezogen.

### Was der Sketch vor Deep Sleep macht

1. `display.hibernate()` (SSD1680 Deep Sleep)
2. ePaper-Pins: CS/RST **HIGH**, DC/SCK/MOSI **LOW**
3. TX (GPIO 25) **LOW**
4. `gpio_hold_en` + `gpio_deep_sleep_hold_en` — sonst schweben die Leitungen

Beim Start: `lowPowerReleaseGpioHold()` löst alle Holds.

Zusätzlich: WiFi/BT aus, CPU 80 MHz, ePaper-Refresh nur bei Änderung, eine DS18B20-Konvertierung pro Zyklus.

### Hardware-Tipps (wenn noch ~0,9 mA)

| Maßnahme | Wirkung |
|----------|---------|
| 100 kΩ Pull-up CS (GPIO 5) → 3,3 V | CS bleibt HIGH |
| 100 kΩ Pull-up RST (GPIO 16) → 3,3 V | RST bleibt HIGH |
| ADC-Teiler ≥100 kΩ+100 kΩ (besser 470 k) | 10 k+10 k allein ≈ 200 µA Dauerstrom |
| STX882-Versorgung abschaltbar (P-MOSFET) | Sender-Ruhestrom weg |
| Display testweise abklemmen | Referenz: oft ~100–150 µA ohne Panel |

Wakeup-Quellen: **Timer** (`SEND_INTERVAL_MS`) und **Taste** (GPIO 39).

---

## Serial-Debug (Beispiel)

Aktivieren mit `#define SERIAL_DEBUG 1`.

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

### Sensorfehler

| Meldung | Bedeutung |
|---------|-----------|
| `(no device)` | Sensor/Pull-up/Verdrahtung |
| `(crc)` | gestörte 1-Wire-Übertragung — Bus wird neu initialisiert |
| `(invalid)` | Temperatur außerhalb −55…125 °C |
| `(read fail)` | sonstiger Lesefehler |

---

## Empfang mit rtl_433

```bash
rtl_433 -f 433.92M -s 250k
```

Hauscode und Kanal müssen zu `WT450_HOUSE_CODE` / `WT450_CHANNEL` passen. Bei keinem Empfang ggf. `TX_INVERT 1` testen. Batterie-% erscheint als Feuchtewert.

---

## Projektstruktur

```
epaper433/
├── epaper433.ino         Hauptprogramm
├── config.h              GPIO, Timing, Power, Display, Batterie, Taste
├── DisplayEpaper.h/cpp   ePaper (Temp, Volt, %, Splash, Error)
├── Ds18b20OneWire.h/cpp  DS18B20 (OneWireESP32 / RMT)
├── Wt450Send.h/cpp       WT450-Funkprotokoll
├── LowPowerSleep.h/cpp   Light/Deep Sleep + Taste + GPIO-Hold
├── epaper433_logo.h      Splash-Logo (Bitmap)
├── assets/               Logo-Quellbilder
├── Dbg.h                 Debug-Makros
├── LICENSE               GPL-3.0
├── README.md             English
└── README_DE.md          Deutsch
```

---

## Fehlerbehebung

| Problem | Lösung |
|---------|--------|
| Bootloop `DEEPSLEEP_RESET` | USB abziehen oder `ESP32_DEEP_SLEEP 0` |
| Deep Sleep ~0,9 mA | Pin-Holds? CS/RST mit 100 kΩ pull-up; ADC-Teiler; USB ab |
| Taste weckt nicht | `BUTTON_PIN 39`, `BUTTON_WAKE_LEVEL 0` |
| ePaper bleibt weiß | GxEPD2 + Adafruit GFX; Panel `GxEPD2_213_BN` |
| Display/BUSY hängt | Sender **nicht** an GPIO 4 — `TX_PIN=25` |
| DS18B20 fehlt / `(crc)` | 4,7 kΩ Pull-up, GPIO 21, Library `esp32-ds18b20`, Serial `(RMT)` |
| Kein Funkempfang | Antenne, `TX_PIN`, ggf. `TX_INVERT`, Hauscode/Kanal |
| Batteriewert falsch | `BATTERY_DIVIDER_RATIO` anpassen; nur ADC-Pins 32–39 |
| Batterie-% ungenau | `BATTERY_FULL_V` / `BATTERY_EMPTY_V` anpassen |

---

## Lizenz

[GPL-3.0](LICENSE)

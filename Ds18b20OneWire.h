#ifndef DS18B20_ONEWIRE_H
#define DS18B20_ONEWIRE_H

#include <Arduino.h>
#include <OneWireESP32.h>
#include "config.h"

// DS18B20 via OneWireESP32 (RMT)

class Ds18b20OneWire {
public:
  enum Error : int {
    OK = 0,
    NO_DEVICE = 1,
    READ_FAIL = 2,
    CRC_FAIL = 3,
    INVALID = 4,
  };

  explicit Ds18b20OneWire(uint8_t dataPin);
  ~Ds18b20OneWire();

  void begin();
  void releaseBus();
  void reinitBus();  // recreate RMT driver after sleep
  bool init();
  bool readTemperatureC(float &tempC, int &errorCode);

private:
  uint8_t dataPin_;
  OneWire32 *bus_;
  uint64_t address_;
  bool hasAddress_;

  bool ensureBus();
  bool ensureAddress();
};

#endif

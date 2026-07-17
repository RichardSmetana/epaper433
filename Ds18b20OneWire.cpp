#include "Ds18b20OneWire.h"

Ds18b20OneWire::Ds18b20OneWire(uint8_t dataPin)
  : dataPin_(dataPin),
    bus_(nullptr),
    address_(0),
    hasAddress_(false) {}

Ds18b20OneWire::~Ds18b20OneWire() {
  if (bus_) {
    delete bus_;
    bus_ = nullptr;
  }
}

static int mapRmtError(uint8_t err) {
  // OneWireESP32: 0=OK, 1=CRC, 2=BAD, 3=TIMEOUT/DC, 4=DRV
  switch (err) {
    case 0: return Ds18b20OneWire::OK;
    case 1: return Ds18b20OneWire::CRC_FAIL;
    case 2: return Ds18b20OneWire::READ_FAIL;
    case 3: return Ds18b20OneWire::NO_DEVICE;
    default: return Ds18b20OneWire::READ_FAIL;
  }
}

void Ds18b20OneWire::begin() {
  ensureBus();
}

void Ds18b20OneWire::releaseBus() {
}

void Ds18b20OneWire::reinitBus() {
  if (bus_) {
    delete bus_;
    bus_ = nullptr;
  }
  hasAddress_ = false;
  address_ = 0;
  ensureBus();
}

bool Ds18b20OneWire::ensureBus() {
  if (!bus_) {
    bus_ = new OneWire32(dataPin_);
  }
  return bus_ != nullptr;
}

bool Ds18b20OneWire::ensureAddress() {
  if (!ensureBus()) {
    return false;
  }
  if (hasAddress_ && address_ != 0) {
    return true;
  }

  uint64_t foundAddr[1] = {0};
  if (bus_->search(foundAddr, 1) == 0 || foundAddr[0] == 0) {
    hasAddress_ = false;
    address_ = 0;
    return false;
  }

  address_ = foundAddr[0];
  hasAddress_ = true;
  return true;
}

bool Ds18b20OneWire::init() {
  reinitBus();
  delay(50);
  return ensureAddress();
}

bool Ds18b20OneWire::readTemperatureC(float &tempC, int &errorCode) {
  if (!ensureBus()) {
    errorCode = READ_FAIL;
    return false;
  }
  if (!ensureAddress()) {
    errorCode = NO_DEVICE;
    return false;
  }

  bus_->request();
  delay(DS18B20_CONVERT_MS);

  uint8_t lastErr = 1;
  for (uint8_t i = 0; i < DS18B20_CRC_RETRIES; i++) {
    lastErr = bus_->getTemp(address_, tempC);
    if (lastErr == 0) {
      break;
    }
    if (lastErr == 3) {
      delay(20);
      continue;
    }
    delay(10);
  }

  if (lastErr != 0) {
    reinitBus();
    delay(20);
    if (!ensureAddress()) {
      errorCode = NO_DEVICE;
      return false;
    }
    bus_->request();
    delay(DS18B20_CONVERT_MS);
    for (uint8_t i = 0; i < DS18B20_CRC_RETRIES; i++) {
      lastErr = bus_->getTemp(address_, tempC);
      if (lastErr == 0) {
        break;
      }
      delay(15);
    }
  }

  errorCode = mapRmtError(lastErr);
  if (errorCode != OK) {
    return false;
  }

  if (tempC < -55.0f || tempC > 125.0f) {
    errorCode = INVALID;
    return false;
  }

  return true;
}

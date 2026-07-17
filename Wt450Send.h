/*
 * WT450 / WT450H Sender (433 MHz OOK)
 * Protocol: rtl_433 #33 (OOK pulse DMC)
 */

#ifndef WT450_SEND_H
#define WT450_SEND_H

#include <Arduino.h>

struct Wt450Data {
  uint8_t house;      // 1..15
  uint8_t channel;    // 1..4
  float   tempC;
  uint8_t humidity;   // 0..127
  bool    batteryOk;  // true = OK
};

class Wt450Sender {
public:
  explicit Wt450Sender(uint8_t txPin = 3);

  void begin(bool invert = false);
  void setInvert(bool invert);
  bool send(const Wt450Data &data);
  bool buildPreview(const Wt450Data &data, uint8_t seq, uint8_t out[5]) const;

  // DMC: pulse count = zeros + 2*ones (same formula as original sensor).
  static uint8_t expectedPulseCount(const uint8_t pkt[5]);

private:
  uint8_t txPin_;
  uint8_t pulseState_;
  bool    invert_;

  static void delayUs(uint32_t us);
  void pinWrite(uint8_t level) const;
  void warmupDelays();
  void sendFrameSync();
  void sendFrameEnd();
  void sendBit(uint8_t bit);
  void sendFrame(const uint8_t pkt[5]);
  static bool buildPacket(const Wt450Data &data, uint8_t seq, uint8_t out[5]);
};

#endif

#include "Wt450Send.h"
#include <math.h>

/*
 * Timings from working WT450-TH capture:
 *   short ~976 us, long ~1953 us, sync ~576 us (sync not counted in pulse list)
 *
 * Pulse count per frame = zeros + 2*ones (DMC), typically 50..58 for 36 bits.
 * Example: seq 1 @ 21.87 C / 63 % -> 20 ones -> 56 pulses (same as original).
 *
 * 3 separate OOK packets with ~19 ms silence between them.
 */
static const uint8_t  PACKET_BITS    = 36;
static const uint16_t SHORT_US       = 926;
static const uint16_t LONG_US        = 1867;
static const uint16_t SYNC_US        = 576;
static const uint16_t BURST_RESET_US = 19800;

Wt450Sender::Wt450Sender(uint8_t txPin)
  : txPin_(txPin), pulseState_(HIGH), invert_(false) {}

void Wt450Sender::begin(bool invert) {
  invert_ = invert;
  pinMode(txPin_, OUTPUT);
  digitalWrite(txPin_, invert_ ? HIGH : LOW);
}

void Wt450Sender::setInvert(bool invert) {
  invert_ = invert;
  digitalWrite(txPin_, invert_ ? HIGH : LOW);
}

uint8_t Wt450Sender::expectedPulseCount(const uint8_t pkt[5]) {
  uint8_t ones = 0;
  for (uint8_t i = 0; i < PACKET_BITS; i++) {
    if ((pkt[i >> 3] >> (7 - (i & 7))) & 1) {
      ones++;
    }
  }
  return (PACKET_BITS - ones) + (2 * ones);
}

void Wt450Sender::delayUs(uint32_t us) {
  while (us > 16383UL) {
    delayMicroseconds(16383);
    us -= 16383UL;
  }
  if (us > 0) {
    delayMicroseconds((uint16_t)us);
  }
}

void Wt450Sender::pinWrite(uint8_t level) const {
  if (invert_) {
    level = level ? LOW : HIGH;
  }
  digitalWrite(txPin_, level);
}

static void setPacketBit(uint8_t *buf, uint8_t idx, uint8_t v) {
  uint8_t mask = (uint8_t)(1 << (7 - (idx & 7)));
  if (v) {
    buf[idx >> 3] |= mask;
  } else {
    buf[idx >> 3] &= ~mask;
  }
}

static bool verifyParity(const uint8_t *b) {
  uint8_t p = 0;
  for (uint8_t i = 0; i < 5; i++) {
    p ^= b[i];
  }
  p ^= (p >> 4);
  p ^= (p >> 2);
  return (p & 0x03) == 0;
}

bool Wt450Sender::buildPacket(const Wt450Data &p, uint8_t seq, uint8_t out[5]) {
  if (p.house < 1 || p.house > 15 || p.channel < 1 || p.channel > 4 || p.humidity > 99) {
    return false;
  }

  memset(out, 0, 5);

  setPacketBit(out, 0, 1);
  setPacketBit(out, 1, 1);
  setPacketBit(out, 2, 0);
  setPacketBit(out, 3, 0);

  for (uint8_t i = 0; i < 4; i++) {
    setPacketBit(out, 4 + i, (p.house >> (3 - i)) & 1);
  }

  uint8_t ch = p.channel - 1;
  setPacketBit(out, 8, (ch >> 1) & 1);
  setPacketBit(out, 9, ch & 1);

  setPacketBit(out, 10, 1);
  setPacketBit(out, 11, 1);
  setPacketBit(out, 12, p.batteryOk ? 0 : 1);

  for (uint8_t i = 0; i < 7; i++) {
    setPacketBit(out, 13 + i, (p.humidity >> (6 - i)) & 1);
  }

  int16_t tempWhole = (int16_t)floorf(p.tempC + 50.0f);
  if (tempWhole < 0) {
    tempWhole = 0;
  }
  if (tempWhole > 255) {
    tempWhole = 255;
  }
  float fracPart = p.tempC - floorf(p.tempC);
  uint8_t tempFrac = (uint8_t)(fracPart * 16.0f + 0.5f) & 0x0F;
  uint16_t temp12 = ((uint16_t)tempWhole << 4) | tempFrac;
  for (uint8_t i = 0; i < 12; i++) {
    setPacketBit(out, 20 + i, (temp12 >> (11 - i)) & 1);
  }

  setPacketBit(out, 32, (seq >> 1) & 1);
  setPacketBit(out, 33, seq & 1);

  for (uint8_t b34 = 0; b34 < 2; b34++) {
    for (uint8_t b35 = 0; b35 < 2; b35++) {
      setPacketBit(out, 34, b34);
      setPacketBit(out, 35, b35);
      if (verifyParity(out)) {
        return true;
      }
    }
  }
  return false;
}

bool Wt450Sender::buildPreview(const Wt450Data &data, uint8_t seq, uint8_t out[5]) const {
  return buildPacket(data, seq, out);
}

void Wt450Sender::warmupDelays() {
  // First delayMicroseconds() calls can run long; no RF emitted.
  for (uint8_t i = 0; i < 4; i++) {
    delayUs(SHORT_US);
    delayUs(LONG_US);
  }
}

void Wt450Sender::sendFrameSync() {
  pinWrite(LOW);
  delayUs(SYNC_US);
  pulseState_ = HIGH;
}

void Wt450Sender::sendFrameEnd() {
  // Original ends on full DMC pulses; half-bit tail sits in the 19 ms reset gap.
  if (pulseState_ == HIGH) {
    pinWrite(HIGH);
    delayUs(SHORT_US / 2);
  }
  pinWrite(LOW);
}

void Wt450Sender::sendBit(uint8_t bit) {
  if (bit == 0) {
    pinWrite(pulseState_);
    delayUs(LONG_US);
    pulseState_ = (pulseState_ == HIGH) ? LOW : HIGH;
    return;
  }
  pinWrite(pulseState_);
  delayUs(SHORT_US);
  pinWrite(pulseState_ == HIGH ? LOW : HIGH);
  delayUs(SHORT_US);
}

void Wt450Sender::sendFrame(const uint8_t pkt[5]) {
  sendFrameSync();
  for (uint8_t i = 0; i < PACKET_BITS; i++) {
    sendBit((pkt[i >> 3] >> (7 - (i & 7))) & 1);
  }
  sendFrameEnd();
}

bool Wt450Sender::send(const Wt450Data &data) {
  uint8_t sent = 0;

  warmupDelays();

  for (uint8_t seq = 0; seq < 3; seq++) {
    uint8_t pkt[5];
    if (!buildPacket(data, seq, pkt)) {
      continue;
    }

    sendFrame(pkt);
    sent++;

    pinWrite(LOW);
    delayUs(BURST_RESET_US);
    pulseState_ = HIGH;
  }

  return sent > 0;
}

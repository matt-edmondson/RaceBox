// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "UbxParser.hpp"

#include <algorithm>

namespace ktsu { namespace racebox { namespace ble {
namespace {

constexpr uint8_t kSync1 = 0xB5;
constexpr uint8_t kSync2 = 0x62;
// sync(2) + class(1) + id(1) + length(2) + checksum(2)
constexpr size_t kFrameOverhead = 8;
constexpr size_t kHeaderLen = 6;

inline uint16_t rdU16(const uint8_t* p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}
inline int16_t rdI16(const uint8_t* p) { return static_cast<int16_t>(rdU16(p)); }
inline uint32_t rdU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline int32_t rdI32(const uint8_t* p) { return static_cast<int32_t>(rdU32(p)); }

} // namespace

UbxParser::UbxParser() { buffer_.reserve(256); }

void UbxParser::computeChecksum(const uint8_t* from, size_t len, uint8_t& ckA, uint8_t& ckB) {
  uint8_t a = 0, b = 0;
  for (size_t i = 0; i < len; ++i) {
    a = static_cast<uint8_t>(a + from[i]);
    b = static_cast<uint8_t>(b + a);
  }
  ckA = a;
  ckB = b;
}

void UbxParser::reset() { buffer_.clear(); }

std::vector<uint8_t> UbxParser::buildFrame(uint8_t msgClass, uint8_t msgId,
                                           const uint8_t* payload, uint16_t payloadLen) {
  if (payloadLen > kMaxPayloadLen) return {};
  if (!payload) payloadLen = 0;

  std::vector<uint8_t> frame;
  frame.reserve(static_cast<size_t>(payloadLen) + kFrameOverhead);
  frame.push_back(kSync1);
  frame.push_back(kSync2);
  frame.push_back(msgClass);
  frame.push_back(msgId);
  frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
  frame.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
  if (payloadLen) frame.insert(frame.end(), payload, payload + payloadLen);

  // The checksum covers everything from the class byte to the end of the
  // payload -- the sync word is excluded.
  uint8_t ckA = 0, ckB = 0;
  computeChecksum(frame.data() + 2, frame.size() - 2, ckA, ckB);
  frame.push_back(ckA);
  frame.push_back(ckB);
  return frame;
}

void UbxParser::append(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;

  // Never let a corrupt length field turn the stream buffer into a memory leak.
  if (buffer_.size() + len > kMaxBufferedBytes) {
    buffer_.clear();
    ++overflows_;
    // A single notification larger than the cap is itself nonsense; drop it.
    if (len > kMaxBufferedBytes) return;
  }

  buffer_.insert(buffer_.end(), data, data + len);
  drain();
}

void UbxParser::drain() {
  for (;;) {
    // 1. Resynchronise: discard anything before the sync word.
    if (buffer_.size() < 2) return;
    const uint8_t sync[2] = {kSync1, kSync2};
    auto it = std::search(buffer_.begin(), buffer_.end(), std::begin(sync), std::end(sync));
    if (it == buffer_.end()) {
      // No sync word present. Keep a trailing lone 0xB5, which may be the first
      // half of a sync word split across notifications.
      const bool danglingSync = buffer_.back() == kSync1;
      buffer_.clear();
      if (danglingSync) buffer_.push_back(kSync1);
      ++resyncs_;
      return;
    }
    if (it != buffer_.begin()) {
      buffer_.erase(buffer_.begin(), it);
      ++resyncs_;
    }

    // 2. Need the full header before the length is readable.
    if (buffer_.size() < kFrameOverhead) return;

    const uint8_t msgClass = buffer_[2];
    const uint8_t msgId = buffer_[3];
    const uint16_t payloadLen = rdU16(&buffer_[4]);

    // 3. An implausible length means we synced on payload bytes that happened to
    //    look like a sync word. Step past it and rescan rather than stalling.
    if (payloadLen > kMaxPayloadLen) {
      buffer_.erase(buffer_.begin(), buffer_.begin() + 2);
      ++resyncs_;
      continue;
    }

    const size_t frameLen = kHeaderLen + payloadLen + 2;
    if (buffer_.size() < frameLen) return; // incomplete; wait for more notifications

    // 4. Validate. The checksum covers class, id, length and payload.
    uint8_t ckA = 0, ckB = 0;
    computeChecksum(&buffer_[2], 4 + payloadLen, ckA, ckB);
    const bool checksumOk =
        buffer_[kHeaderLen + payloadLen] == ckA && buffer_[kHeaderLen + payloadLen + 1] == ckB;

    if (!checksumOk) {
      // Consume the frame to resync, but never hand a corrupt payload onward.
      ++checksumErrors_;
      buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<long>(frameLen));
      continue;
    }

    // 5. Decode telemetry; hand everything else to the message sink, which is
    //    where acknowledgements and command replies are picked up.
    if (msgClass == kRaceboxClass && msgId == kRaceboxDataId) {
      // A data message too short to decode is dropped rather than reported as a
      // command reply.
      if (payloadLen >= kRaceboxDataPayloadLen) {
        ++framesDecoded_;
        if (sink_) sink_(decodeRaceboxPayload(&buffer_[kHeaderLen]));
      }
    } else {
      ++otherFrames_;
      if (messageSink_) {
        messageSink_(msgClass, msgId, payloadLen ? &buffer_[kHeaderLen] : nullptr, payloadLen);
      }
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<long>(frameLen));
  }
}

RaceboxData UbxParser::decodeRaceboxPayload(const uint8_t* p) {
  RaceboxData d{};

  d.iTowMs = rdU32(p + 0);
  d.year = rdU16(p + 4);
  d.month = p[6];
  d.day = p[7];
  d.hour = p[8];
  d.minute = p[9];
  d.second = p[10];
  d.validityFlags = p[11];
  d.timeAccuracyNs = rdU32(p + 12);
  d.nanoseconds = rdI32(p + 16);

  d.fixType = static_cast<FixType>(p[20]);
  d.fixStatusFlags = p[21];
  d.fixValid = (p[21] & 0x01) != 0;
  d.dateTimeFlags = p[22];
  d.satellites = p[23];

  d.longitudeDeg = static_cast<double>(rdI32(p + 24)) * 1e-7;
  d.latitudeDeg = static_cast<double>(rdI32(p + 28)) * 1e-7;
  d.wgsAltitudeM = static_cast<float>(rdI32(p + 32)) / 1000.0f;
  d.mslAltitudeM = static_cast<float>(rdI32(p + 36)) / 1000.0f;
  d.horizontalAccuracyM = static_cast<float>(rdU32(p + 40)) / 1000.0f;
  d.verticalAccuracyM = static_cast<float>(rdU32(p + 44)) / 1000.0f;

  // Wire speed is mm/s; 1 mm/s == 0.0036 km/h.
  d.speedKmh = static_cast<float>(rdI32(p + 48)) * 0.0036f;
  d.headingDeg = static_cast<float>(rdI32(p + 52)) * 1e-5f;
  d.speedAccuracyKmh = static_cast<float>(rdU32(p + 56)) * 0.0036f;
  d.headingAccuracyDeg = static_cast<float>(rdU32(p + 60)) * 1e-5f;
  d.pdop = static_cast<float>(rdU16(p + 64)) * 0.01f;

  d.batteryRaw = p[67];
  d.batteryPercent = static_cast<uint8_t>(p[67] & 0x7F);
  d.charging = (p[67] & 0x80) != 0;
  d.inputVoltage = static_cast<float>(p[67]) / 10.0f;

  // Inertial values are milli-g and centi-degrees/second on the wire.
  d.gForceX = static_cast<float>(rdI16(p + 68)) / 1000.0f;
  d.gForceY = static_cast<float>(rdI16(p + 70)) / 1000.0f;
  d.gForceZ = static_cast<float>(rdI16(p + 72)) / 1000.0f;
  d.rotationRateX = static_cast<float>(rdI16(p + 74)) / 100.0f;
  d.rotationRateY = static_cast<float>(rdI16(p + 76)) / 100.0f;
  d.rotationRateZ = static_cast<float>(rdI16(p + 78)) / 100.0f;

  return d;
}

} } } // namespaces

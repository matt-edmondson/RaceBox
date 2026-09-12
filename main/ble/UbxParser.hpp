// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "RaceBoxMessages.hpp"
#include "RaceboxData.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ktsu { namespace racebox { namespace ble {

// Streaming UBX frame reassembler and RaceBox data-message decoder.
//
// BLE notifications deliver an arbitrary byte stream: a single UBX packet may be
// split across notifications, and several may arrive in one. This class buffers
// the stream, resynchronises on the 0xB5 0x62 sync word, validates the Fletcher
// checksum, and emits decoded telemetry via the sink.
//
// Deliberately free of any ESP-IDF dependency so it can be unit tested on a host
// compiler -- see test/host/test_ubx_parser.cpp.
class UbxParser {
 public:
  using Sink = std::function<void(const RaceboxData&)>;
  // Every other checksum-valid frame -- acknowledgements and command replies --
  // is handed over raw. The payload pointer is only valid for the duration of
  // the call, and the callee must not feed the parser from inside it.
  using MessageSink = std::function<void(uint8_t msgClass, uint8_t msgId, const uint8_t* payload,
                                        size_t payloadLen)>;

  // A UBX payload larger than this is treated as a corrupt length field. RaceBox
  // data messages carry 80 bytes; the headroom covers other message classes.
  static constexpr size_t kMaxPayloadLen = 512;
  // Hard cap on retained stream bytes. Without this a single corrupt length
  // field would make the parser wait forever while notifications accumulate.
  static constexpr size_t kMaxBufferedBytes = 2048;

  // RaceBox data message identity (see RaceBoxMessages.hpp for the full set).
  static constexpr uint8_t kRaceboxClass = kRaceboxMsgClass;
  static constexpr uint8_t kRaceboxDataId = kMsgIdData;
  static constexpr size_t kRaceboxDataPayloadLen = 80;

  UbxParser();

  void setSink(Sink sink) { sink_ = std::move(sink); }
  void setMessageSink(MessageSink sink) { messageSink_ = std::move(sink); }

  // Feed bytes from a BLE notification. Complete, checksum-valid RaceBox data
  // messages are decoded and passed to the sink before this returns.
  void append(const uint8_t* data, size_t len);

  // Drop all buffered stream state (call on disconnect).
  void reset();

  // --- Diagnostics ---
  size_t buffered() const { return buffer_.size(); }
  uint32_t framesDecoded() const { return framesDecoded_; }
  uint32_t otherFrames() const { return otherFrames_; }
  uint32_t checksumErrors() const { return checksumErrors_; }
  uint32_t overflows() const { return overflows_; }
  uint32_t resyncs() const { return resyncs_; }

  // Exposed for reuse by the transmit path, which must frame outbound packets
  // with the same checksum algorithm.
  static void computeChecksum(const uint8_t* from, size_t len, uint8_t& ckA, uint8_t& ckB);

  // Frame an outbound UBX packet: sync word, class, id, little-endian length,
  // payload and Fletcher checksum. Returns an empty vector when payloadLen
  // exceeds kMaxPayloadLen, so callers cannot transmit a length field this
  // parser would itself reject as corrupt. Lives here rather than in the BLE
  // client so the framing is covered by the host tests.
  static std::vector<uint8_t> buildFrame(uint8_t msgClass, uint8_t msgId, const uint8_t* payload,
                                         uint16_t payloadLen);

  // Decode an 80-byte RaceBox data-message payload. Public for unit testing.
  static RaceboxData decodeRaceboxPayload(const uint8_t* payload);

 private:
  void drain();

  std::vector<uint8_t> buffer_;
  Sink sink_;
  MessageSink messageSink_;
  uint32_t framesDecoded_ = 0;
  uint32_t otherFrames_ = 0;
  uint32_t checksumErrors_ = 0;
  uint32_t overflows_ = 0;
  uint32_t resyncs_ = 0;
};

} } } // namespaces

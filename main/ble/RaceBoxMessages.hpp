// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Message identities and payload codecs for the RaceBox-specific UBX messages,
// per the RaceBox BLE Protocol Documentation (revision 9).
//
// RaceBox does NOT use the standard UBX ACK class (0x05): acknowledgements come
// back as 0xFF 0x02 (ACK) and 0xFF 0x03 (NACK), each carrying the class and id
// of the message being answered.
//
// Deliberately free of any ESP-IDF dependency so the framing and decoding are
// unit tested on the host -- see test/host/TestRaceBoxMessages.cpp.
namespace ktsu { namespace racebox { namespace ble {

// All RaceBox messages share the UBX class 0xFF.
constexpr uint8_t kRaceboxMsgClass = 0xFF;

constexpr uint8_t kMsgIdData = 0x01;        // live telemetry (80-byte payload)
constexpr uint8_t kMsgIdAck = 0x02;         // command accepted
constexpr uint8_t kMsgIdNack = 0x03;        // command rejected or unsupported
constexpr uint8_t kMsgIdGnssConfig = 0x27;  // GNSS receiver configuration

// Which RaceBox is on the other end. It matters for byte 67 of the telemetry
// payload, which the protocol documentation defines differently per model:
// battery charge on a Mini and Mini S, input voltage on a Micro.
enum class DeviceModel : uint8_t { Unknown, Mini, MiniS, Micro };

// Identify the model from the advertised device name ("RaceBox Mini 1234567",
// "RaceBox Mini S 1234567", "RaceBox Micro 1234567"). Matching is
// case-insensitive, and "Mini S" is tested before "Mini" so the longer name is
// not swallowed by the shorter one.
DeviceModel deviceModelFromName(const char* name);

const char* toString(DeviceModel model);

// Byte 67 of the telemetry payload, interpreted per model.
// Percent and charging apply to a Mini and Mini S; volts to a Micro. Each is
// nonsense on the other model, so callers pick with the model in hand.
inline uint8_t batteryPercentFromRaw(uint8_t raw) { return static_cast<uint8_t>(raw & 0x7F); }
inline bool chargingFromRaw(uint8_t raw) { return (raw & 0x80) != 0; }
inline float inputVoltsFromRaw(uint8_t raw) { return static_cast<float>(raw) / 10.0f; }

// True when byte 67 should be read as a battery percentage rather than volts.
// An unidentified device is treated as a Mini: that is what this firmware
// targets, and a peer matched only by its service UUID advertises no name.
inline bool reportsBatteryPercent(DeviceModel model) { return model != DeviceModel::Micro; }

// Answer to a command we sent: which message it refers to, and whether the
// device accepted it. A NACK also means "unsupported" -- the GNSS configuration
// message needs device firmware 3.3 or later.
struct Acknowledgement {
  uint8_t msgClass = 0;
  uint8_t msgId = 0;
  bool accepted = false;
};

// GNSS receiver configuration (0xFF 0x27), 3 bytes on the wire.
struct GnssConfig {
  // u-blox CFG-NAVSPG-DYNMODEL. The protocol documentation recommends 4 for
  // ground-based use, 5 for sea, 6 for low-dynamics airborne, 8 above 300 kph.
  // Valid range is 0 to 8; the device NACKs anything else.
  uint8_t dynamicModel = 0;
  bool enable3dSpeed = false;
  uint8_t minHorizontalAccuracyM = 0;
};

constexpr uint8_t kMaxDynamicModel = 8;

// Decode an ACK/NACK payload. msgId selects which of the two it is; anything
// else, or a payload that is not the documented 2 bytes, is rejected.
bool decodeAcknowledgement(uint8_t msgId, const uint8_t* payload, size_t len, Acknowledgement& out);

// Decode the 3-byte GNSS configuration payload the device sends in reply to a
// query. Shorter payloads are rejected; longer ones are read from the front, so
// a firmware that appends fields stays readable.
bool decodeGnssConfig(const uint8_t* payload, size_t len, GnssConfig& out);

// Frame a GNSS configuration *query*: the same class and id with an empty
// payload. This only reads -- the device answers with a 3-byte payload of its
// own rather than an ACK.
std::vector<uint8_t> buildGnssConfigQuery();

// Frame a GNSS configuration *update*. Returns an empty vector for a dynamic
// model the device would reject, so an invalid value never reaches the wire.
// Nothing in the firmware sends this yet -- it is the documented counterpart to
// the query above, and is covered by the host tests.
std::vector<uint8_t> buildGnssConfigUpdate(const GnssConfig& config);

} } } // namespaces

// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "RaceBoxMessages.hpp"

#include "UbxParser.hpp"

#include <cctype>
#include <cstring>

namespace ktsu { namespace racebox { namespace ble {

namespace {

// Case-insensitive "does `haystack` contain `needle`", with no locale or
// allocation involved.
bool containsNoCase(const char* haystack, const char* needle) {
  const size_t n = strlen(needle);
  if (n == 0) return true;
  for (const char* p = haystack; *p; ++p) {
    size_t i = 0;
    while (i < n && p[i] &&
           std::tolower(static_cast<unsigned char>(p[i])) ==
               std::tolower(static_cast<unsigned char>(needle[i]))) {
      ++i;
    }
    if (i == n) return true;
  }
  return false;
}

// ACK and NACK both carry the class and id of the message they answer.
constexpr size_t kAckPayloadLen = 2;
constexpr size_t kGnssConfigPayloadLen = 3;
} // namespace

DeviceModel deviceModelFromName(const char* name) {
  if (!name || !*name) return DeviceModel::Unknown;
  // Order matters: "Mini S" also contains "Mini".
  if (containsNoCase(name, "Mini S")) return DeviceModel::MiniS;
  if (containsNoCase(name, "Micro")) return DeviceModel::Micro;
  if (containsNoCase(name, "Mini")) return DeviceModel::Mini;
  return DeviceModel::Unknown;
}

const char* toString(DeviceModel model) {
  switch (model) {
    case DeviceModel::Mini: return "Mini";
    case DeviceModel::MiniS: return "Mini S";
    case DeviceModel::Micro: return "Micro";
    case DeviceModel::Unknown: break;
  }
  return "unknown";
}

bool decodeAcknowledgement(uint8_t msgId, const uint8_t* payload, size_t len,
                           Acknowledgement& out) {
  if (msgId != kMsgIdAck && msgId != kMsgIdNack) return false;
  if (!payload || len < kAckPayloadLen) return false;

  out.msgClass = payload[0];
  out.msgId = payload[1];
  out.accepted = (msgId == kMsgIdAck);
  return true;
}

bool decodeGnssConfig(const uint8_t* payload, size_t len, GnssConfig& out) {
  if (!payload || len < kGnssConfigPayloadLen) return false;

  out.dynamicModel = payload[0];
  out.enable3dSpeed = payload[1] != 0;
  out.minHorizontalAccuracyM = payload[2];
  return true;
}

std::vector<uint8_t> buildGnssConfigQuery() {
  return UbxParser::buildFrame(kRaceboxMsgClass, kMsgIdGnssConfig, nullptr, 0);
}

std::vector<uint8_t> buildGnssConfigUpdate(const GnssConfig& config) {
  if (config.dynamicModel > kMaxDynamicModel) return {};

  const uint8_t payload[kGnssConfigPayloadLen] = {
      config.dynamicModel,
      static_cast<uint8_t>(config.enable3dSpeed ? 1 : 0),
      config.minHorizontalAccuracyM,
  };
  return UbxParser::buildFrame(kRaceboxMsgClass, kMsgIdGnssConfig, payload, sizeof(payload));
}

} } } // namespaces

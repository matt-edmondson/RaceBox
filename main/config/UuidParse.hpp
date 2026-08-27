// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cstdint>

namespace ktsu { namespace racebox { namespace config {

// A 128-bit UUID in the byte order NimBLE stores it: little-endian, i.e. the
// reverse of how the UUID is written down.
struct Uuid128Bytes {
  uint8_t bytes[16]{};
};

constexpr uint8_t hexNibble(char c) {
  return (c >= '0' && c <= '9')   ? static_cast<uint8_t>(c - '0')
         : (c >= 'a' && c <= 'f') ? static_cast<uint8_t>(c - 'a' + 10)
         : (c >= 'A' && c <= 'F') ? static_cast<uint8_t>(c - 'A' + 10)
                                  : static_cast<uint8_t>(0);
}

// Convert a canonical UUID string ("6E400001-B5A3-F393-E0A9-E50E24DCCA9E") into
// NimBLE's little-endian byte order, at compile time.
//
// ESP-IDF's NimBLE port exposes ble_uuid_to_str() but no string *parser*, so
// UUIDs have to reach the stack as bytes. Deriving those bytes from the literal
// here keeps one source of truth: there are no hand-written byte tables to drift
// out of step with the documented UUID.
constexpr Uuid128Bytes uuidToNimbleBytes(const char* text) {
  uint8_t bigEndian[16]{};
  int byteIndex = 0;
  int highNibble = -1;

  for (int i = 0; text[i] != '\0' && byteIndex < 16; ++i) {
    if (text[i] == '-') continue;
    const uint8_t value = hexNibble(text[i]);
    if (highNibble < 0) {
      highNibble = value;
    } else {
      bigEndian[byteIndex++] = static_cast<uint8_t>((highNibble << 4) | value);
      highNibble = -1;
    }
  }

  Uuid128Bytes out{};
  for (int i = 0; i < 16; ++i) out.bytes[i] = bigEndian[15 - i];
  return out;
}

} } } // namespaces

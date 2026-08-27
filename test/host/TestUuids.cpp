// Verifies UUID string -> NimBLE little-endian byte conversion.
//
// ESP-IDF's NimBLE has no ble_uuid128_from_str(), so these bytes are what
// actually reaches the stack during service discovery. Getting the byte order
// wrong would not fail to build -- it would simply never find the device.
#include "TestMain.hpp"

#include "../../main/config/BleUuids.hpp"
#include "../../main/config/UuidParse.hpp"

using ktsu::racebox::config::Uuid128Bytes;
using ktsu::racebox::config::uuidToNimbleBytes;

TEST(NordicUartServiceMatchesTheCanonicalNimbleByteOrder) {
  // This is the byte sequence NimBLE's own examples use for the Nordic UART
  // Service, 6E400001-B5A3-F393-E0A9-E50E24DCCA9E.
  const uint8_t expected[16] = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e};
  for (int i = 0; i < 16; ++i) {
    CHECK_EQ(int(ktsu::racebox::ble::kUartServiceBytes.bytes[i]), int(expected[i]));
  }
}

TEST(RxAndTxCharacteristicsDifferOnlyInTheirIdentifierByte) {
  // 6E400002 (write) and 6E400003 (notify) sit at index 12 once reversed.
  CHECK_EQ(int(ktsu::racebox::ble::kUartRxBytes.bytes[12]), 0x02);
  CHECK_EQ(int(ktsu::racebox::ble::kUartTxBytes.bytes[12]), 0x03);
  for (int i = 0; i < 16; ++i) {
    if (i == 12) continue;
    CHECK_EQ(int(ktsu::racebox::ble::kUartRxBytes.bytes[i]),
             int(ktsu::racebox::ble::kUartTxBytes.bytes[i]));
  }
}

TEST(ParserIsCaseInsensitiveAndDashAgnostic) {
  constexpr Uuid128Bytes upper = uuidToNimbleBytes("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  constexpr Uuid128Bytes lower = uuidToNimbleBytes("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
  constexpr Uuid128Bytes nodash = uuidToNimbleBytes("6E400001B5A3F393E0A9E50E24DCCA9E");
  for (int i = 0; i < 16; ++i) {
    CHECK_EQ(int(upper.bytes[i]), int(lower.bytes[i]));
    CHECK_EQ(int(upper.bytes[i]), int(nodash.bytes[i]));
  }
}

TEST(ConversionReversesByteOrder) {
  // An all-distinct UUID makes the reversal obvious.
  constexpr Uuid128Bytes u = uuidToNimbleBytes("000102030405-0607-0809-0A0B-0C0D0E0F");
  for (int i = 0; i < 16; ++i) {
    CHECK_EQ(int(u.bytes[i]), 15 - i);
  }
}

TEST(ConversionHappensAtCompileTime) {
  // If this were not constexpr the static_assert would not compile, so the
  // bytes cost nothing at runtime.
  static_assert(uuidToNimbleBytes("6E400001-B5A3-F393-E0A9-E50E24DCCA9E").bytes[12] == 0x01,
                "UUID conversion must be usable in a constant expression");
  CHECK(true);
}

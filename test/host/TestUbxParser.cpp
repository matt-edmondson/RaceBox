// Unit tests for the UBX stream reassembler and RaceBox data decoder.
#include "TestMain.hpp"

#include "../../main/ble/UbxParser.hpp"

#include <cstring>

using ktsu::racebox::ble::FixType;
using ktsu::racebox::ble::RaceboxData;
using ktsu::racebox::ble::UbxParser;

namespace {

void putU16(std::vector<uint8_t>& v, size_t off, uint16_t x) {
  v[off] = static_cast<uint8_t>(x & 0xFF);
  v[off + 1] = static_cast<uint8_t>((x >> 8) & 0xFF);
}
void putU32(std::vector<uint8_t>& v, size_t off, uint32_t x) {
  v[off] = static_cast<uint8_t>(x & 0xFF);
  v[off + 1] = static_cast<uint8_t>((x >> 8) & 0xFF);
  v[off + 2] = static_cast<uint8_t>((x >> 16) & 0xFF);
  v[off + 3] = static_cast<uint8_t>((x >> 24) & 0xFF);
}
void putI32(std::vector<uint8_t>& v, size_t off, int32_t x) {
  putU32(v, off, static_cast<uint32_t>(x));
}
void putI16(std::vector<uint8_t>& v, size_t off, int16_t x) {
  putU16(v, off, static_cast<uint16_t>(x));
}

// Wrap a payload in a well-formed UBX frame with a correct checksum.
std::vector<uint8_t> frame(uint8_t cls, uint8_t id, const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> f;
  f.push_back(0xB5);
  f.push_back(0x62);
  f.push_back(cls);
  f.push_back(id);
  f.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
  f.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
  f.insert(f.end(), payload.begin(), payload.end());
  uint8_t a = 0, b = 0;
  UbxParser::computeChecksum(&f[2], 4 + payload.size(), a, b);
  f.push_back(a);
  f.push_back(b);
  return f;
}

// A RaceBox data payload with recognisable values in every field we decode.
std::vector<uint8_t> samplePayload() {
  std::vector<uint8_t> p(80, 0);
  putU32(p, 0, 123456789);      // iTOW
  putU16(p, 4, 2025);           // year
  p[6] = 7;                     // month
  p[7] = 14;                    // day
  p[8] = 9;                     // hour
  p[9] = 30;                    // minute
  p[10] = 45;                   // second
  p[11] = 0x07;                 // validity flags
  putU32(p, 12, 25);            // time accuracy ns
  putI32(p, 16, -500);          // nanoseconds
  p[20] = 3;                    // fix type: 3D
  p[21] = 0x01;                 // gnssFixOK
  p[22] = 0x00;                 // date/time flags
  p[23] = 11;                   // satellites
  putI32(p, 24, 1500000000);    // longitude 150.0 deg
  putI32(p, 28, -338000000);    // latitude -33.8 deg
  putI32(p, 32, 105000);        // WGS altitude 105.0 m
  putI32(p, 36, 98500);         // MSL altitude 98.5 m
  putU32(p, 40, 1200);          // horiz accuracy 1.2 m
  putU32(p, 44, 2400);          // vert accuracy 2.4 m
  putI32(p, 48, 27778);         // speed 27778 mm/s == 100.0008 km/h
  putI32(p, 52, 18000000);      // heading 180.0 deg
  putU32(p, 56, 1000);          // speed accuracy 3.6 km/h
  putU32(p, 60, 50000);         // heading accuracy 0.5 deg
  putU16(p, 64, 175);           // PDOP 1.75
  p[66] = 0x00;                 // lat/lon flags
  p[67] = 0x80 | 87;            // charging, 87%
  putI16(p, 68, 1023);          // gX 1.023 g
  putI16(p, 70, -250);          // gY -0.250 g
  putI16(p, 72, 980);           // gZ 0.980 g
  putI16(p, 74, 1500);          // rotX 15.00 deg/s
  putI16(p, 76, -3000);         // rotY -30.00 deg/s
  putI16(p, 78, 45);            // rotZ 0.45 deg/s
  return p;
}

std::vector<uint8_t> sampleFrame() { return frame(0xFF, 0x01, samplePayload()); }

// Collects everything the parser emits.
struct Collector {
  std::vector<RaceboxData> got;
  UbxParser::Sink sink() {
    return [this](const RaceboxData& d) { got.push_back(d); };
  }
};

void feed(UbxParser& p, const std::vector<uint8_t>& bytes) {
  p.append(bytes.data(), bytes.size());
}

} // namespace

TEST(DecodesEveryFieldOfAValidFrame) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());
  feed(parser, sampleFrame());

  CHECK_EQ(c.got.size(), size_t{1});
  if (c.got.empty()) return;
  const RaceboxData& d = c.got[0];

  CHECK_EQ(d.iTowMs, uint32_t{123456789});
  CHECK_EQ(d.year, uint16_t{2025});
  CHECK_EQ(int(d.month), 7);
  CHECK_EQ(int(d.day), 14);
  CHECK_EQ(int(d.hour), 9);
  CHECK_EQ(int(d.minute), 30);
  CHECK_EQ(int(d.second), 45);
  CHECK_EQ(d.nanoseconds, int32_t{-500});
  CHECK(d.fixType == FixType::Fix3D);
  CHECK(d.fixValid);
  CHECK_EQ(int(d.satellites), 11);

  CHECK_NEAR(d.longitudeDeg, 150.0, 1e-9);
  CHECK_NEAR(d.latitudeDeg, -33.8, 1e-9);
  CHECK_NEAR(d.wgsAltitudeM, 105.0, 1e-4);
  CHECK_NEAR(d.mslAltitudeM, 98.5, 1e-4);
  CHECK_NEAR(d.horizontalAccuracyM, 1.2, 1e-4);
  CHECK_NEAR(d.verticalAccuracyM, 2.4, 1e-4);

  CHECK_NEAR(d.speedKmh, 100.0008, 1e-3);
  CHECK_NEAR(d.headingDeg, 180.0, 1e-3);
  CHECK_NEAR(d.speedAccuracyKmh, 3.6, 1e-3);
  CHECK_NEAR(d.headingAccuracyDeg, 0.5, 1e-4);
  CHECK_NEAR(d.pdop, 1.75, 1e-4);

  CHECK_EQ(int(d.batteryPercent), 87);
  CHECK(d.charging);

  CHECK_NEAR(d.gForceX, 1.023, 1e-4);
  CHECK_NEAR(d.gForceY, -0.250, 1e-4);
  CHECK_NEAR(d.gForceZ, 0.980, 1e-4);
  CHECK_NEAR(d.rotationRateX, 15.0, 1e-3);
  CHECK_NEAR(d.rotationRateY, -30.0, 1e-3);
  CHECK_NEAR(d.rotationRateZ, 0.45, 1e-3);

  CHECK_EQ(parser.framesDecoded(), uint32_t{1});
  CHECK_EQ(parser.checksumErrors(), uint32_t{0});
  CHECK_EQ(parser.buffered(), size_t{0});
}

TEST(ReassemblesAFrameSplitAcrossNotifications) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  const std::vector<uint8_t> f = sampleFrame();
  // Split into 20-byte chunks, the way a 23-byte-MTU link would deliver it.
  for (size_t i = 0; i < f.size(); i += 20) {
    const size_t n = std::min<size_t>(20, f.size() - i);
    parser.append(f.data() + i, n);
    if (i + n < f.size()) CHECK_EQ(c.got.size(), size_t{0}); // nothing until complete
  }
  CHECK_EQ(c.got.size(), size_t{1});
  CHECK_EQ(parser.buffered(), size_t{0});
}

TEST(DecodesTwoFramesDeliveredInOneNotification) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  std::vector<uint8_t> two = sampleFrame();
  const std::vector<uint8_t> second = sampleFrame();
  two.insert(two.end(), second.begin(), second.end());
  feed(parser, two);

  CHECK_EQ(c.got.size(), size_t{2});
  CHECK_EQ(parser.framesDecoded(), uint32_t{2});
}

TEST(ResynchronisesPastLeadingGarbage) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  std::vector<uint8_t> stream = {0x00, 0xFF, 0x12, 0x34, 0xB5, 0x00, 0x62};
  const std::vector<uint8_t> f = sampleFrame();
  stream.insert(stream.end(), f.begin(), f.end());
  feed(parser, stream);

  CHECK_EQ(c.got.size(), size_t{1});
  CHECK(parser.resyncs() > 0);
}

TEST(CorruptFrameIsNeverDeliveredToTheSink) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  std::vector<uint8_t> bad = sampleFrame();
  bad[30] ^= 0xFF; // flip a payload bit, invalidating the checksum

  feed(parser, bad);
  CHECK_EQ(c.got.size(), size_t{0});          // the whole point of issue #30
  CHECK_EQ(parser.checksumErrors(), uint32_t{1});
  CHECK_EQ(parser.framesDecoded(), uint32_t{0});

  // ...and the stream still recovers for the next good frame.
  feed(parser, sampleFrame());
  CHECK_EQ(c.got.size(), size_t{1});
}

TEST(BogusLengthFieldDoesNotStallTheStream) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  // Sync word followed by a 60000-byte length that will never arrive.
  std::vector<uint8_t> stream = {0xB5, 0x62, 0xFF, 0x01, 0x60, 0xEA, 0x00, 0x00};
  const std::vector<uint8_t> f = sampleFrame();
  stream.insert(stream.end(), f.begin(), f.end());
  feed(parser, stream);

  // The good frame behind the bogus header must still be decoded.
  CHECK_EQ(c.got.size(), size_t{1});
  CHECK_EQ(parser.buffered(), size_t{0});
}

TEST(BufferStaysBoundedUnderSustainedGarbage) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  // A sync word claiming the largest plausible payload, followed by an endless
  // stream that never completes it cleanly. The buffer must never run away:
  // the frame either completes (and fails its checksum) or we resync past it.
  std::vector<uint8_t> stuck = {0xB5, 0x62, 0xFF, 0x01, 0x00, 0x02};
  feed(parser, stuck);
  const std::vector<uint8_t> filler(200, 0x00);
  for (int i = 0; i < 100; ++i) {
    feed(parser, filler);
    CHECK(parser.buffered() <= UbxParser::kMaxBufferedBytes);
  }

  // ...and a good frame after the flood is still decoded.
  feed(parser, sampleFrame());
  CHECK_EQ(c.got.size(), size_t{1});
}

TEST(OversizedNotificationIsDroppedRatherThanBuffered) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  // A single notification larger than the whole cap is nonsense on a BLE link;
  // it must be discarded outright rather than blowing the buffer.
  const std::vector<uint8_t> huge(UbxParser::kMaxBufferedBytes + 1024, 0xAA);
  feed(parser, huge);
  CHECK_EQ(parser.overflows(), uint32_t{1});
  CHECK(parser.buffered() <= UbxParser::kMaxBufferedBytes);

  feed(parser, sampleFrame());
  CHECK_EQ(c.got.size(), size_t{1});
}

TEST(PreservesADanglingSyncByteBetweenNotifications) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  const std::vector<uint8_t> f = sampleFrame();
  // Deliver only the first sync byte, then the remainder.
  parser.append(f.data(), 1);
  parser.append(f.data() + 1, f.size() - 1);

  CHECK_EQ(c.got.size(), size_t{1});
}

TEST(IgnoresNonRaceboxMessages) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  // UBX-ACK-ACK, then a real data frame.
  std::vector<uint8_t> stream = frame(0x05, 0x01, {0xFF, 0x01});
  const std::vector<uint8_t> f = sampleFrame();
  stream.insert(stream.end(), f.begin(), f.end());
  feed(parser, stream);

  CHECK_EQ(c.got.size(), size_t{1});
  CHECK_EQ(parser.checksumErrors(), uint32_t{0});
}

TEST(ShortPayloadIsRejected) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  // Correct class/id but a truncated 40-byte payload: must not be decoded,
  // because decodeRaceboxPayload would read past the end.
  feed(parser, frame(0xFF, 0x01, std::vector<uint8_t>(40, 0x11)));
  CHECK_EQ(c.got.size(), size_t{0});
  CHECK_EQ(parser.checksumErrors(), uint32_t{0});
}

TEST(ResetClearsPartialFrameState) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  const std::vector<uint8_t> f = sampleFrame();
  parser.append(f.data(), 30); // half a frame
  CHECK(parser.buffered() > 0);
  parser.reset();
  CHECK_EQ(parser.buffered(), size_t{0});

  // The tail of the old frame must not combine with a new one.
  parser.append(f.data() + 30, f.size() - 30);
  CHECK_EQ(c.got.size(), size_t{0});
  feed(parser, sampleFrame());
  CHECK_EQ(c.got.size(), size_t{1});
}

// --- Transmit framing -------------------------------------------------------
// sendUbx() on the BLE client frames outbound packets with UbxParser::buildFrame,
// so the framing is exercised here rather than only on hardware.

TEST(BuildFrameMatchesTheReferenceFraming) {
  const std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
  const std::vector<uint8_t> built =
      UbxParser::buildFrame(0x06, 0x08, payload.data(), static_cast<uint16_t>(payload.size()));

  CHECK_EQ(built.size(), payload.size() + 8);
  CHECK(built == frame(0x06, 0x08, payload));
}

TEST(BuildFrameWritesTheLengthLittleEndian) {
  const std::vector<uint8_t> payload(300, 0xAB);
  const std::vector<uint8_t> built =
      UbxParser::buildFrame(0xFF, 0x02, payload.data(), static_cast<uint16_t>(payload.size()));

  CHECK_EQ(built.size(), size_t{308});
  CHECK_EQ(built[4], uint8_t{300 & 0xFF});
  CHECK_EQ(built[5], uint8_t{300 >> 8});
}

TEST(BuildFrameHandlesAnEmptyPayload) {
  const std::vector<uint8_t> built = UbxParser::buildFrame(0x0A, 0x04, nullptr, 0);
  CHECK_EQ(built.size(), size_t{8});
  CHECK_EQ(built[0], uint8_t{0xB5});
  CHECK_EQ(built[1], uint8_t{0x62});
  CHECK_EQ(built[4], uint8_t{0});
  CHECK_EQ(built[5], uint8_t{0});
  // A null payload with a non-zero length is framed as empty rather than read.
  CHECK(UbxParser::buildFrame(0x0A, 0x04, nullptr, 16) == built);
}

TEST(BuildFrameRejectsAnOversizedPayload) {
  const std::vector<uint8_t> payload(UbxParser::kMaxPayloadLen + 1, 0);
  CHECK(UbxParser::buildFrame(0xFF, 0x01, payload.data(),
                              static_cast<uint16_t>(payload.size()))
            .empty());
}

TEST(BuiltFramesRoundTripThroughTheParser) {
  UbxParser parser;
  Collector c;
  parser.setSink(c.sink());

  // A frame we build must be one we would accept: same sync word, same length
  // encoding, same checksum.
  const std::vector<uint8_t> payload = samplePayload();
  const std::vector<uint8_t> built = UbxParser::buildFrame(
      UbxParser::kRaceboxClass, UbxParser::kRaceboxDataId, payload.data(),
      static_cast<uint16_t>(payload.size()));

  parser.append(built.data(), built.size());
  CHECK_EQ(c.got.size(), size_t{1});
  CHECK_EQ(parser.checksumErrors(), uint32_t{0});
  CHECK_EQ(parser.buffered(), size_t{0});
  if (!c.got.empty()) CHECK_EQ(c.got[0].satellites, uint8_t{11});
}

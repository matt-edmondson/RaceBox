// Unit tests for the RaceBox command/reply codecs.
//
// Byte vectors are taken from the RaceBox BLE Protocol Documentation (rev 9)
// where it gives worked examples, so these tests check against the spec rather
// than against our own encoder.
#include "TestMain.hpp"

#include "../../main/ble/RaceBoxMessages.hpp"
#include "../../main/ble/UbxParser.hpp"

#include <cstring>

using ktsu::racebox::ble::Acknowledgement;
using ktsu::racebox::ble::decodeAcknowledgement;
using ktsu::racebox::ble::decodeGnssConfig;
using ktsu::racebox::ble::GnssConfig;
using ktsu::racebox::ble::kMsgIdAck;
using ktsu::racebox::ble::kMsgIdData;
using ktsu::racebox::ble::kMsgIdGnssConfig;
using ktsu::racebox::ble::kMsgIdNack;
using ktsu::racebox::ble::kRaceboxMsgClass;
using ktsu::racebox::ble::UbxParser;

namespace {

// Collects the raw non-telemetry messages the parser hands up.
struct MessageCollector {
  struct Entry {
    uint8_t msgClass;
    uint8_t msgId;
    std::vector<uint8_t> payload;
  };
  std::vector<Entry> got;

  UbxParser::MessageSink sink() {
    return [this](uint8_t msgClass, uint8_t msgId, const uint8_t* payload, size_t len) {
      Entry e{msgClass, msgId, {}};
      if (payload && len) e.payload.assign(payload, payload + len);
      got.push_back(std::move(e));
    };
  }
};

} // namespace

// --- Acknowledgements -------------------------------------------------------

TEST(AcknowledgementNamesTheMessageItAnswers) {
  // Documentation example of an ACK for a Memory Unlock (0xFF 0x30):
  //   B5 62 FF 02 02 00 FF 30 32 3A
  const uint8_t payload[2] = {0xFF, 0x30};

  Acknowledgement ack;
  CHECK(decodeAcknowledgement(kMsgIdAck, payload, sizeof(payload), ack));
  CHECK_EQ(ack.msgClass, uint8_t{0xFF});
  CHECK_EQ(ack.msgId, uint8_t{0x30});
  CHECK(ack.accepted);

  Acknowledgement nack;
  CHECK(decodeAcknowledgement(kMsgIdNack, payload, sizeof(payload), nack));
  CHECK_EQ(nack.msgId, uint8_t{0x30});
  CHECK(!nack.accepted);
}

TEST(AcknowledgementRejectsWrongIdOrShortPayload) {
  const uint8_t payload[2] = {0xFF, 0x27};
  Acknowledgement ack;

  // The data message id is not an acknowledgement.
  CHECK(!decodeAcknowledgement(kMsgIdData, payload, sizeof(payload), ack));
  // Truncated payloads carry no usable identity.
  CHECK(!decodeAcknowledgement(kMsgIdAck, payload, 1, ack));
  CHECK(!decodeAcknowledgement(kMsgIdAck, nullptr, 2, ack));
}

TEST(ParserRoutesADocumentedAckFrame) {
  // The full frame from the documentation, byte for byte.
  const std::vector<uint8_t> frame = {0xB5, 0x62, 0xFF, 0x02, 0x02, 0x00, 0xFF, 0x30, 0x32, 0x3A};

  UbxParser parser;
  MessageCollector messages;
  parser.setMessageSink(messages.sink());
  parser.append(frame.data(), frame.size());

  // Our own checksum agrees with the one in the document, so the frame is
  // accepted rather than discarded.
  CHECK_EQ(parser.checksumErrors(), uint32_t{0});
  CHECK_EQ(parser.otherFrames(), uint32_t{1});
  CHECK_EQ(messages.got.size(), size_t{1});
  if (!messages.got.empty()) {
    CHECK_EQ(messages.got[0].msgClass, uint8_t{0xFF});
    CHECK_EQ(messages.got[0].msgId, kMsgIdAck);
    CHECK_EQ(messages.got[0].payload.size(), size_t{2});

    Acknowledgement ack;
    CHECK(decodeAcknowledgement(messages.got[0].msgId, messages.got[0].payload.data(),
                                messages.got[0].payload.size(), ack));
    CHECK(ack.accepted);
    CHECK_EQ(ack.msgId, uint8_t{0x30});
  }
}

// --- GNSS receiver configuration -------------------------------------------

TEST(GnssConfigDecodesAllThreeFields) {
  const uint8_t payload[3] = {4, 1, 25};

  GnssConfig cfg;
  CHECK(decodeGnssConfig(payload, sizeof(payload), cfg));
  CHECK_EQ(cfg.dynamicModel, uint8_t{4});
  CHECK(cfg.enable3dSpeed);
  CHECK_EQ(cfg.minHorizontalAccuracyM, uint8_t{25});

  // Any non-zero value enables 3D speed.
  const uint8_t enabledOtherwise[3] = {8, 0x7F, 3};
  CHECK(decodeGnssConfig(enabledOtherwise, sizeof(enabledOtherwise), cfg));
  CHECK(cfg.enable3dSpeed);

  const uint8_t disabled[3] = {6, 0, 10};
  CHECK(decodeGnssConfig(disabled, sizeof(disabled), cfg));
  CHECK(!cfg.enable3dSpeed);
}

TEST(GnssConfigRejectsShortPayloadAndToleratesALongerOne) {
  const uint8_t payload[5] = {4, 1, 25, 0xAA, 0xBB};
  GnssConfig cfg;

  CHECK(!decodeGnssConfig(payload, 2, cfg));
  CHECK(!decodeGnssConfig(nullptr, 3, cfg));

  // A firmware that appends fields must stay readable.
  CHECK(decodeGnssConfig(payload, sizeof(payload), cfg));
  CHECK_EQ(cfg.dynamicModel, uint8_t{4});
  CHECK_EQ(cfg.minHorizontalAccuracyM, uint8_t{25});
}

TEST(GnssConfigQueryIsAnEmptyPayloadFrame) {
  const std::vector<uint8_t> query = ktsu::racebox::ble::buildGnssConfigQuery();

  CHECK_EQ(query.size(), size_t{8});
  CHECK_EQ(query[0], uint8_t{0xB5});
  CHECK_EQ(query[1], uint8_t{0x62});
  CHECK_EQ(query[2], kRaceboxMsgClass);
  CHECK_EQ(query[3], kMsgIdGnssConfig);
  CHECK_EQ(query[4], uint8_t{0});
  CHECK_EQ(query[5], uint8_t{0});

  // It must survive a round trip through our own parser.
  UbxParser parser;
  MessageCollector messages;
  parser.setMessageSink(messages.sink());
  parser.append(query.data(), query.size());
  CHECK_EQ(parser.checksumErrors(), uint32_t{0});
  CHECK_EQ(messages.got.size(), size_t{1});
  if (!messages.got.empty()) {
    CHECK_EQ(messages.got[0].msgId, kMsgIdGnssConfig);
    CHECK(messages.got[0].payload.empty());
  }
}

TEST(GnssConfigUpdateCarriesTheThreeBytesAndRejectsABadModel) {
  GnssConfig cfg;
  cfg.dynamicModel = 4;
  cfg.enable3dSpeed = true;
  cfg.minHorizontalAccuracyM = 25;

  const std::vector<uint8_t> update = ktsu::racebox::ble::buildGnssConfigUpdate(cfg);
  CHECK_EQ(update.size(), size_t{11});
  CHECK_EQ(update[4], uint8_t{3});
  CHECK_EQ(update[6], uint8_t{4});
  CHECK_EQ(update[7], uint8_t{1});
  CHECK_EQ(update[8], uint8_t{25});

  // The device NACKs a dynamic model above 8, so it never reaches the wire.
  cfg.dynamicModel = 9;
  CHECK(ktsu::racebox::ble::buildGnssConfigUpdate(cfg).empty());
}

// --- Routing ----------------------------------------------------------------

TEST(TelemetryAndCommandRepliesAreRoutedSeparately) {
  UbxParser parser;
  MessageCollector messages;
  int telemetry = 0;
  parser.setMessageSink(messages.sink());
  parser.setSink([&telemetry](const ktsu::racebox::ble::RaceboxData&) { ++telemetry; });

  // An 80-byte data message, then a config reply, in one notification.
  std::vector<uint8_t> stream =
      UbxParser::buildFrame(kRaceboxMsgClass, kMsgIdData, std::vector<uint8_t>(80, 0).data(), 80);
  const uint8_t cfgPayload[3] = {4, 0, 20};
  const std::vector<uint8_t> reply =
      UbxParser::buildFrame(kRaceboxMsgClass, kMsgIdGnssConfig, cfgPayload, sizeof(cfgPayload));
  stream.insert(stream.end(), reply.begin(), reply.end());

  parser.append(stream.data(), stream.size());

  CHECK_EQ(telemetry, 1);
  CHECK_EQ(parser.framesDecoded(), uint32_t{1});
  CHECK_EQ(messages.got.size(), size_t{1});
  CHECK_EQ(parser.otherFrames(), uint32_t{1});
  if (!messages.got.empty()) {
    GnssConfig cfg;
    CHECK(decodeGnssConfig(messages.got[0].payload.data(), messages.got[0].payload.size(), cfg));
    CHECK_EQ(cfg.dynamicModel, uint8_t{4});
    CHECK_EQ(cfg.minHorizontalAccuracyM, uint8_t{20});
  }
}

TEST(ATruncatedDataMessageIsNotReportedAsACommandReply) {
  UbxParser parser;
  MessageCollector messages;
  int telemetry = 0;
  parser.setMessageSink(messages.sink());
  parser.setSink([&telemetry](const ktsu::racebox::ble::RaceboxData&) { ++telemetry; });

  const std::vector<uint8_t> shortData =
      UbxParser::buildFrame(kRaceboxMsgClass, kMsgIdData, std::vector<uint8_t>(40, 0).data(), 40);
  parser.append(shortData.data(), shortData.size());

  CHECK_EQ(telemetry, 0);
  CHECK_EQ(messages.got.size(), size_t{0});
}

// --- Device model -----------------------------------------------------------

TEST(DeviceModelComesFromTheAdvertisedName) {
  using ktsu::racebox::ble::deviceModelFromName;
  using ktsu::racebox::ble::DeviceModel;

  CHECK(deviceModelFromName("RaceBox Mini 1234567") == DeviceModel::Mini);
  // "Mini S" must not be read as a plain "Mini".
  CHECK(deviceModelFromName("RaceBox Mini S 1234567") == DeviceModel::MiniS);
  CHECK(deviceModelFromName("RaceBox Micro 1234567") == DeviceModel::Micro);

  // Matching is case-insensitive and tolerates an unexpected shape.
  CHECK(deviceModelFromName("racebox mini s 42") == DeviceModel::MiniS);
  CHECK(deviceModelFromName("MICRO") == DeviceModel::Micro);

  // A peer matched only by its service UUID advertises no name at all.
  CHECK(deviceModelFromName(nullptr) == DeviceModel::Unknown);
  CHECK(deviceModelFromName("") == DeviceModel::Unknown);
  CHECK(deviceModelFromName("RaceBox") == DeviceModel::Unknown);
}

TEST(ByteSixtySevenIsReadPerModel) {
  using ktsu::racebox::ble::batteryPercentFromRaw;
  using ktsu::racebox::ble::chargingFromRaw;
  using ktsu::racebox::ble::DeviceModel;
  using ktsu::racebox::ble::inputVoltsFromRaw;
  using ktsu::racebox::ble::reportsBatteryPercent;

  // Documentation example for a Mini: 0x59 is 89%, not charging.
  CHECK_EQ(batteryPercentFromRaw(0x59), uint8_t{89});
  CHECK(!chargingFromRaw(0x59));
  // The same byte with the top bit set is the same level, charging.
  CHECK_EQ(batteryPercentFromRaw(0xD9), uint8_t{89});
  CHECK(chargingFromRaw(0xD9));

  // Documentation example for a Micro: 0x79 is 12.1 V.
  CHECK_NEAR(inputVoltsFromRaw(0x79), 12.1, 1e-6);
  // Read as a percentage it would be a nonsensical 121%, which is exactly why
  // the status bar picks by model.
  CHECK_EQ(batteryPercentFromRaw(0x79), uint8_t{121});

  CHECK(reportsBatteryPercent(DeviceModel::Mini));
  CHECK(reportsBatteryPercent(DeviceModel::MiniS));
  CHECK(reportsBatteryPercent(DeviceModel::Unknown));
  CHECK(!reportsBatteryPercent(DeviceModel::Micro));
}

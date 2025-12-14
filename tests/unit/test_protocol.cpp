/**
 * @file test_protocol.cpp
 * @brief Unit tests for SeaDrop wire protocol
 */

#include <gtest/gtest.h>
#include <seadrop/protocol.h>

using namespace seadrop;

// ============================================================================
// Packet Header Tests
// ============================================================================

TEST(ProtocolTest, PacketHeaderCreate) {
  auto header = PacketHeader::create(MessageType::Hello, 100);

  EXPECT_EQ(header.magic, PROTOCOL_MAGIC);
  EXPECT_EQ(header.version, PROTOCOL_VERSION);
  EXPECT_EQ(header.type, static_cast<uint8_t>(MessageType::Hello));
  EXPECT_EQ(header.payload_size, 100u);
  EXPECT_TRUE(header.is_valid());
}

TEST(ProtocolTest, PacketHeaderSerializeRoundtrip) {
  auto original = PacketHeader::create(MessageType::TransferRequest, 1024);

  Bytes serialized = serialize_header(original);
  EXPECT_EQ(serialized.size(), PACKET_HEADER_SIZE);

  auto result = deserialize_header(serialized);
  ASSERT_TRUE(result.is_ok());

  auto deserialized = result.value();
  EXPECT_EQ(deserialized.magic, original.magic);
  EXPECT_EQ(deserialized.version, original.version);
  EXPECT_EQ(deserialized.type, original.type);
  EXPECT_EQ(deserialized.payload_size, original.payload_size);
}

TEST(ProtocolTest, PacketHeaderInvalidMagic) {
  Bytes data = {0x00,
                0x00,
                0x00,
                0x00, // Invalid magic
                PROTOCOL_VERSION,
                0x01,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00};

  auto result = deserialize_header(data);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code, ErrorCode::InvalidArgument);
}

TEST(ProtocolTest, PacketHeaderTooShort) {
  Bytes data = {0x53, 0x45, 0x41, 0x44}; // Only 4 bytes

  auto result = deserialize_header(data);
  EXPECT_TRUE(result.is_error());
}

// ============================================================================
// Hello Message Tests
// ============================================================================

TEST(ProtocolTest, HelloMessageSerializeRoundtrip) {
  HelloMessage original;
  original.device_id = DeviceId::generate();
  original.device_name = "Test Device";
  original.platform = DevicePlatform::Linux;
  original.version_string = "1.0.0";
  original.capabilities =
      HelloMessage::CAP_WIFI_DIRECT | HelloMessage::CAP_CLIPBOARD;

  Bytes serialized = serialize_hello(original);
  auto result = deserialize_hello(serialized);

  ASSERT_TRUE(result.is_ok());
  auto deserialized = result.value();

  EXPECT_EQ(deserialized.device_id.bytes, original.device_id.bytes);
  EXPECT_EQ(deserialized.device_name, original.device_name);
  EXPECT_EQ(deserialized.platform, original.platform);
  EXPECT_EQ(deserialized.version_string, original.version_string);
  EXPECT_EQ(deserialized.capabilities, original.capabilities);
}

// ============================================================================
// Transfer Request Tests
// ============================================================================

TEST(ProtocolTest, TransferRequestSerializeRoundtrip) {
  TransferRequestMessage original;
  original.transfer_id = TransferId::generate();
  original.total_size = 1024 * 1024 * 100; // 100 MB
  original.include_checksum = true;

  FileEntry file1;
  file1.relative_path = "documents/report.pdf";
  file1.size = 50 * 1024 * 1024;
  file1.mime_type = "application/pdf";
  file1.modified_time = 1702500000;
  original.files.push_back(file1);

  FileEntry file2;
  file2.relative_path = "images/photo.jpg";
  file2.size = 5 * 1024 * 1024;
  file2.mime_type = "image/jpeg";
  file2.modified_time = 1702500001;
  original.files.push_back(file2);

  Bytes serialized = serialize_transfer_request(original);
  auto result = deserialize_transfer_request(serialized);

  ASSERT_TRUE(result.is_ok());
  auto deserialized = result.value();

  EXPECT_EQ(deserialized.transfer_id.bytes, original.transfer_id.bytes);
  EXPECT_EQ(deserialized.total_size, original.total_size);
  EXPECT_EQ(deserialized.include_checksum, original.include_checksum);
  ASSERT_EQ(deserialized.files.size(), 2u);
  EXPECT_EQ(deserialized.files[0].relative_path, file1.relative_path);
  EXPECT_EQ(deserialized.files[0].size, file1.size);
  EXPECT_EQ(deserialized.files[1].relative_path, file2.relative_path);
}

// ============================================================================
// File Header Tests
// ============================================================================

TEST(ProtocolTest, FileHeaderSerializeRoundtrip) {
  FileHeaderMessage original;
  original.transfer_id = TransferId::generate();
  original.file_index = 0;
  original.filename = "test_file.txt";
  original.file_size = 12345;
  original.total_chunks = 10;
  original.chunk_size = DEFAULT_CHUNK_SIZE;

  Bytes serialized = serialize_file_header(original);
  auto result = deserialize_file_header(serialized);

  ASSERT_TRUE(result.is_ok());
  auto deserialized = result.value();

  EXPECT_EQ(deserialized.transfer_id.bytes, original.transfer_id.bytes);
  EXPECT_EQ(deserialized.file_index, original.file_index);
  EXPECT_EQ(deserialized.filename, original.filename);
  EXPECT_EQ(deserialized.file_size, original.file_size);
  EXPECT_EQ(deserialized.total_chunks, original.total_chunks);
  EXPECT_EQ(deserialized.chunk_size, original.chunk_size);
}

// ============================================================================
// Chunk Ack Tests
// ============================================================================

TEST(ProtocolTest, ChunkAckSerializeRoundtrip) {
  ChunkAckMessage original;
  original.transfer_id = TransferId::generate();
  original.file_index = 2;
  original.chunk_index = 15;
  original.success = true;

  Bytes serialized = serialize_chunk_ack(original);
  auto result = deserialize_chunk_ack(serialized);

  ASSERT_TRUE(result.is_ok());
  auto deserialized = result.value();

  EXPECT_EQ(deserialized.transfer_id.bytes, original.transfer_id.bytes);
  EXPECT_EQ(deserialized.file_index, original.file_index);
  EXPECT_EQ(deserialized.chunk_index, original.chunk_index);
  EXPECT_EQ(deserialized.success, original.success);
}

// ============================================================================
// Progress Tests
// ============================================================================

TEST(ProtocolTest, ProgressSerializeRoundtrip) {
  ProgressMessage original;
  original.transfer_id = TransferId::generate();
  original.bytes_transferred = 50 * 1024 * 1024;
  original.total_bytes = 100 * 1024 * 1024;
  original.files_completed = 3;
  original.total_files = 10;

  Bytes serialized = serialize_progress(original);
  auto result = deserialize_progress(serialized);

  ASSERT_TRUE(result.is_ok());
  auto deserialized = result.value();

  EXPECT_EQ(deserialized.bytes_transferred, original.bytes_transferred);
  EXPECT_EQ(deserialized.total_bytes, original.total_bytes);
  EXPECT_EQ(deserialized.files_completed, original.files_completed);
  EXPECT_EQ(deserialized.total_files, original.total_files);
}

// ============================================================================
// Packet Builder Tests
// ============================================================================

TEST(ProtocolTest, BuildPacket) {
  Bytes payload = {0x01, 0x02, 0x03, 0x04};
  Bytes packet = build_packet(MessageType::Ping, payload);

  EXPECT_EQ(packet.size(), PACKET_HEADER_SIZE + payload.size());

  auto header_result = deserialize_header(packet);
  ASSERT_TRUE(header_result.is_ok());

  auto header = header_result.value();
  EXPECT_EQ(header.type, static_cast<uint8_t>(MessageType::Ping));
  EXPECT_EQ(header.payload_size, payload.size());
}

// ============================================================================
// Packet Parser Tests
// ============================================================================

TEST(ProtocolTest, PacketParserSinglePacket) {
  Bytes payload = {0xAA, 0xBB, 0xCC};
  Bytes packet = build_packet(MessageType::Hello, payload);

  PacketParser parser;
  parser.feed(packet);

  EXPECT_TRUE(parser.has_packet());

  auto result = parser.next_packet();
  ASSERT_TRUE(result.is_ok());

  auto [header, parsed_payload] = result.value();
  EXPECT_EQ(header.type, static_cast<uint8_t>(MessageType::Hello));
  EXPECT_EQ(parsed_payload, payload);
  EXPECT_FALSE(parser.has_packet());
}

TEST(ProtocolTest, PacketParserMultiplePackets) {
  Bytes packet1 = build_packet(MessageType::Ping, {});
  Bytes packet2 = build_packet(MessageType::Pong, {0x01});

  Bytes combined = packet1;
  combined.insert(combined.end(), packet2.begin(), packet2.end());

  PacketParser parser;
  parser.feed(combined);

  EXPECT_TRUE(parser.has_packet());
  auto result1 = parser.next_packet();
  ASSERT_TRUE(result1.is_ok());
  EXPECT_EQ(result1.value().first.type,
            static_cast<uint8_t>(MessageType::Ping));

  EXPECT_TRUE(parser.has_packet());
  auto result2 = parser.next_packet();
  ASSERT_TRUE(result2.is_ok());
  EXPECT_EQ(result2.value().first.type,
            static_cast<uint8_t>(MessageType::Pong));

  EXPECT_FALSE(parser.has_packet());
}

TEST(ProtocolTest, PacketParserPartialFeed) {
  Bytes packet = build_packet(MessageType::Hello, {0x01, 0x02, 0x03});

  PacketParser parser;

  // Feed half the packet
  size_t half = packet.size() / 2;
  parser.feed(Bytes(packet.begin(), packet.begin() + half));
  EXPECT_FALSE(parser.has_packet());

  // Feed the rest
  parser.feed(Bytes(packet.begin() + half, packet.end()));
  EXPECT_TRUE(parser.has_packet());

  auto result = parser.next_packet();
  ASSERT_TRUE(result.is_ok());
}

// ============================================================================
// Message Type Names
// ============================================================================

TEST(ProtocolTest, MessageTypeNames) {
  EXPECT_STREQ(message_type_name(MessageType::Hello), "Hello");
  EXPECT_STREQ(message_type_name(MessageType::TransferRequest),
               "TransferRequest");
  EXPECT_STREQ(message_type_name(MessageType::FileChunk), "FileChunk");
}

/**
 * @file protocol.cpp
 * @brief SeaDrop wire protocol implementation
 */

#include "seadrop/protocol.h"
#include <algorithm>
#include <cstring>

namespace seadrop {

// ============================================================================
// Message Type Names
// ============================================================================

const char *message_type_name(MessageType type) {
  switch (type) {
  case MessageType::Hello:
    return "Hello";
  case MessageType::HelloAck:
    return "HelloAck";
  case MessageType::VersionMismatch:
    return "VersionMismatch";
  case MessageType::TransferRequest:
    return "TransferRequest";
  case MessageType::TransferAccept:
    return "TransferAccept";
  case MessageType::TransferReject:
    return "TransferReject";
  case MessageType::TransferCancel:
    return "TransferCancel";
  case MessageType::TransferPause:
    return "TransferPause";
  case MessageType::TransferResume:
    return "TransferResume";
  case MessageType::FileHeader:
    return "FileHeader";
  case MessageType::FileChunk:
    return "FileChunk";
  case MessageType::FileComplete:
    return "FileComplete";
  case MessageType::ChunkAck:
    return "ChunkAck";
  case MessageType::ChunkNack:
    return "ChunkNack";
  case MessageType::Progress:
    return "Progress";
  case MessageType::Error:
    return "Error";
  case MessageType::Ping:
    return "Ping";
  case MessageType::Pong:
    return "Pong";
  case MessageType::ClipboardPush:
    return "ClipboardPush";
  case MessageType::ClipboardAck:
    return "ClipboardAck";
  default:
    return "Unknown";
  }
}

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Write little-endian uint16
void write_u16(Bytes &buf, uint16_t val) {
  buf.push_back(static_cast<Byte>(val & 0xFF));
  buf.push_back(static_cast<Byte>((val >> 8) & 0xFF));
}

// Write little-endian uint32
void write_u32(Bytes &buf, uint32_t val) {
  buf.push_back(static_cast<Byte>(val & 0xFF));
  buf.push_back(static_cast<Byte>((val >> 8) & 0xFF));
  buf.push_back(static_cast<Byte>((val >> 16) & 0xFF));
  buf.push_back(static_cast<Byte>((val >> 24) & 0xFF));
}

// Write little-endian uint64
void write_u64(Bytes &buf, uint64_t val) {
  for (int i = 0; i < 8; ++i) {
    buf.push_back(static_cast<Byte>((val >> (i * 8)) & 0xFF));
  }
}

// Write length-prefixed string (u16 length + chars)
void write_string(Bytes &buf, const std::string &str) {
  uint16_t len = static_cast<uint16_t>(std::min(str.size(), size_t(65535)));
  write_u16(buf, len);
  buf.insert(buf.end(), str.begin(), str.begin() + len);
}

// Write fixed-size byte array
template <size_t N>
void write_array(Bytes &buf, const std::array<Byte, N> &arr) {
  buf.insert(buf.end(), arr.begin(), arr.end());
}

// Read little-endian uint16
uint16_t read_u16(const Byte *d) {
  return static_cast<uint16_t>(d[0]) | (static_cast<uint16_t>(d[1]) << 8);
}

// Read little-endian uint32
uint32_t read_u32(const Byte *d) {
  return static_cast<uint32_t>(d[0]) | (static_cast<uint32_t>(d[1]) << 8) |
         (static_cast<uint32_t>(d[2]) << 16) |
         (static_cast<uint32_t>(d[3]) << 24);
}

// Read little-endian uint64
uint64_t read_u64(const Byte *d) {
  uint64_t result = 0;
  for (int i = 0; i < 8; ++i) {
    result |= static_cast<uint64_t>(d[i]) << (i * 8);
  }
  return result;
}

// Read length-prefixed string
std::string read_string(const Byte *d, size_t &offset, size_t max_size) {
  if (offset + 2 > max_size) {
    return "";
  }
  uint16_t len = read_u16(d + offset);
  offset += 2;
  if (offset + len > max_size) {
    return "";
  }
  std::string result(reinterpret_cast<const char *>(d + offset), len);
  offset += len;
  return result;
}

// Read fixed-size byte array
template <size_t N> std::array<Byte, N> read_array(const Byte *d) {
  std::array<Byte, N> result;
  std::memcpy(result.data(), d, N);
  return result;
}

} // anonymous namespace

// ============================================================================
// PacketHeader
// ============================================================================

PacketHeader PacketHeader::create(MessageType msg_type, uint32_t payload_len) {
  PacketHeader header;
  header.magic = PROTOCOL_MAGIC;
  header.version = PROTOCOL_VERSION;
  header.type = static_cast<uint8_t>(msg_type);
  header.flags = 0;
  header.payload_size = payload_len;
  return header;
}

bool PacketHeader::is_valid() const {
  return magic == PROTOCOL_MAGIC && version == PROTOCOL_VERSION &&
         payload_size <= MAX_PAYLOAD_SIZE;
}

// ============================================================================
// Header Serialization
// ============================================================================

Bytes serialize_header(const PacketHeader &header) {
  Bytes buf;
  buf.reserve(PACKET_HEADER_SIZE);
  write_u32(buf, header.magic);
  buf.push_back(header.version);
  buf.push_back(header.type);
  write_u16(buf, header.flags);
  write_u32(buf, header.payload_size);
  return buf;
}

Result<PacketHeader> deserialize_header(const Bytes &buf) {
  if (buf.size() < PACKET_HEADER_SIZE) {
    return Error(ErrorCode::InvalidArgument, "Header too short");
  }

  PacketHeader header;
  header.magic = read_u32(buf.data());
  header.version = buf[4];
  header.type = buf[5];
  header.flags = read_u16(buf.data() + 6);
  header.payload_size = read_u32(buf.data() + 8);

  if (!header.is_valid()) {
    if (header.magic != PROTOCOL_MAGIC) {
      return Error(ErrorCode::InvalidArgument, "Invalid magic number");
    }
    if (header.version != PROTOCOL_VERSION) {
      return Error(ErrorCode::NotSupported, "Protocol version mismatch");
    }
    if (header.payload_size > MAX_PAYLOAD_SIZE) {
      return Error(ErrorCode::InvalidArgument, "Payload too large");
    }
  }

  return header;
}

// ============================================================================
// Hello Message
// ============================================================================

Bytes serialize_hello(const HelloMessage &msg) {
  Bytes buf;
  buf.reserve(128);
  write_array(buf, msg.device_id.data);
  write_string(buf, msg.device_name);
  buf.push_back(static_cast<Byte>(msg.platform));
  write_string(buf, msg.version_string);
  write_u32(buf, msg.capabilities);
  return buf;
}

Result<HelloMessage> deserialize_hello(const Bytes &buf) {
  if (buf.size() < 32 + 2 + 1 + 2 + 4) {
    return Error(ErrorCode::InvalidArgument, "Hello message too short");
  }

  HelloMessage msg;
  size_t offset = 0;
  msg.device_id.data = read_array<32>(buf.data());
  offset += 32;
  msg.device_name = read_string(buf.data(), offset, buf.size());
  if (offset >= buf.size()) {
    return Error(ErrorCode::InvalidArgument, "Hello message truncated");
  }
  msg.platform = static_cast<DevicePlatform>(buf[offset]);
  offset++;
  msg.version_string = read_string(buf.data(), offset, buf.size());
  if (offset + 4 > buf.size()) {
    return Error(ErrorCode::InvalidArgument, "Hello message truncated");
  }
  msg.capabilities = read_u32(buf.data() + offset);
  return msg;
}

// ============================================================================
// Transfer Request Message
// ============================================================================

Bytes serialize_transfer_request(const TransferRequestMessage &msg) {
  Bytes buf;
  buf.reserve(256 + msg.files.size() * 128);
  write_array(buf, msg.transfer_id.data);
  write_u64(buf, msg.total_size);
  buf.push_back(msg.include_checksum ? 1 : 0);
  uint32_t file_count =
      static_cast<uint32_t>(std::min(msg.files.size(), MAX_FILES_PER_REQUEST));
  write_u32(buf, file_count);

  for (size_t i = 0; i < file_count; ++i) {
    const auto &file = msg.files[i];
    write_string(buf, file.relative_path);
    write_u64(buf, file.size);
    write_string(buf, file.mime_type);
    if (msg.include_checksum) {
      write_array(buf, file.checksum);
    }
    write_u64(buf, file.modified_time);
  }
  return buf;
}

Result<TransferRequestMessage> deserialize_transfer_request(const Bytes &buf) {
  if (buf.size() < 16 + 8 + 1 + 4) {
    return Error(ErrorCode::InvalidArgument, "Transfer request too short");
  }

  TransferRequestMessage msg;
  size_t offset = 0;
  msg.transfer_id.data = read_array<16>(buf.data());
  offset += 16;
  msg.total_size = read_u64(buf.data() + offset);
  offset += 8;
  msg.include_checksum = buf[offset] != 0;
  offset++;
  uint32_t file_count = read_u32(buf.data() + offset);
  offset += 4;

  if (file_count > MAX_FILES_PER_REQUEST) {
    return Error(ErrorCode::InvalidArgument, "Too many files in request");
  }

  msg.files.reserve(file_count);
  for (uint32_t i = 0; i < file_count; ++i) {
    FileEntry file;
    file.relative_path = read_string(buf.data(), offset, buf.size());
    if (offset + 8 > buf.size()) {
      return Error(ErrorCode::InvalidArgument, "Transfer request truncated");
    }
    file.size = read_u64(buf.data() + offset);
    offset += 8;
    file.mime_type = read_string(buf.data(), offset, buf.size());
    if (msg.include_checksum) {
      if (offset + 32 > buf.size()) {
        return Error(ErrorCode::InvalidArgument, "Transfer request truncated");
      }
      file.checksum = read_array<32>(buf.data() + offset);
      offset += 32;
    }
    if (offset + 8 > buf.size()) {
      return Error(ErrorCode::InvalidArgument, "Transfer request truncated");
    }
    file.modified_time = read_u64(buf.data() + offset);
    offset += 8;
    msg.files.push_back(std::move(file));
  }
  return msg;
}

// ============================================================================
// Transfer Accept Message
// ============================================================================

Bytes serialize_transfer_accept(const TransferAcceptMessage &msg) {
  Bytes buf;
  buf.reserve(64);
  write_array(buf, msg.transfer_id.data);
  write_string(buf, msg.save_directory);
  return buf;
}

Result<TransferAcceptMessage> deserialize_transfer_accept(const Bytes &buf) {
  if (buf.size() < 16 + 2) {
    return Error(ErrorCode::InvalidArgument, "Transfer accept too short");
  }
  TransferAcceptMessage msg;
  size_t offset = 0;
  msg.transfer_id.data = read_array<16>(buf.data());
  offset += 16;
  msg.save_directory = read_string(buf.data(), offset, buf.size());
  return msg;
}

// ============================================================================
// File Header Message
// ============================================================================

Bytes serialize_file_header(const FileHeaderMessage &msg) {
  Bytes buf;
  buf.reserve(128);
  write_array(buf, msg.transfer_id.data);
  write_u32(buf, msg.file_index);
  write_string(buf, msg.filename);
  write_u64(buf, msg.file_size);
  write_u32(buf, msg.total_chunks);
  write_u32(buf, msg.chunk_size);
  return buf;
}

Result<FileHeaderMessage> deserialize_file_header(const Bytes &buf) {
  if (buf.size() < 16 + 4 + 2 + 8 + 4 + 4) {
    return Error(ErrorCode::InvalidArgument, "File header too short");
  }
  FileHeaderMessage msg;
  size_t offset = 0;
  msg.transfer_id.data = read_array<16>(buf.data());
  offset += 16;
  msg.file_index = read_u32(buf.data() + offset);
  offset += 4;
  msg.filename = read_string(buf.data(), offset, buf.size());
  if (offset + 8 + 4 + 4 > buf.size()) {
    return Error(ErrorCode::InvalidArgument, "File header truncated");
  }
  msg.file_size = read_u64(buf.data() + offset);
  offset += 8;
  msg.total_chunks = read_u32(buf.data() + offset);
  offset += 4;
  msg.chunk_size = read_u32(buf.data() + offset);
  return msg;
}

// ============================================================================
// File Chunk Message
// ============================================================================

Bytes serialize_chunk_header(const FileChunkMessage &msg) {
  Bytes buf;
  buf.reserve(28);
  write_array(buf, msg.transfer_id.data);
  write_u32(buf, msg.file_index);
  write_u32(buf, msg.chunk_index);
  write_u32(buf, msg.chunk_size);
  return buf;
}

Result<FileChunkMessage> deserialize_chunk_header(const Bytes &buf) {
  if (buf.size() < 16 + 4 + 4 + 4) {
    return Error(ErrorCode::InvalidArgument, "Chunk header too short");
  }
  FileChunkMessage msg;
  msg.transfer_id.data = read_array<16>(buf.data());
  msg.file_index = read_u32(buf.data() + 16);
  msg.chunk_index = read_u32(buf.data() + 20);
  msg.chunk_size = read_u32(buf.data() + 24);
  return msg;
}

// ============================================================================
// Chunk Acknowledgment
// ============================================================================

Bytes serialize_chunk_ack(const ChunkAckMessage &msg) {
  Bytes buf;
  buf.reserve(25);
  write_array(buf, msg.transfer_id.data);
  write_u32(buf, msg.file_index);
  write_u32(buf, msg.chunk_index);
  buf.push_back(msg.success ? 1 : 0);
  return buf;
}

Result<ChunkAckMessage> deserialize_chunk_ack(const Bytes &buf) {
  if (buf.size() < 16 + 4 + 4 + 1) {
    return Error(ErrorCode::InvalidArgument, "Chunk ack too short");
  }
  ChunkAckMessage msg;
  msg.transfer_id.data = read_array<16>(buf.data());
  msg.file_index = read_u32(buf.data() + 16);
  msg.chunk_index = read_u32(buf.data() + 20);
  msg.success = buf[24] != 0;
  return msg;
}

// ============================================================================
// Progress Message
// ============================================================================

Bytes serialize_progress(const ProgressMessage &msg) {
  Bytes buf;
  buf.reserve(40);
  write_array(buf, msg.transfer_id.data);
  write_u64(buf, msg.bytes_transferred);
  write_u64(buf, msg.total_bytes);
  write_u32(buf, msg.files_completed);
  write_u32(buf, msg.total_files);
  return buf;
}

Result<ProgressMessage> deserialize_progress(const Bytes &buf) {
  if (buf.size() < 16 + 8 + 8 + 4 + 4) {
    return Error(ErrorCode::InvalidArgument, "Progress message too short");
  }
  ProgressMessage msg;
  msg.transfer_id.data = read_array<16>(buf.data());
  msg.bytes_transferred = read_u64(buf.data() + 16);
  msg.total_bytes = read_u64(buf.data() + 24);
  msg.files_completed = read_u32(buf.data() + 32);
  msg.total_files = read_u32(buf.data() + 36);
  return msg;
}

// ============================================================================
// Error Message
// ============================================================================

Bytes serialize_error(const ErrorMessage &msg) {
  Bytes buf;
  buf.reserve(48);
  write_array(buf, msg.transfer_id.data);
  write_u32(buf, static_cast<uint32_t>(msg.code));
  write_string(buf, msg.message);
  buf.push_back(msg.fatal ? 1 : 0);
  return buf;
}

Result<ErrorMessage> deserialize_error(const Bytes &buf) {
  if (buf.size() < 16 + 4 + 2 + 1) {
    return Error(ErrorCode::InvalidArgument, "Error message too short");
  }
  ErrorMessage msg;
  size_t offset = 0;
  msg.transfer_id.data = read_array<16>(buf.data());
  offset += 16;
  msg.code = static_cast<ErrorCode>(read_u32(buf.data() + offset));
  offset += 4;
  msg.message = read_string(buf.data(), offset, buf.size());
  if (offset >= buf.size()) {
    return Error(ErrorCode::InvalidArgument, "Error message truncated");
  }
  msg.fatal = buf[offset] != 0;
  return msg;
}

// ============================================================================
// Packet Builder
// ============================================================================

Bytes build_packet(MessageType type, const Bytes &payload) {
  PacketHeader header =
      PacketHeader::create(type, static_cast<uint32_t>(payload.size()));
  Bytes packet = serialize_header(header);
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

// ============================================================================
// Packet Parser
// ============================================================================

void PacketParser::feed(const Bytes &data) {
  buffer_.insert(buffer_.end(), data.begin(), data.end());
}

bool PacketParser::has_packet() const {
  if (buffer_.size() < PACKET_HEADER_SIZE) {
    return false;
  }
  uint32_t payload_size = read_u32(buffer_.data() + 8);
  return buffer_.size() >= PACKET_HEADER_SIZE + payload_size;
}

Result<std::pair<PacketHeader, Bytes>> PacketParser::next_packet() {
  if (!has_packet()) {
    return Error(ErrorCode::InvalidState, "No complete packet available");
  }

  Bytes header_bytes(buffer_.begin(), buffer_.begin() + PACKET_HEADER_SIZE);
  auto header_result = deserialize_header(header_bytes);
  if (header_result.is_error()) {
    return header_result.error();
  }

  PacketHeader header = header_result.value();
  Bytes payload(buffer_.begin() + PACKET_HEADER_SIZE,
                buffer_.begin() + PACKET_HEADER_SIZE + header.payload_size);

  buffer_.erase(buffer_.begin(),
                buffer_.begin() + PACKET_HEADER_SIZE + header.payload_size);

  return std::make_pair(header, payload);
}

void PacketParser::reset() {
  buffer_.clear();
  parse_offset_ = 0;
}

size_t PacketParser::buffered_size() const { return buffer_.size(); }

} // namespace seadrop

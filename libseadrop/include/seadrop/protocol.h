/**
 * @file protocol.h
 * @brief SeaDrop wire protocol definitions
 *
 * Defines the message format and packet structure for P2P communication.
 * All multi-byte values are little-endian.
 */

#ifndef SEADROP_PROTOCOL_H
#define SEADROP_PROTOCOL_H

#include "seadrop/error.h"
#include "seadrop/types.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
/// Default chunk size for file transfers (64 KB)
constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024;



namespace seadrop {

// ============================================================================
// Protocol Constants
// ============================================================================

/// Protocol magic number: "SEAD" in little-endian
constexpr uint32_t PROTOCOL_MAGIC = 0x44414553;

/// Current protocol version
constexpr uint8_t PROTOCOL_VERSION = 1;

/// Maximum payload size (16 MB)
constexpr uint32_t MAX_PAYLOAD_SIZE = 16 * 1024 * 1024;

/// Header size in bytes
constexpr size_t PACKET_HEADER_SIZE = 12;

/// Maximum filename length in transfer request
constexpr size_t MAX_PROTOCOL_FILENAME = 255;

/// Maximum files per transfer request
constexpr size_t MAX_FILES_PER_REQUEST = 1000;

// ============================================================================
// Message Types
// ============================================================================

/**
 * @brief Protocol message type identifiers
 */
enum class MessageType : uint8_t {
  // ---- Handshake (0x01-0x0F) ----
  /// Initial connection hello
  Hello = 0x01,
  /// Hello acknowledgment with device info
  HelloAck = 0x02,
  /// Protocol version mismatch
  VersionMismatch = 0x03,

  // ---- Transfer Control (0x10-0x1F) ----
  /// Request to send files
  TransferRequest = 0x10,
  /// Accept transfer request
  TransferAccept = 0x11,
  /// Reject transfer request
  TransferReject = 0x12,
  /// Cancel ongoing transfer
  TransferCancel = 0x13,
  /// Transfer paused
  TransferPause = 0x14,
  /// Resume paused transfer
  TransferResume = 0x15,

  // ---- Data Transfer (0x20-0x2F) ----
  /// File metadata header
  FileHeader = 0x20,
  /// File data chunk
  FileChunk = 0x21,
  /// File transfer complete
  FileComplete = 0x22,
  /// Chunk acknowledgment
  ChunkAck = 0x23,
  /// Request chunk retransmission
  ChunkNack = 0x24,

  // ---- Status (0x30-0x3F) ----
  /// Progress update
  Progress = 0x30,
  /// Error notification
  Error = 0x31,

  // ---- Keep-alive (0x40-0x4F) ----
  /// Ping request
  Ping = 0x40,
  /// Pong response
  Pong = 0x41,

  // ---- Clipboard (0x50-0x5F) ----
  /// Clipboard content push
  ClipboardPush = 0x50,
  /// Clipboard push acknowledgment
  ClipboardAck = 0x51
};

/**
 * @brief Get human-readable name for message type
 */
SEADROP_API const char *message_type_name(MessageType type);

// ============================================================================
// Packet Header
// ============================================================================

/**
 * @brief Wire format packet header (12 bytes)
 *
 * Layout:
 *   Offset  Size  Field
 *   0       4     magic (0x44414553 = "SEAD")
 *   4       1     version
 *   5       1     type (MessageType)
 *   6       2     flags (reserved)
 *   8       4     payload_size
 */
struct PacketHeader {
  uint32_t magic = PROTOCOL_MAGIC;
  uint8_t version = PROTOCOL_VERSION;
  uint8_t type = 0;
  uint16_t flags = 0;
  uint32_t payload_size = 0;

  /// Create header for a message type
  static PacketHeader create(MessageType msg_type, uint32_t payload_len);

  /// Validate header fields
  bool is_valid() const;
};

// ============================================================================
// Message Payloads
// ============================================================================

/**
 * @brief Hello message payload
 */
struct HelloMessage {
  DeviceId device_id;
  std::string device_name;
  DevicePlatform platform;
  std::string version_string;
  uint32_t capabilities = 0; // Bitmask of supported features

  enum Capability : uint32_t {
    CAP_WIFI_DIRECT = 1 << 0,
    CAP_BLUETOOTH = 1 << 1,
    CAP_CLIPBOARD = 1 << 2,
    CAP_RESUMABLE = 1 << 3
  };
};

/**
 * @brief File entry in transfer request
 */
struct FileEntry {
  std::string relative_path;
  uint64_t size = 0;
  std::string mime_type;
  std::array<Byte, 32> checksum = {};
  uint64_t modified_time = 0; // Unix timestamp
};

/**
 * @brief Transfer request payload
 */
struct TransferRequestMessage {
  TransferId transfer_id;
  std::vector<FileEntry> files;
  uint64_t total_size = 0;
  bool include_checksum = true;
};

/**
 * @brief Transfer accept payload
 */
struct TransferAcceptMessage {
  TransferId transfer_id;
  std::string save_directory; // Optional: receiver-specified path
};

/**
 * @brief Transfer reject payload
 */
struct TransferRejectMessage {
  TransferId transfer_id;
  std::string reason;
};

/**
 * @brief File header (sent before file data)
 */
struct FileHeaderMessage {
  TransferId transfer_id;
  uint32_t file_index = 0; // Index in transfer request
  std::string filename;
  uint64_t file_size = 0;
  uint32_t total_chunks = 0;
  uint32_t chunk_size = DEFAULT_CHUNK_SIZE;
};

/**
 * @brief File chunk payload
 */
struct FileChunkMessage {
  TransferId transfer_id;
  uint32_t file_index = 0;
  uint32_t chunk_index = 0;
  uint32_t chunk_size = 0;
  // Actual data follows in payload
};

/**
 * @brief Chunk acknowledgment
 */
struct ChunkAckMessage {
  TransferId transfer_id;
  uint32_t file_index = 0;
  uint32_t chunk_index = 0;
  bool success = true;
};

/**
 * @brief Progress update
 */
struct ProgressMessage {
  TransferId transfer_id;
  uint64_t bytes_transferred = 0;
  uint64_t total_bytes = 0;
  uint32_t files_completed = 0;
  uint32_t total_files = 0;
};

/**
 * @brief Error message
 */
struct ErrorMessage {
  TransferId transfer_id;
  ErrorCode code;
  std::string message;
  bool fatal = false; // If true, transfer is terminated
};

// ============================================================================
// Serialization
// ============================================================================

/**
 * @brief Serialize packet header to bytes
 */
SEADROP_API Bytes serialize_header(const PacketHeader &header);

/**
 * @brief Deserialize packet header from bytes
 */
SEADROP_API Result<PacketHeader> deserialize_header(const Bytes &data);

/**
 * @brief Serialize hello message
 */
SEADROP_API Bytes serialize_hello(const HelloMessage &msg);

/**
 * @brief Deserialize hello message
 */
SEADROP_API Result<HelloMessage> deserialize_hello(const Bytes &data);

/**
 * @brief Serialize transfer request
 */
SEADROP_API Bytes serialize_transfer_request(const TransferRequestMessage &msg);

/**
 * @brief Deserialize transfer request
 */
SEADROP_API Result<TransferRequestMessage>
deserialize_transfer_request(const Bytes &data);

/**
 * @brief Serialize transfer accept
 */
SEADROP_API Bytes serialize_transfer_accept(const TransferAcceptMessage &msg);

/**
 * @brief Deserialize transfer accept
 */
SEADROP_API Result<TransferAcceptMessage>
deserialize_transfer_accept(const Bytes &data);

/**
 * @brief Serialize file header
 */
SEADROP_API Bytes serialize_file_header(const FileHeaderMessage &msg);

/**
 * @brief Deserialize file header
 */
SEADROP_API Result<FileHeaderMessage>
deserialize_file_header(const Bytes &data);

/**
 * @brief Serialize file chunk header (data appended separately)
 */
SEADROP_API Bytes serialize_chunk_header(const FileChunkMessage &msg);

/**
 * @brief Deserialize file chunk header
 */
SEADROP_API Result<FileChunkMessage>
deserialize_chunk_header(const Bytes &data);

/**
 * @brief Serialize chunk acknowledgment
 */
SEADROP_API Bytes serialize_chunk_ack(const ChunkAckMessage &msg);

/**
 * @brief Deserialize chunk acknowledgment
 */
SEADROP_API Result<ChunkAckMessage> deserialize_chunk_ack(const Bytes &data);

/**
 * @brief Serialize progress message
 */
SEADROP_API Bytes serialize_progress(const ProgressMessage &msg);

/**
 * @brief Deserialize progress message
 */
SEADROP_API Result<ProgressMessage> deserialize_progress(const Bytes &data);

/**
 * @brief Serialize error message
 */
SEADROP_API Bytes serialize_error(const ErrorMessage &msg);

/**
 * @brief Deserialize error message
 */
SEADROP_API Result<ErrorMessage> deserialize_error(const Bytes &data);

// ============================================================================
// Packet Builder
// ============================================================================

/**
 * @brief Build a complete packet (header + payload)
 */
SEADROP_API Bytes build_packet(MessageType type, const Bytes &payload);

/**
 * @brief Parse incoming data stream for complete packets
 */
class SEADROP_API PacketParser {
public:
  PacketParser() = default;

  /**
   * @brief Feed data into parser
   * @param data Incoming bytes
   */
  void feed(const Bytes &data);

  /**
   * @brief Check if a complete packet is available
   */
  bool has_packet() const;

  /**
   * @brief Get next complete packet
   * @return Pair of (header, payload) or error
   */
  Result<std::pair<PacketHeader, Bytes>> next_packet();

  /**
   * @brief Reset parser state
   */
  void reset();

  /**
   * @brief Get buffered data size
   */
  size_t buffered_size() const;

private:
  Bytes buffer_;
  size_t parse_offset_ = 0;
};

} // namespace seadrop

#endif // SEADROP_PROTOCOL_H

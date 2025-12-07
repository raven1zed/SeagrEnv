/**
 * @file security.h
 * @brief Encryption and security primitives for SeaDrop
 *
 * SeaDrop uses libsodium for all cryptographic operations:
 * - X25519 for key exchange (Curve25519)
 * - XChaCha20-Poly1305 for authenticated encryption
 * - BLAKE2b for hashing and checksums
 * - Ed25519 for signatures (device identity)
 *
 * All transfers are end-to-end encrypted with perfect forward secrecy.
 */

#ifndef SEADROP_SECURITY_H
#define SEADROP_SECURITY_H

#include "error.h"
#include "platform.h"
#include "types.h"
#include <array>
#include <memory>


namespace seadrop {

// ============================================================================
// Constants
// ============================================================================

/// Size of public key (X25519)
constexpr size_t PUBLIC_KEY_SIZE = 32;

/// Size of secret key (X25519)
constexpr size_t SECRET_KEY_SIZE = 32;

/// Size of shared secret
constexpr size_t SHARED_SECRET_SIZE = 32;

/// Size of symmetric key (XChaCha20-Poly1305)
constexpr size_t SYMMETRIC_KEY_SIZE = 32;

/// Size of nonce (XChaCha20-Poly1305)
constexpr size_t NONCE_SIZE = 24;

/// Size of authentication tag
constexpr size_t AUTH_TAG_SIZE = 16;

/// Size of Ed25519 signature
constexpr size_t SIGNATURE_SIZE = 64;

/// Size of BLAKE2b hash (default)
constexpr size_t HASH_SIZE = 32;

// ============================================================================
// Key Types
// ============================================================================

/// X25519 public key
using PublicKey = std::array<Byte, PUBLIC_KEY_SIZE>;

/// X25519 secret key
using SecretKey = std::array<Byte, SECRET_KEY_SIZE>;

/// Symmetric encryption key
using SymmetricKey = std::array<Byte, SYMMETRIC_KEY_SIZE>;

/// Nonce for encryption
using Nonce = std::array<Byte, NONCE_SIZE>;

/// Ed25519 signing key
using SigningKey = std::array<Byte, 64>;

/// Ed25519 verify key
using VerifyKey = std::array<Byte, 32>;

/// Digital signature
using Signature = std::array<Byte, SIGNATURE_SIZE>;

/// BLAKE2b hash
using Hash = std::array<Byte, HASH_SIZE>;

// ============================================================================
// Key Pair
// ============================================================================

/**
 * @brief A cryptographic key pair
 */
struct KeyPair {
  PublicKey public_key;
  SecretKey secret_key;

  /// Generate a new random key pair
  static Result<KeyPair> generate();

  /// Load from bytes
  static Result<KeyPair> from_bytes(const Bytes &secret_key_bytes);

  /// Check if keys are valid (non-zero)
  bool is_valid() const;
};

/**
 * @brief Ed25519 signing key pair
 */
struct SigningKeyPair {
  VerifyKey verify_key;   // Public
  SigningKey signing_key; // Secret

  /// Generate a new random signing key pair
  static Result<SigningKeyPair> generate();

  /// Load from bytes
  static Result<SigningKeyPair> from_bytes(const Bytes &signing_key_bytes);
};

// ============================================================================
// Encryption/Decryption
// ============================================================================

/**
 * @brief Encrypt data using XChaCha20-Poly1305
 *
 * @param plaintext Data to encrypt
 * @param key Symmetric encryption key
 * @param associated_data Optional additional authenticated data (AAD)
 * @return Ciphertext (nonce + encrypted data + auth tag) or error
 *
 * The nonce is randomly generated and prepended to the ciphertext.
 */
SEADROP_API Result<Bytes> encrypt(const Bytes &plaintext,
                                  const SymmetricKey &key,
                                  const Bytes &associated_data = {});

/**
 * @brief Decrypt data using XChaCha20-Poly1305
 *
 * @param ciphertext Data to decrypt (nonce + encrypted + tag)
 * @param key Symmetric encryption key
 * @param associated_data Optional AAD (must match encryption)
 * @return Plaintext or error (if authentication fails)
 */
SEADROP_API Result<Bytes> decrypt(const Bytes &ciphertext,
                                  const SymmetricKey &key,
                                  const Bytes &associated_data = {});

/**
 * @brief Encrypt with explicit nonce (for protocols that manage nonces)
 */
SEADROP_API Result<Bytes> encrypt_with_nonce(const Bytes &plaintext,
                                             const SymmetricKey &key,
                                             const Nonce &nonce,
                                             const Bytes &associated_data = {});

/**
 * @brief Decrypt with explicit nonce
 */
SEADROP_API Result<Bytes> decrypt_with_nonce(const Bytes &ciphertext,
                                             const SymmetricKey &key,
                                             const Nonce &nonce,
                                             const Bytes &associated_data = {});

// ============================================================================
// Key Exchange
// ============================================================================

/**
 * @brief Perform X25519 key exchange
 *
 * @param our_secret Our secret key
 * @param their_public Their public key
 * @return Shared secret or error
 *
 * Both parties derive the same shared secret by exchanging public keys.
 */
SEADROP_API Result<SymmetricKey> key_exchange(const SecretKey &our_secret,
                                              const PublicKey &their_public);

/**
 * @brief Derive encryption key from shared secret using HKDF
 *
 * @param shared_secret Raw shared secret from key exchange
 * @param context Context string for domain separation
 * @param salt Optional salt (use device IDs for uniqueness)
 * @return Derived encryption key
 */
SEADROP_API Result<SymmetricKey> derive_key(const Bytes &shared_secret,
                                            const std::string &context,
                                            const Bytes &salt = {});

// ============================================================================
// Signatures
// ============================================================================

/**
 * @brief Sign a message using Ed25519
 *
 * @param message Message to sign
 * @param signing_key Secret signing key
 * @return Signature or error
 */
SEADROP_API Result<Signature> sign(const Bytes &message,
                                   const SigningKey &signing_key);

/**
 * @brief Verify an Ed25519 signature
 *
 * @param message Original message
 * @param signature Signature to verify
 * @param verify_key Public verification key
 * @return Success if valid, error if invalid
 */
SEADROP_API Result<void> verify_signature(const Bytes &message,
                                          const Signature &signature,
                                          const VerifyKey &verify_key);

// ============================================================================
// Hashing
// ============================================================================

/**
 * @brief Compute BLAKE2b hash
 *
 * @param data Data to hash
 * @param key Optional key for keyed hashing
 * @return Hash or error
 */
SEADROP_API Result<Hash> hash(const Bytes &data, const Bytes &key = {});

/**
 * @brief Compute hash of a file
 *
 * @param path Path to file
 * @return Hash or error
 */
SEADROP_API Result<Hash> hash_file(const std::string &path);

/**
 * @brief Compute hash incrementally (for large data)
 */
class SEADROP_API HashStream {
public:
  HashStream();
  ~HashStream();

  /// Initialize for hashing
  Result<void> init(const Bytes &key = {});

  /// Add data to hash
  Result<void> update(const Bytes &data);
  Result<void> update(const Byte *data, size_t length);

  /// Finalize and get hash
  Result<Hash> finalize();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Random Number Generation
// ============================================================================

/**
 * @brief Generate random bytes
 *
 * @param count Number of bytes to generate
 * @return Random bytes
 */
SEADROP_API Bytes random_bytes(size_t count);

/**
 * @brief Generate random 32-bit integer
 */
SEADROP_API uint32_t random_uint32();

/**
 * @brief Generate random integer in range [0, upper_bound)
 */
SEADROP_API uint32_t random_uniform(uint32_t upper_bound);

/**
 * @brief Generate random nonce
 */
SEADROP_API Nonce random_nonce();

// ============================================================================
// PIN Generation (for Pairing)
// ============================================================================

/**
 * @brief Generate a 6-digit pairing PIN
 *
 * @return PIN as string (e.g., "123456")
 */
SEADROP_API std::string generate_pairing_pin();

/**
 * @brief Derive verification code from shared secret
 *
 * Used to verify both devices computed the same shared secret.
 *
 * @param shared_secret Shared secret from key exchange
 * @return 6-digit verification code
 */
SEADROP_API std::string
derive_verification_code(const SymmetricKey &shared_secret);

// ============================================================================
// Secure Memory
// ============================================================================

/**
 * @brief Securely zero memory (won't be optimized away)
 */
SEADROP_API void secure_zero(void *ptr, size_t length);

/**
 * @brief Secure memory wrapper that zeros on destruction
 */
template <typename T> class SecureBuffer {
public:
  SecureBuffer() = default;
  explicit SecureBuffer(size_t size) : data_(size) {}
  ~SecureBuffer() { secure_zero(data_.data(), data_.size()); }

  // Non-copyable
  SecureBuffer(const SecureBuffer &) = delete;
  SecureBuffer &operator=(const SecureBuffer &) = delete;

  // Movable
  SecureBuffer(SecureBuffer &&other) noexcept : data_(std::move(other.data_)) {}
  SecureBuffer &operator=(SecureBuffer &&other) noexcept {
    if (this != &other) {
      secure_zero(data_.data(), data_.size());
      data_ = std::move(other.data_);
    }
    return *this;
  }

  T *data() { return data_.data(); }
  const T *data() const { return data_.data(); }
  size_t size() const { return data_.size(); }

private:
  std::vector<T> data_;
};

using SecureBytes = SecureBuffer<Byte>;

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize libsodium (called automatically on first use)
 * @return Success or error
 */
SEADROP_API Result<void> security_init();

/**
 * @brief Check if security module is initialized
 */
SEADROP_API bool is_security_initialized();

} // namespace seadrop

#endif // SEADROP_SECURITY_H

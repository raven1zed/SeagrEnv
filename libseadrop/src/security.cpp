/**
 * @file security.cpp
 * @brief libsodium wrapper implementation for SeaDrop
 */

#include "seadrop/security.h"
#include <cstring>
#include <atomic>
#include <fstream>
#include <sodium.h>

namespace seadrop {

namespace {
std::atomic<bool> g_initialized{false};
}

// ============================================================================
// Initialization
// ============================================================================

Result<void> security_init() {
  if (g_initialized.load()) {
    return Result<void>::ok();
  }

  if (sodium_init() < 0) {
    return Error(ErrorCode::SecurityError, "Failed to initialize libsodium");
  }

  g_initialized.store(true);
  return Result<void>::ok();
}

bool is_security_initialized() { return g_initialized.load(); }

// Auto-initialize on first crypto operation
static inline Result<void> ensure_initialized() {
  if (!g_initialized.load()) {
    auto result = security_init();
    if (result.is_error()) {
      return result;
    }
  }
  return Result<void>::ok();
}

// ============================================================================
// Key Pair Generation
// ============================================================================

Result<KeyPair> KeyPair::generate() {
  SEADROP_TRY(ensure_initialized());

  KeyPair kp;
  if (crypto_box_keypair(kp.public_key.data(), kp.secret_key.data()) != 0) {
    return Error(ErrorCode::SecurityError, "Failed to generate key pair");
  }

  return kp;
}

Result<KeyPair> KeyPair::from_bytes(const Bytes &secret_key_bytes) {
  if (secret_key_bytes.size() != SECRET_KEY_SIZE) {
    return Error(ErrorCode::InvalidArgument, "Invalid secret key size");
  }

  SEADROP_TRY(ensure_initialized());

  KeyPair kp;
  std::memcpy(kp.secret_key.data(), secret_key_bytes.data(), SECRET_KEY_SIZE);

  // Derive public key from secret key
  if (crypto_scalarmult_base(kp.public_key.data(), kp.secret_key.data()) != 0) {
    return Error(ErrorCode::SecurityError, "Failed to derive public key");
  }

  return kp;
}

bool KeyPair::is_valid() const {
  // Check if keys are non-zero
  bool public_zero = true;
  bool secret_zero = true;

  for (size_t i = 0; i < PUBLIC_KEY_SIZE; ++i) {
    if (public_key[i] != 0)
      public_zero = false;
    if (secret_key[i] != 0)
      secret_zero = false;
  }

  return !public_zero && !secret_zero;
}

Result<SigningKeyPair> SigningKeyPair::generate() {
  SEADROP_TRY(ensure_initialized());

  SigningKeyPair kp;
  if (crypto_sign_keypair(kp.verify_key.data(), kp.signing_key.data()) != 0) {
    return Error(ErrorCode::SecurityError,
                 "Failed to generate signing key pair");
  }

  return kp;
}

Result<SigningKeyPair>
SigningKeyPair::from_bytes(const Bytes &signing_key_bytes) {
  if (signing_key_bytes.size() != 64) {
    return Error(ErrorCode::InvalidArgument, "Invalid signing key size");
  }

  SEADROP_TRY(ensure_initialized());

  SigningKeyPair kp;
  std::memcpy(kp.signing_key.data(), signing_key_bytes.data(), 64);

  // Extract verify key from signing key (last 32 bytes contain it)
  std::memcpy(kp.verify_key.data(), kp.signing_key.data() + 32, 32);

  return kp;
}

// ============================================================================
// Encryption/Decryption
// ============================================================================

Result<Bytes> encrypt(const Bytes &plaintext, const SymmetricKey &key,
                      const Bytes &associated_data) {
  SEADROP_TRY(ensure_initialized());

  // Generate random nonce
  Nonce nonce;
  randombytes_buf(nonce.data(), NONCE_SIZE);

  // Prepare output: nonce + ciphertext + tag
  Bytes result(NONCE_SIZE + plaintext.size() +
               crypto_aead_xchacha20poly1305_ietf_ABYTES);

  // Copy nonce to beginning
  std::memcpy(result.data(), nonce.data(), NONCE_SIZE);

  unsigned long long ciphertext_len;

  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
          result.data() + NONCE_SIZE, &ciphertext_len, plaintext.data(),
          plaintext.size(),
          associated_data.empty() ? nullptr : associated_data.data(),
          associated_data.size(),
          nullptr, // nsec (not used)
          nonce.data(), key.data()) != 0) {
    return Error(ErrorCode::EncryptionFailed, "Encryption failed");
  }

  return result;
}

Result<Bytes> decrypt(const Bytes &ciphertext, const SymmetricKey &key,
                      const Bytes &associated_data) {
  SEADROP_TRY(ensure_initialized());

  if (ciphertext.size() <
      NONCE_SIZE + crypto_aead_xchacha20poly1305_ietf_ABYTES) {
    return Error(ErrorCode::DecryptionFailed, "Ciphertext too short");
  }

  // Extract nonce from beginning
  Nonce nonce;
  std::memcpy(nonce.data(), ciphertext.data(), NONCE_SIZE);

  // Prepare output
  Bytes plaintext(ciphertext.size() - NONCE_SIZE -
                  crypto_aead_xchacha20poly1305_ietf_ABYTES);

  unsigned long long plaintext_len;

  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          plaintext.data(), &plaintext_len,
          nullptr, // nsec (not used)
          ciphertext.data() + NONCE_SIZE, ciphertext.size() - NONCE_SIZE,
          associated_data.empty() ? nullptr : associated_data.data(),
          associated_data.size(), nonce.data(), key.data()) != 0) {
    return Error(ErrorCode::DecryptionFailed,
                 "Decryption failed - authentication error");
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

Result<Bytes> encrypt_with_nonce(const Bytes &plaintext,
                                 const SymmetricKey &key, const Nonce &nonce,
                                 const Bytes &associated_data) {
  SEADROP_TRY(ensure_initialized());

  Bytes ciphertext(plaintext.size() +
                   crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long ciphertext_len;

  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
          ciphertext.data(), &ciphertext_len, plaintext.data(),
          plaintext.size(),
          associated_data.empty() ? nullptr : associated_data.data(),
          associated_data.size(), nullptr, nonce.data(), key.data()) != 0) {
    return Error(ErrorCode::EncryptionFailed, "Encryption failed");
  }

  return ciphertext;
}

Result<Bytes> decrypt_with_nonce(const Bytes &ciphertext,
                                 const SymmetricKey &key, const Nonce &nonce,
                                 const Bytes &associated_data) {
  SEADROP_TRY(ensure_initialized());

  if (ciphertext.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES) {
    return Error(ErrorCode::DecryptionFailed, "Ciphertext too short");
  }

  Bytes plaintext(ciphertext.size() -
                  crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long plaintext_len;

  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          plaintext.data(), &plaintext_len, nullptr, ciphertext.data(),
          ciphertext.size(),
          associated_data.empty() ? nullptr : associated_data.data(),
          associated_data.size(), nonce.data(), key.data()) != 0) {
    return Error(ErrorCode::DecryptionFailed,
                 "Decryption failed - authentication error");
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

// ============================================================================
// Key Exchange
// ============================================================================

Result<SymmetricKey> key_exchange(const SecretKey &our_secret,
                                  const PublicKey &their_public) {
  SEADROP_TRY(ensure_initialized());

  SymmetricKey shared;

  if (crypto_scalarmult(shared.data(), our_secret.data(),
                        their_public.data()) != 0) {
    return Error(ErrorCode::KeyExchangeFailed, "Key exchange failed");
  }

  return shared;
}

Result<SymmetricKey> derive_key(const Bytes &shared_secret,
                                const std::string &context, const Bytes &salt) {
  SEADROP_TRY(ensure_initialized());

  SymmetricKey derived;

  // Use BLAKE2b for key derivation
  crypto_generichash_state state;

  if (crypto_generichash_init(&state, salt.empty() ? nullptr : salt.data(),
                              salt.size(), SYMMETRIC_KEY_SIZE) != 0) {
    return Error(ErrorCode::SecurityError, "Hash init failed");
  }

  if (crypto_generichash_update(&state, shared_secret.data(),
                                shared_secret.size()) != 0) {
    return Error(ErrorCode::SecurityError, "Hash update failed");
  }

  if (!context.empty()) {
    if (crypto_generichash_update(
            &state, reinterpret_cast<const uint8_t *>(context.data()),
            context.size()) != 0) {
      return Error(ErrorCode::SecurityError, "Hash update failed");
    }
  }

  if (crypto_generichash_final(&state, derived.data(), SYMMETRIC_KEY_SIZE) !=
      0) {
    return Error(ErrorCode::SecurityError, "Hash final failed");
  }

  return derived;
}

// ============================================================================
// Signatures
// ============================================================================

Result<Signature> sign(const Bytes &message, const SigningKey &signing_key) {
  SEADROP_TRY(ensure_initialized());

  Signature sig;
  unsigned long long sig_len;

  if (crypto_sign_detached(sig.data(), &sig_len, message.data(), message.size(),
                           signing_key.data()) != 0) {
    return Error(ErrorCode::SecurityError, "Signing failed");
  }

  return sig;
}

Result<void> verify_signature(const Bytes &message, const Signature &signature,
                              const VerifyKey &verify_key) {
  SEADROP_TRY(ensure_initialized());

  if (crypto_sign_verify_detached(signature.data(), message.data(),
                                  message.size(), verify_key.data()) != 0) {
    return Error(ErrorCode::InvalidSignature, "Signature verification failed");
  }

  return Result<void>::ok();
}

// ============================================================================
// Hashing
// ============================================================================

Result<Hash> hash(const Bytes &data, const Bytes &key) {
  SEADROP_TRY(ensure_initialized());

  Hash result;

  if (crypto_generichash(result.data(), HASH_SIZE, data.data(), data.size(),
                         key.empty() ? nullptr : key.data(), key.size()) != 0) {
    return Error(ErrorCode::SecurityError, "Hashing failed");
  }

  return result;
}

Result<Hash> hash_file(const std::string &path) {
  SEADROP_TRY(ensure_initialized());

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return Error(ErrorCode::FileReadError, "Cannot open file: " + path);
  }

  crypto_generichash_state state;
  if (crypto_generichash_init(&state, nullptr, 0, HASH_SIZE) != 0) {
    return Error(ErrorCode::SecurityError, "Hash init failed");
  }

  constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB
  std::vector<uint8_t> buffer(BUFFER_SIZE);

  while (file.read(reinterpret_cast<char *>(buffer.data()), BUFFER_SIZE) ||
         file.gcount() > 0) {
    if (crypto_generichash_update(&state, buffer.data(), file.gcount()) != 0) {
      return Error(ErrorCode::SecurityError, "Hash update failed");
    }
  }

  Hash result;
  if (crypto_generichash_final(&state, result.data(), HASH_SIZE) != 0) {
    return Error(ErrorCode::SecurityError, "Hash final failed");
  }

  return result;
}

// HashStream implementation
class HashStream::Impl {
public:
  crypto_generichash_state state;
  bool initialized = false;
  bool finalized = false;
};

HashStream::HashStream() : impl_(std::make_unique<Impl>()) {}
HashStream::~HashStream() = default;

Result<void> HashStream::init(const Bytes &key) {
  SEADROP_TRY(ensure_initialized());

  if (crypto_generichash_init(&impl_->state, key.empty() ? nullptr : key.data(),
                              key.size(), HASH_SIZE) != 0) {
    return Error(ErrorCode::SecurityError, "Hash init failed");
  }

  impl_->initialized = true;
  impl_->finalized = false;
  return Result<void>::ok();
}

Result<void> HashStream::update(const Bytes &data) {
  return update(data.data(), data.size());
}

Result<void> HashStream::update(const Byte *data, size_t length) {
  if (!impl_->initialized || impl_->finalized) {
    return Error(ErrorCode::InvalidState, "HashStream not in valid state");
  }

  if (crypto_generichash_update(&impl_->state, data, length) != 0) {
    return Error(ErrorCode::SecurityError, "Hash update failed");
  }

  return Result<void>::ok();
}

Result<Hash> HashStream::finalize() {
  if (!impl_->initialized || impl_->finalized) {
    return Error(ErrorCode::InvalidState, "HashStream not in valid state");
  }

  Hash result;
  if (crypto_generichash_final(&impl_->state, result.data(), HASH_SIZE) != 0) {
    return Error(ErrorCode::SecurityError, "Hash final failed");
  }

  impl_->finalized = true;
  return result;
}

// ============================================================================
// Random Number Generation
// ============================================================================

Bytes random_bytes(size_t count) {
  if (!g_initialized.load()) {
    security_init();
  }

  Bytes result(count);
  randombytes_buf(result.data(), count);
  return result;
}

uint32_t random_uint32() {
  if (!g_initialized.load()) {
    security_init();
  }
  return randombytes_random();
}

uint32_t random_uniform(uint32_t upper_bound) {
  if (!g_initialized.load()) {
    security_init();
  }
  return randombytes_uniform(upper_bound);
}

Nonce random_nonce() {
  Nonce nonce;
  if (!g_initialized.load()) {
    security_init();
  }
  randombytes_buf(nonce.data(), NONCE_SIZE);
  return nonce;
}

// ============================================================================
// PIN Generation
// ============================================================================

std::string generate_pairing_pin() {
  if (!g_initialized.load()) {
    security_init();
  }

  // Generate 6-digit PIN
  uint32_t pin = random_uniform(1000000);

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%06u", pin);
  return std::string(buffer);
}

std::string derive_verification_code(const SymmetricKey &shared_secret) {
  if (!g_initialized.load()) {
    security_init();
  }

  // Hash shared secret with context
  // Hash shared secret with context
  Hash h;
  const char *ctx = "SeaDrop-Verify";
  const Byte *start = reinterpret_cast<const Byte *>(ctx);
  Bytes context_bytes(start, start + 14);

  crypto_generichash(h.data(), 6, // Only need 6 bytes
                     shared_secret.data(), SYMMETRIC_KEY_SIZE,
                     context_bytes.data(), context_bytes.size());

  // Convert first 4 bytes to 6-digit number
  uint32_t val = (static_cast<uint32_t>(h[0]) << 16) |
                 (static_cast<uint32_t>(h[1]) << 8) |
                 static_cast<uint32_t>(h[2]);
  val %= 1000000;

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%06u", val);
  return std::string(buffer);
}

// ============================================================================
// Secure Memory
// ============================================================================

void secure_zero(void *ptr, size_t length) { sodium_memzero(ptr, length); }

} // namespace seadrop

/**
 * @file test_security.cpp
 * @brief Unit tests for the security module
 */

#include <gtest/gtest.h>
#include <seadrop/seadrop.h>

using namespace seadrop;

class SecurityTest : public ::testing::Test {
protected:
  void SetUp() override { security_init(); }
};

// ============================================================================
// Key Pair Generation
// ============================================================================

TEST_F(SecurityTest, GenerateKeyPair) {
  auto result = KeyPair::generate();
  ASSERT_TRUE(result.is_ok());

  auto &kp = result.value();
  EXPECT_TRUE(kp.is_valid());
}

TEST_F(SecurityTest, KeyPairFromBytes) {
  auto original = KeyPair::generate();
  ASSERT_TRUE(original.is_ok());

  Bytes secret_bytes(original.value().secret_key.begin(),
                     original.value().secret_key.end());

  auto restored = KeyPair::from_bytes(secret_bytes);
  ASSERT_TRUE(restored.is_ok());

  EXPECT_EQ(original.value().public_key, restored.value().public_key);
  EXPECT_EQ(original.value().secret_key, restored.value().secret_key);
}

// ============================================================================
// Encryption / Decryption
// ============================================================================

TEST_F(SecurityTest, EncryptDecryptBasic) {
  // Generate key
  auto kp = KeyPair::generate();
  ASSERT_TRUE(kp.is_ok());

  SymmetricKey key;
  std::copy(kp.value().secret_key.begin(), kp.value().secret_key.end(),
            key.begin());

  // Original message
  std::string message = "Hello, SeaDrop!";
  Bytes plaintext(message.begin(), message.end());

  // Encrypt
  auto encrypted = encrypt(plaintext, key);
  ASSERT_TRUE(encrypted.is_ok());
  EXPECT_GT(encrypted.value().size(), plaintext.size());

  // Decrypt
  auto decrypted = decrypt(encrypted.value(), key);
  ASSERT_TRUE(decrypted.is_ok());

  std::string result(decrypted.value().begin(), decrypted.value().end());
  EXPECT_EQ(message, result);
}

TEST_F(SecurityTest, EncryptWithAAD) {
  auto kp = KeyPair::generate();
  ASSERT_TRUE(kp.is_ok());

  SymmetricKey key;
  std::copy(kp.value().secret_key.begin(), kp.value().secret_key.end(),
            key.begin());

  std::string message = "Sensitive data";
  Bytes plaintext(message.begin(), message.end());

  std::string aad_str = "additional authenticated data";
  Bytes aad(aad_str.begin(), aad_str.end());

  // Encrypt with AAD
  auto encrypted = encrypt(plaintext, key, aad);
  ASSERT_TRUE(encrypted.is_ok());

  // Decrypt with correct AAD
  auto decrypted = decrypt(encrypted.value(), key, aad);
  ASSERT_TRUE(decrypted.is_ok());

  // Decrypt with wrong AAD should fail
  std::string wrong_aad = "wrong aad";
  auto failed = decrypt(encrypted.value(), key,
                        Bytes(wrong_aad.begin(), wrong_aad.end()));
  EXPECT_TRUE(failed.is_error());
  EXPECT_EQ(failed.error().code, ErrorCode::DecryptionFailed);
}

TEST_F(SecurityTest, DecryptTamperedCiphertext) {
  auto kp = KeyPair::generate();
  ASSERT_TRUE(kp.is_ok());

  SymmetricKey key;
  std::copy(kp.value().secret_key.begin(), kp.value().secret_key.end(),
            key.begin());

  std::string message = "Important message";
  Bytes plaintext(message.begin(), message.end());

  auto encrypted = encrypt(plaintext, key);
  ASSERT_TRUE(encrypted.is_ok());

  // Tamper with ciphertext
  auto &ciphertext = encrypted.value();
  if (!ciphertext.empty()) {
    ciphertext[ciphertext.size() / 2] ^= 0xFF;
  }

  // Decryption should fail
  auto result = decrypt(ciphertext, key);
  EXPECT_TRUE(result.is_error());
}

// ============================================================================
// Key Exchange
// ============================================================================

TEST_F(SecurityTest, KeyExchange) {
  // Generate two key pairs
  auto alice_keys = KeyPair::generate();
  auto bob_keys = KeyPair::generate();
  ASSERT_TRUE(alice_keys.is_ok());
  ASSERT_TRUE(bob_keys.is_ok());

  // Alice computes shared secret with Bob's public key
  auto alice_shared =
      key_exchange(alice_keys.value().secret_key, bob_keys.value().public_key);
  ASSERT_TRUE(alice_shared.is_ok());

  // Bob computes shared secret with Alice's public key
  auto bob_shared =
      key_exchange(bob_keys.value().secret_key, alice_keys.value().public_key);
  ASSERT_TRUE(bob_shared.is_ok());

  // Both should derive the same shared secret
  EXPECT_EQ(alice_shared.value(), bob_shared.value());
}

// ============================================================================
// Hashing
// ============================================================================

TEST_F(SecurityTest, HashBasic) {
  std::string data = "Test data for hashing";
  Bytes bytes(data.begin(), data.end());

  auto result = hash(bytes);
  ASSERT_TRUE(result.is_ok());
  EXPECT_EQ(result.value().size(), HASH_SIZE);
}

TEST_F(SecurityTest, HashDeterministic) {
  std::string data = "Same data";
  Bytes bytes(data.begin(), data.end());

  auto hash1 = hash(bytes);
  auto hash2 = hash(bytes);

  ASSERT_TRUE(hash1.is_ok());
  ASSERT_TRUE(hash2.is_ok());
  EXPECT_EQ(hash1.value(), hash2.value());
}

TEST_F(SecurityTest, HashDifferentData) {
  Bytes data1 = {1, 2, 3, 4, 5};
  Bytes data2 = {1, 2, 3, 4, 6}; // One byte different

  auto hash1 = hash(data1);
  auto hash2 = hash(data2);

  ASSERT_TRUE(hash1.is_ok());
  ASSERT_TRUE(hash2.is_ok());
  EXPECT_NE(hash1.value(), hash2.value());
}

// ============================================================================
// Signatures
// ============================================================================

TEST_F(SecurityTest, SignAndVerify) {
  auto keys = SigningKeyPair::generate();
  ASSERT_TRUE(keys.is_ok());

  std::string message = "Message to sign";
  Bytes msg(message.begin(), message.end());

  // Sign
  auto signature = sign(msg, keys.value().signing_key);
  ASSERT_TRUE(signature.is_ok());
  EXPECT_EQ(signature.value().size(), SIGNATURE_SIZE);

  // Verify
  auto verify_result =
      verify_signature(msg, signature.value(), keys.value().verify_key);
  EXPECT_TRUE(verify_result.is_ok());
}

TEST_F(SecurityTest, VerifyTamperedSignature) {
  auto keys = SigningKeyPair::generate();
  ASSERT_TRUE(keys.is_ok());

  std::string message = "Original message";
  Bytes msg(message.begin(), message.end());

  auto signature = sign(msg, keys.value().signing_key);
  ASSERT_TRUE(signature.is_ok());

  // Tamper with message
  std::string tampered = "Tampered message";
  Bytes tampered_msg(tampered.begin(), tampered.end());

  auto result = verify_signature(tampered_msg, signature.value(),
                                 keys.value().verify_key);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code, ErrorCode::InvalidSignature);
}

// ============================================================================
// Random Generation
// ============================================================================

TEST_F(SecurityTest, RandomBytes) {
  auto bytes1 = random_bytes(32);
  auto bytes2 = random_bytes(32);

  EXPECT_EQ(bytes1.size(), 32u);
  EXPECT_EQ(bytes2.size(), 32u);

  // Should be different (with overwhelming probability)
  EXPECT_NE(bytes1, bytes2);
}

TEST_F(SecurityTest, PairingPin) {
  auto pin1 = generate_pairing_pin();
  auto pin2 = generate_pairing_pin();

  EXPECT_EQ(pin1.length(), 6u);
  EXPECT_EQ(pin2.length(), 6u);

  // All digits
  for (char c : pin1) {
    EXPECT_TRUE(c >= '0' && c <= '9');
  }
}

// ============================================================================
// Verification Code
// ============================================================================

TEST_F(SecurityTest, VerificationCodeDeterministic) {
  auto kp = KeyPair::generate();
  ASSERT_TRUE(kp.is_ok());

  SymmetricKey key;
  std::copy(kp.value().secret_key.begin(), kp.value().secret_key.end(),
            key.begin());

  auto code1 = derive_verification_code(key);
  auto code2 = derive_verification_code(key);

  EXPECT_EQ(code1, code2);
  EXPECT_EQ(code1.length(), 6u);
}

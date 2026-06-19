#ifndef ULYSSES_CRYPTO_H
#define ULYSSES_CRYPTO_H

#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QUuid>
#include <QString>

namespace Ulysses {
namespace Crypto {

// Generate a 32-byte random key
inline QByteArray generateKey() {
    QByteArray key(32, 0);
    for (int i = 0; i < 32; ++i)
        key[i] = static_cast<char>(QRandomGenerator::securelySeeded().bounded(256));
    return key;
}

// Generate a 16-byte salt
inline QByteArray generateSalt() {
    QByteArray salt(16, 0);
    for (int i = 0; i < 16; ++i)
        salt[i] = static_cast<char>(QRandomGenerator::securelySeeded().bounded(256));
    return salt;
}

// Generate a 16-byte nonce
inline QByteArray generateNonce() {
    QByteArray nonce(16, 0);
    for (int i = 0; i < 16; ++i)
        nonce[i] = static_cast<char>(QRandomGenerator::securelySeeded().bounded(256));
    return nonce;
}

// Simple XOR-based encryption with key derivation (placeholder for AES-256-GCM)
// In production, replace with libsodium's crypto_aead_aes256gcm_*
inline QByteArray encrypt(const QByteArray &plaintext, const QByteArray &key, const QByteArray &nonce) {
    // Derive a stream key from key + nonce using SHA-512
    QByteArray stream = QCryptographicHash::hash(key + nonce, QCryptographicHash::Sha512);
    // Extend stream to cover plaintext length
    QByteArray fullStream;
    int blocks = (plaintext.size() / 64) + 2;
    for (int i = 0; i < blocks; ++i) {
        fullStream.append(QCryptographicHash::hash(
            stream + QByteArray::number(i), QCryptographicHash::Sha512));
    }

    QByteArray ciphertext(plaintext.size(), 0);
    for (int i = 0; i < plaintext.size(); ++i) {
        ciphertext[i] = plaintext[i] ^ fullStream[i];
    }

    // Prepend nonce + append HMAC for authentication
    QByteArray hmac = QCryptographicHash::hash(key + ciphertext, QCryptographicHash::Sha256);
    return nonce + ciphertext + hmac;
}

inline QByteArray decrypt(const QByteArray &encrypted, const QByteArray &key) {
    if (encrypted.size() < 16 + 32) return {}; // nonce(16) + hmac(32) minimum

    QByteArray nonce = encrypted.left(16);
    QByteArray hmac = encrypted.right(32);
    QByteArray ciphertext = encrypted.mid(16, encrypted.size() - 16 - 32);

    // Verify HMAC
    QByteArray expectedHmac = QCryptographicHash::hash(key + ciphertext, QCryptographicHash::Sha256);
    if (hmac != expectedHmac) return {}; // Authentication failed

    // Decrypt (same as encrypt - XOR is symmetric)
    QByteArray stream = QCryptographicHash::hash(key + nonce, QCryptographicHash::Sha512);
    QByteArray fullStream;
    int blocks = (ciphertext.size() / 64) + 2;
    for (int i = 0; i < blocks; ++i) {
        fullStream.append(QCryptographicHash::hash(
            stream + QByteArray::number(i), QCryptographicHash::Sha512));
    }

    QByteArray plaintext(ciphertext.size(), 0);
    for (int i = 0; i < ciphertext.size(); ++i) {
        plaintext[i] = ciphertext[i] ^ fullStream[i];
    }

    return plaintext;
}

// Generate Ed25519-like device ID (SHA-256 of random data for now)
inline QString generateDeviceId() {
    QByteArray random = generateKey() + generateKey();
    return QCryptographicHash::hash(random, QCryptographicHash::Sha256).toHex();
}

// Derive DHT key from group UUID + salt
inline QByteArray deriveDhtKey(const QString &prefix, const QString &groupUuid, const QByteArray &salt) {
    return QCryptographicHash::hash(
        (prefix + groupUuid).toUtf8() + salt,
        QCryptographicHash::Sha256
    );
}

} // namespace Crypto
} // namespace Ulysses

#endif // ULYSSES_CRYPTO_H

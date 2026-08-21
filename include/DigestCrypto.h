#pragma once
#include <stddef.h>

namespace DigestCrypto{
    constexpr size_t SHA256_HEX_LEN = 64;
    constexpr size_t NONCE_HEX_LEN  = 16;
    constexpr size_t REALM_BUF_LEN  = 32;

    void sha256Hex(const char *input, char *output, size_t inLen);
    void buildRealm (char *out, size_t len);
    void generateNonce(char *out, size_t len);
        /**
     * @brief computes Ha1 for the digest auth challenge and fills it in the given buffer
     * @param password given password
     * @param out buffer for ha1, length 64 hex + '\0'
     * @param outLen buffer size, must be >= 65
     */
    void computeHa1(const char *password, char *out, size_t outLen);
}
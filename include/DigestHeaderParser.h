#pragma once
#include <stddef.h>

namespace DigestHeaderParser
{
    /**
     * @brief Extract a Digest-Auth field's value from an Authorization header line.
     * @param line Header line to search (the full Authorization header value).
     * @param key Field name to find, e.g. "nonce".
     * @param dest Destination buffer, always left null-terminated.
     * @param destSize Destination buffer size; result is truncated to fit.
     * @return true if the field was found (dest filled), false if absent
     *         (dest set to "").
     * @note RFC 7616 quotes most fields (realm, nonce, response, uri, cnonce,
     *       opaque) but leaves a few unquoted (algorithm, nc) -- tries
     *       key="value" first, then falls back to unquoted key=value.
     * @note Pulled out of DigestAuth into its own dependency-free module:
     *       it's pure string parsing (no crypto, no NVS, no HTTP), unlike
     *       the rest of DigestAuth::verify() which needs DigestCrypto's
     *       SHA-256. Same layering principle as the DigestCrypto/DigestAuth
     *       split (see CLAUDE.md) -- and it means this specific parsing
     *       logic, the most bug-prone hand-written part of the auth chain,
     *       is unit-testable without pulling in mbedtls.
     */
    bool extractValue(const char *line, const char *key, char *dest, size_t destSize);
}

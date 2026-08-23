#pragma once
#include <stddef.h>

namespace DigestAuth
{
    // Max content length (excl. '\0') that verify() actually hashes into HA2
    // for method/path — public so callers (HttpServer) can size their own
    // method[]/path[] buffers directly from these instead of picking numbers
    // independently. A mismatch here would silently truncate the HA2 input
    // and make auth fail for no visible reason.
    constexpr size_t MAX_METHOD_LEN = 7;  // only GET/POST are routed today, plenty of headroom
    constexpr size_t MAX_URI_LEN = 63;    // longest current path ("/provision/wifi/reset") is 22 chars

    /**
     * @brief Build the WWW-Authenticate Header for 401 challenge
     * @param out buffer
     * @param len buffer size
     * @note realm is redived from device MAC; nonce generated
     */
    void buildWwwAuthenticate(char *out, size_t len);

    /**
     * @brief Verify a DigesAuth Authorization header against stored Ha1.
     * @param authHeader Full Authorization header line from request.
     * @param method HTTP method of the request.
     * @param path Actual request path as received (e.g. "/status"), NOT the
     *        client-declared uri= field from the header. Matches the Shelly
     *        Gen2/3 server convention the MainUnit client is built against.
     * @param ha1 stored ha1 to verify.
     * @return true if response hash matches, false otherwise.
     */
    bool verify(const char *authHeader, const char *method, const char *path, const char *ha1);
}

#pragma once
#include <stddef.h>

namespace DigestAuth
{

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

#include "DigestAuth.h"
#include <Arduino.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include "InternalStorage.h"
#include <esp_random.h>
#include "DigestCrypto.h"

namespace {
    constexpr size_t KEY_BUF_LEN = 24;

    constexpr size_t NC_BUF_LEN =16;
    constexpr size_t CNONCE_BUF_LEN = 16;
    constexpr size_t URI_BUF_LEN = 64;
    constexpr size_t RESPONSE_INPUT_BUF_LEN = 192;

    constexpr size_t HA2_INPUT_BUF_LEN = 8+URI_BUF_LEN; //URI_BUF_LEN + 8 (method)

    // @warning '\0' not included
 
    bool extractValue(const char *line, const char *key, char *dest, size_t destSize)
    {
        char keyQ[KEY_BUF_LEN];
        snprintf(keyQ, sizeof(keyQ), "%s=\"", key);
        const char *start = strstr(line, keyQ);
        if (start)
        {
            start += strlen(keyQ);
            const char *end = strchr(start, '"');
            if (end)
            {
                size_t length = (size_t)(end - start);
                if (length >= destSize) length = destSize - 1;
                memcpy(dest, start, length);
                dest[length] = '\0';
                return true;
            }
        }
        char keyP[KEY_BUF_LEN];
        snprintf(keyP, sizeof(keyP), "%s=", key);
        start = strstr(line, keyP);
        if (start)
        {
            start += strlen(keyP);
            const char *end = strpbrk(start, ", \r\n");
            size_t length = end ? (size_t)(end - start) : strlen(start);
            if (length >= destSize) length = destSize - 1;
            memcpy(dest, start, length);
            dest[length] = '\0';
            return true;
        }
        dest[0] = '\0';
        return false;
    }
}
namespace DigestAuth {


    void buildWwwAuthenticate(char *out, size_t len){
        char realm[DigestCrypto::REALM_BUF_LEN];
        DigestCrypto::buildRealm(realm, sizeof(realm));
        char nonce[DigestCrypto::NONCE_HEX_LEN + 1];
        DigestCrypto::generateNonce(nonce,sizeof(nonce));
        snprintf(out, len,
                 "Digest realm=\"%s\", qop=\"auth\", algorithm=SHA-256, nonce=\"%s\"",
                 realm, nonce);
    }

    bool verify(const char *authHeader, const char *method, const char *path, const char *ha1){
        char nonce[DigestCrypto::NONCE_HEX_LEN+1]={};
        char nc[NC_BUF_LEN] ={};
        char cnonce[CNONCE_BUF_LEN+1] ={};
        char response[DigestCrypto::SHA256_HEX_LEN+1]={};

        if (!extractValue(authHeader, "nonce",    nonce,    sizeof(nonce))    ||
            !extractValue(authHeader, "nc",       nc,       sizeof(nc))       ||
            !extractValue(authHeader, "cnonce",   cnonce,   sizeof(cnonce))   ||
            !extractValue(authHeader, "response", response, sizeof(response)))
        {
            return false;
        }
        if(ha1[0]=='\0'){
            return false;
        }
        char ha2Input[HA2_INPUT_BUF_LEN];
        snprintf(ha2Input, sizeof(ha2Input),"%s:%s",method, path);
        char ha2[DigestCrypto::SHA256_HEX_LEN+1]={};
        DigestCrypto::sha256Hex(ha2Input,ha2,strlen(ha2Input));

        char responseInput[RESPONSE_INPUT_BUF_LEN]={};
        snprintf(responseInput,sizeof(responseInput),
            "%s:%s:%s:%s:auth:%s",ha1,nonce,nc,cnonce,ha2);
        char expected[DigestCrypto::SHA256_HEX_LEN+1]={};
        DigestCrypto::sha256Hex(responseInput,expected,strlen(responseInput));

        return memcmp(expected,response,DigestCrypto::SHA256_HEX_LEN)==0;
    }

}
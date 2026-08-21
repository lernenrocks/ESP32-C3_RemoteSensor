#include <Arduino.h>
#include "DigestCrypto.h"
#include <mbedtls/sha256.h>
#include <esp_random.h>
#include <WiFi.h>

namespace {
    constexpr const char *AUTH_USER = "admin";
}

namespace DigestCrypto{
       void sha256Hex(const char *input, char *output, size_t inLen){
        uint8_t digest[32];
        mbedtls_sha256((const uint8_t *)input,inLen,digest,0); //argument 0: use SHA-256, not 224
        for (size_t i=0;i<sizeof(digest);i++){
            sprintf(output+i*2,"%02x",digest[i]);
        }
    }

    void buildRealm (char *out, size_t len){
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(out, len, "terracontrol-c3-%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    void generateNonce(char *out, size_t len){
        snprintf(out, len, "%08x%08x", esp_random(), esp_random());
    }
    
    void computeHa1(const char *password, char *out, size_t outLen){
        if (outLen < SHA256_HEX_LEN + 1){
            if (outLen > 0) out[0] = '\0';
            return;
        }
        char realm[REALM_BUF_LEN];
        DigestCrypto::buildRealm(realm,sizeof(realm));
        char input[128];
        snprintf(input,sizeof(input),"%s:%s:%s",AUTH_USER,realm,password);
        DigestCrypto::sha256Hex(input,out,strlen(input));
    }

}
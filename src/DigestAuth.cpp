#include "DigestAuth.h"
#include <Arduino.h>
#include "DigestCrypto.h"
#include "DigestHeaderParser.h"

namespace {
    constexpr size_t NC_BUF_LEN = 16;
    constexpr size_t CNONCE_BUF_LEN = 16;
    constexpr size_t RESPONSE_INPUT_BUF_LEN = 192;

    // +1 each for the ':' separator and the '\0' terminator.
    constexpr size_t HA2_INPUT_BUF_LEN = DigestAuth::MAX_METHOD_LEN + 1 + DigestAuth::MAX_URI_LEN + 1;
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
        char nc[NC_BUF_LEN+1] ={};
        char cnonce[CNONCE_BUF_LEN+1] ={};
        char response[DigestCrypto::SHA256_HEX_LEN+1]={};

        if (!DigestHeaderParser::extractValue(authHeader, "nonce",    nonce,    sizeof(nonce))    ||
            !DigestHeaderParser::extractValue(authHeader, "nc",       nc,       sizeof(nc))       ||
            !DigestHeaderParser::extractValue(authHeader, "cnonce",   cnonce,   sizeof(cnonce))   ||
            !DigestHeaderParser::extractValue(authHeader, "response", response, sizeof(response)))
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
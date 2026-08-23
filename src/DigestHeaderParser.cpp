#include "DigestHeaderParser.h"
#include <cstring>
#include <cstdio>
#include <cctype>

namespace
{
    constexpr size_t KEY_BUF_LEN = 24;

    // Word-boundary-aware strstr: rejects a match whose preceding character
    // is alphanumeric, so a search for "nonce=" can't match inside
    // "cnonce=..." -- RFC 7616 doesn't fix a field order, so a client
    // sending cnonce before nonce is legal and would otherwise silently
    // extract the wrong field.
    const char *findFieldStart(const char *line, const char *pattern)
    {
        const char *search = line;
        while ((search = strstr(search, pattern)) != nullptr)
        {
            if (search == line || !isalnum((unsigned char)*(search - 1)))
            {
                return search;
            }
            search++;
        }
        return nullptr;
    }
}

namespace DigestHeaderParser
{
    bool extractValue(const char *line, const char *key, char *dest, size_t destSize)
    {
        char keyQ[KEY_BUF_LEN];
        snprintf(keyQ, sizeof(keyQ), "%s=\"", key);
        const char *start = findFieldStart(line, keyQ);
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
        start = findFieldStart(line, keyP);
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

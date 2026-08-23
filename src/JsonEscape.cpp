#include "JsonEscape.h"
#include <cstring>
#include <cstdio>

namespace JsonEscape
{
    void escape(const char *in, char *out, size_t outLen)
    {
        if (outLen == 0) return; // nothing to write into
        size_t o = 0;
        for (size_t i = 0; in[i] != '\0' && o + 1 < outLen; i++)
        {
            unsigned char c = (unsigned char)in[i];
            const char *rep = nullptr;
            switch (c)
            {
                case '"': rep = "\\\""; break;
                case '\\': rep = "\\\\"; break;
                case '\n': rep = "\\n"; break;
                case '\r': rep = "\\r"; break;
                case '\t': rep = "\\t"; break;
            }
            if (rep)
            {
                size_t rl = strlen(rep);
                if (o + rl >= outLen) break;
                memcpy(out + o, rep, rl);
                o += rl;
            }
            else if (c < 0x20)
            {
                if (o + 6 >= outLen) break;
                o += snprintf(out + o, outLen - o, "\\u%04x", c);
            }
            else
            {
                out[o++] = (char)c;
            }
        }
        out[o] = '\0';
    }
}

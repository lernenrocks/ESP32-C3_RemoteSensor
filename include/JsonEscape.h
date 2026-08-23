#pragma once
#include <stddef.h>

namespace JsonEscape
{
    /**
     * @brief Escape a C string for embedding into hand-built JSON.
     * @param in Null-terminated input string.
     * @param out Destination buffer, always left null-terminated.
     * @param outLen Destination buffer size.
     * @note Escapes \", \\, \n, \r, \t and other control chars as \uXXXX, per
     *       CLAUDE.md's JSON-escaping rule. If out doesn't fit the full
     *       result, it's truncated -- but never mid-escape-sequence (a
     *       multi-char escape that wouldn't fully fit is dropped whole,
     *       rather than writing half of it).
     */
    void escape(const char *in, char *out, size_t outLen);
}

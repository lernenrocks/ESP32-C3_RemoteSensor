#pragma once
#include <Arduino.h>

namespace InternalStorage
{
    void erase();

    bool begin(const char *pref_namespace, bool readOnly);
    void end();

    bool writeBool(const char *key, const bool value);
    bool writeLong(const char *key, const long value);
    bool writeString(const char *key, const char *value);
    bool writeFloat(const char *key, const float value);

    bool readBool(const char *key, bool &value);
    bool readLong(const char *key, long &value);
    bool readString(const char *key, char *value, size_t len);
    bool readFloat(const char *key, float &value);

    /**
     * @brief RAII wrapper around begin()/end() — opens in the constructor,
     *        closes automatically in the destructor, even on an early return
     *        from the enclosing scope. Fixes a real risk with the manual
     *        begin()/end() pairing above: a future edit that adds a return
     *        between them would silently skip end(), leaving the NVS
     *        namespace open with no visible error.
     * @note This is a class *inside a namespace* (InternalStorage stays a
     *       namespace, per this project's convention), not a nested/inner
     *       class — a nested class lives inside another *class* and gets
     *       special access to its private members; a class in a namespace
     *       gets none of that, it's just grouped there like any free
     *       function here. Outlook, not needed for this course: C++ nested
     *       classes are a real, different thing worth knowing exists.
     * @note Stack-only, no heap involved — same as any other local variable
     *       in this codebase (no `new`, so no fragmentation risk).
     */
    class Session
    {
    public:
        Session(const char *pref_namespace, bool readOnly) { begin(pref_namespace, readOnly); }
        ~Session() { end(); }
        // Copying would run end() twice (once per destructor call) once both
        // copies go out of scope — forbidden the modern (C++11) way. Before
        // = delete existed, this was done by declaring the copy constructor
        // private and never defining it, which only failed at compile/link
        // time with a less direct error.
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;
    };
}
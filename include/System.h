#pragma once
#include <Arduino.h>
namespace System
{
    /// Puffergroesse fuer den Geraetenamen — zentral hier statt pro Aufrufer
    /// dupliziert, analog zu DigestCrypto::SHA256_HEX_LEN.
    constexpr size_t DEVICE_NAME_LEN = 32;

    /**
     * @brief Load the user-assigned device name from NVS.
     * @param buffer Destination buffer.
     * @param len    Buffer size.
     */
    void loadDeviceName(char *buffer, size_t len);

    /**
     * @brief Store a user-assigned device name in NVS.
     * @param name Plaintext device name.
     * @note Purely cosmetic — used for the AP SSID, STA hostname and the
     *       `device_name` field in GET /status. Never part of the Digest Auth
     *       realm or username, and never required to match between C3 and
     *       MainUnit.
     */
    void storeDeviceName(const char *name);

    /**
     * @brief Load the name currently shown for the device — user's own if set, else a MAC-based default.
     * @param buffer Destination buffer, always filled with a usable name. Size >= DEVICE_NAME_LEN.
     * @param len    Buffer size.
     * @return true if the user's own name is in use, false if still on the MAC-based default.
     * @note Rein kosmetisch — die AP-SSID bleibt bewusst MAC-basiert (siehe CLAUDE.md,
     *       Abschnitt Provisioning), damit die MainUnit den Knoten zuverlaessig
     *       wiederfindet, unabhaengig vom user-aenderbaren Namen.
     */
    bool getActiveDeviceName(char *buffer, size_t len);

    /**
     * @brief Write the compiled-in default device password into buffer.
     * @param buffer Destination buffer.
     * @param len    Buffer size.
     * @note Reads from initialPW.h, not NVS — this value is never stored,
     *       only computed/copied on demand while no user password exists.
     */
    void provideDeviceDefaultPassword(char *buffer, size_t len);

    /**
     * @brief Load the currently active (user-set) device password from NVS.
     * @param buffer Destination buffer.
     * @param len    Buffer size.
     * @note Plaintext, not the hash — needed to reconfigure the AP's WPA2
     *       credentials, which Digest Auth's HA1 alone cannot provide.
     */
    void loadDevicePassword(char *buffer, size_t len);

    /**
     * @brief Store the user's chosen device password in NVS.
     * @param password Plaintext password.
     */
    void storeDevicePassword(const char *password);

    /**
     * @brief Store the HA1 hash of the user's chosen device password in NVS.
     * @param ha1 Precomputed SHA-256 hash.
     */
    void storeDeviceHa1(const char *ha1);

    /**
     * @brief Load the Ha1 currently valid for auth — user's own if set, else the compiled-in default.
     * @param buffer Destination buffer, always filled with a usable Ha1.
     * @param len    Buffer size, must be >= 65.
     * @return true if the user's own password is in use, false if still on default.
     */
    bool getActiveHa1(char *buffer, size_t len);

    /**
     * @brief Load the password currently valid for the AP's WPA2 — user's own if set, else the compiled-in default.
     * @param buffer Destination buffer, always filled with a usable password.
     * @param len    Buffer size.
     * @return true if the user's own password is in use, false if still on default.
     */
    bool getActivePassword(char *buffer, size_t len);

}
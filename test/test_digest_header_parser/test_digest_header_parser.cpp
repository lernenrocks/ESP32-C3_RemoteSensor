#include <unity.h>
#include "DigestHeaderParser.h"
#include <cstring>

void setUp() {}
void tearDown() {}

void test_extracts_quoted_value()
{
    char dest[32] = {};
    bool found = DigestHeaderParser::extractValue(
        "nonce=\"abc123\", qop=\"auth\"", "nonce", dest, sizeof(dest));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("abc123", dest);
}

void test_extracts_unquoted_value_stopping_at_comma()
{
    char dest[32] = {};
    bool found = DigestHeaderParser::extractValue(
        "algorithm=SHA-256, nc=00000001, cnonce=\"xyz\"", "nc", dest, sizeof(dest));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("00000001", dest);
}

void test_extracts_unquoted_value_at_end_of_string()
{
    char dest[32] = {};
    bool found = DigestHeaderParser::extractValue(
        "qop=\"auth\", algorithm=SHA-256", "algorithm", dest, sizeof(dest));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("SHA-256", dest);
}

void test_returns_false_and_empties_dest_when_key_missing()
{
    char dest[32];
    strcpy(dest, "stale");
    bool found = DigestHeaderParser::extractValue(
        "realm=\"x\", nonce=\"y\"", "response", dest, sizeof(dest));
    TEST_ASSERT_FALSE(found);
    TEST_ASSERT_EQUAL_STRING("", dest);
}

void test_truncates_quoted_value_to_fit_dest()
{
    char dest[4] = {}; // room for 3 chars + '\0'
    bool found = DigestHeaderParser::extractValue(
        "nonce=\"abcdef\"", "nonce", dest, sizeof(dest));
    TEST_ASSERT_TRUE(found); // truncated, not rejected
    TEST_ASSERT_EQUAL_STRING("abc", dest);
}

// "cnonce" ends with the letters "nonce" -- a naive substring search for key
// "nonce" can match inside "cnonce=...", returning the wrong field's value.
// A real client is free to put cnonce before nonce in the header (RFC 7616
// doesn't mandate field order), so this isn't just a theoretical case.
void test_does_not_match_nonce_inside_cnonce()
{
    char dest[32] = {};
    bool found = DigestHeaderParser::extractValue(
        "cnonce=\"wrong\", nonce=\"right\"", "nonce", dest, sizeof(dest));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("right", dest);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_extracts_quoted_value);
    RUN_TEST(test_extracts_unquoted_value_stopping_at_comma);
    RUN_TEST(test_extracts_unquoted_value_at_end_of_string);
    RUN_TEST(test_returns_false_and_empties_dest_when_key_missing);
    RUN_TEST(test_truncates_quoted_value_to_fit_dest);
    RUN_TEST(test_does_not_match_nonce_inside_cnonce);
    return UNITY_END();
}

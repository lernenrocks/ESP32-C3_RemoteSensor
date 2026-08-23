#include <unity.h>
#include "JsonEscape.h"
#include <cstring>

void setUp() {}
void tearDown() {}

void test_escapes_special_chars()
{
    char out[32] = {};
    JsonEscape::escape("a\"b\\c\nd\re\tf", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("a\\\"b\\\\c\\nd\\re\\tf", out);
}

void test_escapes_other_control_chars_as_unicode()
{
    char out[16] = {};
    char in[2] = { (char)0x01, '\0' };
    JsonEscape::escape(in, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("\\u0001", out);
}

void test_passes_through_plain_text_unchanged()
{
    char out[16] = {};
    JsonEscape::escape("hello", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("hello", out);
}

void test_empty_input_yields_empty_output()
{
    char out[8] = {};
    JsonEscape::escape("", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_truncates_when_buffer_too_small()
{
    char out[4] = {}; // room for 3 chars + '\0'
    JsonEscape::escape("hello", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("hel", out);
}

void test_does_not_split_an_escape_sequence_across_the_truncation_boundary()
{
    // Only 1 usable slot left before the '"' -- its 2-char escape ("\\\"")
    // doesn't fit, so it must be dropped whole, not written half-finished.
    char out[3] = {};
    JsonEscape::escape("ab\"c", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ab", out);
}

void test_zero_length_buffer_does_not_write_anything()
{
    char out[1] = { 'X' };
    JsonEscape::escape("hello", out, 0);
    TEST_ASSERT_EQUAL_CHAR('X', out[0]); // must not touch out[] at all
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_escapes_special_chars);
    RUN_TEST(test_escapes_other_control_chars_as_unicode);
    RUN_TEST(test_passes_through_plain_text_unchanged);
    RUN_TEST(test_empty_input_yields_empty_output);
    RUN_TEST(test_truncates_when_buffer_too_small);
    RUN_TEST(test_does_not_split_an_escape_sequence_across_the_truncation_boundary);
    RUN_TEST(test_zero_length_buffer_does_not_write_anything);
    return UNITY_END();
}

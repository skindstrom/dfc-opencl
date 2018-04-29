#include "catch.hpp"

#include <vector>

#include "parser.h"

void test_add_pattern(DFC_PATTERN_INIT *dfc, unsigned char *pattern,
                      int pattern_length, int is_case_insensitive,
                      PID_TYPE pattern_id);

struct Pattern {
  PID_TYPE pattern_id;
  bool is_case_insensitive;
  std::string text;
};

bool isEqualHex(unsigned char character, unsigned char val) {
  return character == val;
}

std::vector<Pattern> patterns;
TEST_CASE("Parser") {
  patterns.clear();
  DFC_PATTERN_INIT *init_struct = DFC_PATTERN_INIT_New();

  SECTION("Single text pattern") {
    parse_file("../benchmark/test-files/single-text.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns.size() == 1);
    REQUIRE(patterns[0].text == "test-pattern");
  }

  SECTION("Multiple text patterns") {
    parse_file("../benchmark/test-files/multiple-text.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns.size() == 3);
    REQUIRE(patterns[0].text == "test-pattern");
    REQUIRE(patterns[1].text == "hurrah");
    REQUIRE(patterns[2].text == "attack");
  }

  SECTION("Patterns id increments") {
    parse_file("../benchmark/test-files/multiple-text.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns[0].pattern_id == 0);
    REQUIRE(patterns[1].pattern_id == 1);
  }

  SECTION("Single hex pattern") {
    parse_file("../benchmark/test-files/single-hex.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns.size() == 1);
    REQUIRE(isEqualHex(patterns[0].text[0], 0x40));
    REQUIRE(isEqualHex(patterns[0].text[1], 0x30));
    REQUIRE(isEqualHex(patterns[0].text[2], 0x77));
  }

  SECTION("Multiple hex patterns") {
    parse_file("../benchmark/test-files/multiple-hex.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns.size() == 3);

    REQUIRE(isEqualHex(patterns[0].text[0], 0x40));
    REQUIRE(isEqualHex(patterns[0].text[1], 0x30));
    REQUIRE(isEqualHex(patterns[0].text[2], 0x77));

    REQUIRE(isEqualHex(patterns[1].text[0], 0x20));
    REQUIRE(isEqualHex(patterns[1].text[1], 0x55));

    REQUIRE(isEqualHex(patterns[2].text[0], 0x11));
    REQUIRE(isEqualHex(patterns[2].text[1], 0xFF));
  }

  SECTION("Mixed") {
    parse_file("../benchmark/test-files/multiple-mixed.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns.size() == 5);

    REQUIRE(isEqualHex(patterns[0].text[0], 0xaa));
    REQUIRE(isEqualHex(patterns[0].text[1], 0xbb));

    REQUIRE(patterns[1].text == "this is a string");

    REQUIRE(isEqualHex(patterns[2].text[0], 0x00));
    REQUIRE(isEqualHex(patterns[2].text[1], 0x22));

    REQUIRE(patterns[3].text == "again!");
    REQUIRE(patterns[4].text == "haha");
  }

  SECTION("Inline hex") {
    parse_file("../benchmark/test-files/inline-hex.txt", init_struct,
               test_add_pattern);

    REQUIRE(patterns.size() == 1);

    std::string text = patterns[0].text;
    REQUIRE(isEqualHex(text[0], 't'));
    REQUIRE(isEqualHex(text[1], 'e'));
    REQUIRE(isEqualHex(text[2], 's'));
    REQUIRE(isEqualHex(text[3], 't'));
    REQUIRE(isEqualHex(text[4], 0xAB));
    REQUIRE(isEqualHex(text[5], 0x77));
    REQUIRE(isEqualHex(text[6], 'h'));
    REQUIRE(isEqualHex(text[7], 'm'));
    REQUIRE(isEqualHex(text[8], 'm'));
  }

  DFC_FreePatternsInit(init_struct);
}

void test_add_pattern(DFC_PATTERN_INIT *, unsigned char *pattern,
                      int pattern_length, int is_case_insensitive,
                      PID_TYPE pattern_id) {
  patterns.emplace_back(
      Pattern{pattern_id, is_case_insensitive,
              std::string((const char *)pattern, pattern_length)});
}
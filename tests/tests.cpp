#include "catch.hpp"
#include "stdio.h"

#include "dfc.h"

void addCaseSensitivePattern(DFC_STRUCTURE* dfc, const std::string& pattern,
                             PID_TYPE patternId) {
  DFC_AddPattern(dfc, (unsigned char*)pattern.data(), pattern.size(), 0,
                 patternId);
}
void addCaseInSensitivePattern(DFC_STRUCTURE* dfc, const std::string& pattern,
                             PID_TYPE patternId) {
  DFC_AddPattern(dfc, (unsigned char*)pattern.data(), pattern.size(), 1,
                 patternId);
}

TEST_CASE("Matches if input and pattern is equal") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, input, pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("No matches if pattern is not equal") {
  PID_TYPE pid = 0;
  auto input = "safe";
  auto pattern = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, pattern, pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Matches multiple patterns") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "at", pid);
  addCaseSensitivePattern(dfc, "ck", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Does not match part of pattern") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "attack123", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Multiple equal patterns counts as single even if different pid") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "attack", pid);
  addCaseSensitivePattern(dfc, "attack", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Does not match if case sensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attack", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Does match if case insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseInSensitivePattern(dfc, "Attack", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Can match both case sensitive and insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "at", pid);
  addCaseInSensitivePattern(dfc, "Tk", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Equal case sensitive and insensitive pattern counts separately") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "at", pid);
  addCaseInSensitivePattern(dfc, "At", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input),
                               [](auto, auto, auto) {});
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
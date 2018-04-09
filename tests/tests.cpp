#include "catch.hpp"
#include "stdio.h"

#include "dfc.h"

void addCaseSensitivePattern(DFC_PATTERN_INIT* dfcPatterns, const std::string& pattern,
                             PID_TYPE patternId) {
  DFC_AddPattern(dfcPatterns, (unsigned char*)pattern.data(), pattern.size(), 0,
                 patternId);
}
void addCaseInSensitivePattern(DFC_PATTERN_INIT* dfcPatterns, const std::string& pattern,
                               PID_TYPE patternId) {
  DFC_AddPattern(dfcPatterns, (unsigned char*)pattern.data(), pattern.size(), 1,
                 patternId);
}

TEST_CASE("Matches if input and pattern is equal") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, input, pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("No matches if pattern is not equal") {
  PID_TYPE pid = 0;
  auto input = "safe";
  auto pattern = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, pattern, pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Matches multiple dfcPatterns") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "at", pid);
  addCaseSensitivePattern(dfcPatterns, "ck", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Does not match part of pattern") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "attack123", pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Multiple equal dfcPatterns counts as single even if different pid") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "attack", pid);
  addCaseSensitivePattern(dfcPatterns, "attack", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Does not match if case sensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attack", pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Does match if case insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseInSensitivePattern(dfcPatterns, "Attack", pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Can match both case sensitive and insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "at", pid);
  addCaseInSensitivePattern(dfcPatterns, "Tk", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Equal case sensitive and insensitive pattern counts separately") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "at", pid);
  addCaseInSensitivePattern(dfcPatterns, "At", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("1B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "A", pid);
  addCaseInSensitivePattern(dfcPatterns, "k", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("2B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "At", pid);
  addCaseInSensitivePattern(dfcPatterns, "ck", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("3B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Att", pid);
  addCaseInSensitivePattern(dfcPatterns, "ack", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
TEST_CASE("4B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Atta", pid);
  addCaseInSensitivePattern(dfcPatterns, "tack", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("5B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attac", pid);
  addCaseInSensitivePattern(dfcPatterns, "ttack", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("6B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attack", pid);
  addCaseInSensitivePattern(dfcPatterns, "attack", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("7B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attacks and Crash";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attacks", pid);
  addCaseInSensitivePattern(dfcPatterns, "d CRASH", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("8B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attackers and Crash";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attacker", pid);
  addCaseInSensitivePattern(dfcPatterns, "nd CRASH", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Long pattern works") {
  PID_TYPE pid = 0;
  auto input = "This is a very long input";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "This is a very long", pid);
  addCaseInSensitivePattern(dfcPatterns, "is a VERY long input", pid + 1);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Many dfcPatterns work") {
  // passwords from some old password leak
  // https://github.com/danielmiessler/SecLists/blob/aad07fff50ca37af2926de4d07ff670bf3416fbc/Passwords/elitehacker.txt
  std::vector<std::string> patterns{
#include "elitehacker.txt"
  };

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  for (PID_TYPE i = 0; i < patterns.size(); ++i) {
    addCaseSensitivePattern(dfcPatterns, patterns[i], i);
  }

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  // 4 random passwords
  auto input = "blue twf skar23 hunter2 1spyder";
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  /*
   * I didn't actually calculate,
   * but rather ran the initial version to get a count.
   * Of course, this assumes the initial version was correct.
   */
  REQUIRE(matchCount == 7);
}


/*
This caused problems on the device in about 30% of cases when
executing the test using the password file.
The password was "1340lu" and failed against "blue"
*/
TEST_CASE("Bounds checking done for short dfcPatterns") {
  PID_TYPE pid = 0;

  auto input = "lu";
  auto pattern = "alu";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, pattern, pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Bounds checking done for long dfcPatterns") {
  PID_TYPE pid = 0;

  auto input = "lu";
  auto pattern = "longlu";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, pattern, pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("A pattern may match whole string") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attack", pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Two dfcPatterns with equal last 4 characters both get matched") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* dfcPatterns = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(dfcPatterns, "Attack", pid);
  addCaseSensitivePattern(dfcPatterns, "ttack", pid);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, dfcPatterns);

  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  
  DFC_FreePatternsInit(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
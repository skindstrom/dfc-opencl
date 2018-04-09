#include "catch.hpp"
#include "stdio.h"

#include "dfc.h"

void addCaseSensitivePattern(DFC_PATTERN_INIT* patternInit,
                             const std::string& pattern, PID_TYPE patternId) {
  DFC_AddPattern(patternInit, (unsigned char*)pattern.data(), pattern.size(), 0,
                 patternId);
}
void addCaseInSensitivePattern(DFC_PATTERN_INIT* patternInit,
                               const std::string& pattern, PID_TYPE patternId) {
  DFC_AddPattern(patternInit, (unsigned char*)pattern.data(), pattern.size(), 1,
                 patternId);
}

TEST_CASE("Matches if input and pattern is equal") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, input, pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreePatterns(dfcPatterns);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("No matches if pattern is not equal") {
  PID_TYPE pid = 0;
  auto input = "safe";
  auto pattern = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, pattern, pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Matches multiple patternInit") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "at", pid);
  addCaseSensitivePattern(patternInit, "ck", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Does not match part of pattern") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "attack123", pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Multiple equal patternInit counts as single even if different pid") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "attack", pid);
  addCaseSensitivePattern(patternInit, "attack", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Does not match if case sensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attack", pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Does match if case insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseInSensitivePattern(patternInit, "Attack", pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Can match both case sensitive and insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "at", pid);
  addCaseInSensitivePattern(patternInit, "Tk", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Equal case sensitive and insensitive pattern counts separately") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "at", pid);
  addCaseInSensitivePattern(patternInit, "At", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("1B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "A", pid);
  addCaseInSensitivePattern(patternInit, "k", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("2B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "At", pid);
  addCaseInSensitivePattern(patternInit, "ck", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("3B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Att", pid);
  addCaseInSensitivePattern(patternInit, "ack", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
TEST_CASE("4B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Atta", pid);
  addCaseInSensitivePattern(patternInit, "tack", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("5B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attac", pid);
  addCaseInSensitivePattern(patternInit, "ttack", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("6B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attack", pid);
  addCaseInSensitivePattern(patternInit, "attack", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("7B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attacks and Crash";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attacks", pid);
  addCaseInSensitivePattern(patternInit, "d CRASH", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("8B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attackers and Crash";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attacker", pid);
  addCaseInSensitivePattern(patternInit, "nd CRASH", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Long pattern works") {
  PID_TYPE pid = 0;
  auto input = "This is a very long input";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "This is a very long", pid);
  addCaseInSensitivePattern(patternInit, "is a VERY long input", pid + 1);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Many patternInit work") {
  // passwords from some old password leak
  // https://github.com/danielmiessler/SecLists/blob/aad07fff50ca37af2926de4d07ff670bf3416fbc/Passwords/elitehacker.txt
  std::vector<std::string> patterns{
#include "elitehacker.txt"
  };

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  for (PID_TYPE i = 0; i < patterns.size(); ++i) {
    addCaseSensitivePattern(patternInit, patterns[i], i);
  }

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  // 4 random passwords
  auto input = "blue twf skar23 hunter2 1spyder";
  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
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
TEST_CASE("Bounds checking done for short patternInit") {
  PID_TYPE pid = 0;

  auto input = "lu";
  auto pattern = "alu";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, pattern, pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Bounds checking done for long patternInit") {
  PID_TYPE pid = 0;

  auto input = "lu";
  auto pattern = "longlu";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, pattern, pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("A pattern may match whole string") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attack", pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Two patternInit with equal last 4 characters both get matched") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
  addCaseSensitivePattern(patternInit, "Attack", pid);
  addCaseSensitivePattern(patternInit, "ttack", pid);

  DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
  DFC_CompilePatterns(patternInit, dfcPatterns);

  DFC_STRUCTURE* dfc = DFC_New();
  DFC_Compile(dfc, patternInit);

  auto matchCount =
      DFC_Search(dfc, dfcPatterns, (unsigned char*)input, strlen(input));

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
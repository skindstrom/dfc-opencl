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
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
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
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
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
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Does not match part of pattern") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "attack123", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
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
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Does not match if case sensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attack", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 0);
}

TEST_CASE("Does match if case insensitive") {
  PID_TYPE pid = 0;
  auto input = "attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseInSensitivePattern(dfc, "Attack", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
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
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
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
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("1B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "A", pid);
  addCaseInSensitivePattern(dfc, "k", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("2B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "At", pid);
  addCaseInSensitivePattern(dfc, "ck", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("3B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Att", pid);
  addCaseInSensitivePattern(dfc, "ack", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
TEST_CASE("4B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Atta", pid);
  addCaseInSensitivePattern(dfc, "tack", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("5B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attac", pid);
  addCaseInSensitivePattern(dfc, "ttack", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("6B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attack", pid);
  addCaseInSensitivePattern(dfc, "attack", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("7B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attacks and Crash";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attacks", pid);
  addCaseInSensitivePattern(dfc, "d CRASH", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("8B pattern works") {
  PID_TYPE pid = 0;
  auto input = "Attackers and Crash";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attacker", pid);
  addCaseInSensitivePattern(dfc, "nd CRASH", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Long pattern works") {
  PID_TYPE pid = 0;
  auto input = "This is a very long input";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "This is a very long", pid);
  addCaseInSensitivePattern(dfc, "is a VERY long input", pid + 1);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}

TEST_CASE("Many patterns work") {
  // passwords from some old password leak
  // https://github.com/danielmiessler/SecLists/blob/aad07fff50ca37af2926de4d07ff670bf3416fbc/Passwords/elitehacker.txt
  std::vector<std::string> patterns{
#include "elitehacker.txt"
  };

  DFC_STRUCTURE* dfc = DFC_New();
  for (PID_TYPE i = 0; i < patterns.size(); ++i) {
    addCaseSensitivePattern(dfc, patterns[i], i);
  }

  DFC_Compile(dfc);

  // 4 random passwords
  auto input = "blue twf skar23 hunter2 1spyder";
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
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
 TEST_CASE("Bounds checking done for short patterns") {
   PID_TYPE pid = 0;

   auto input = "lu";
   auto pattern = "alu";

   DFC_STRUCTURE* dfc = DFC_New();
   addCaseSensitivePattern(dfc, pattern, pid);
   DFC_Compile(dfc);
   auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
   DFC_FreeStructure(dfc);

   REQUIRE(matchCount == 0);
 }

 TEST_CASE("Bounds checking done for long patterns") {
   PID_TYPE pid = 0;

   auto input = "lu";
   auto pattern = "longlu";

   DFC_STRUCTURE* dfc = DFC_New();
   addCaseSensitivePattern(dfc, pattern, pid);
   DFC_Compile(dfc);
   auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
   DFC_FreeStructure(dfc);

   REQUIRE(matchCount == 0);
 }

TEST_CASE("A pattern may match whole string") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attack", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 1);
}

TEST_CASE("Two patterns with equal last 4 characters both get matched") {
  PID_TYPE pid = 0;
  auto input = "Attack";

  DFC_STRUCTURE* dfc = DFC_New();
  addCaseSensitivePattern(dfc, "Attack", pid);
  addCaseSensitivePattern(dfc, "ttack", pid);
  DFC_Compile(dfc);
  auto matchCount = DFC_Search(dfc, (unsigned char*)input, strlen(input));
  DFC_FreeStructure(dfc);

  REQUIRE(matchCount == 2);
}
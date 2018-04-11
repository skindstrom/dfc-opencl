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

TEST_CASE("DFC") {
  // released at the end
  DFC_SetupEnvironment();

  SECTION("Matches if input and pattern is equal") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, input, pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 1);
  }

  SECTION("No matches if pattern is not equal") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "safe");
    auto pattern = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, pattern, pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 0);
  }

  SECTION("Matches multiple patterns") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseSensitivePattern(patternInit, "ck", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("Does not match part of pattern") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "attack123", pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 0);
  }

  SECTION("Multiple equal patternInit counts as single even if different pid") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "attack", pid);
    addCaseSensitivePattern(patternInit, "attack", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 1);
  }

  SECTION("Does not match if case sensitive") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 0);
  }

  SECTION("Does match if case insensitive") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseInSensitivePattern(patternInit, "Attack", pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 1);
  }

  SECTION("Can match both case sensitive and insensitive") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseInSensitivePattern(patternInit, "Tk", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 1);
  }

  SECTION("Equal case sensitive and insensitive pattern counts separately") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseInSensitivePattern(patternInit, "At", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("1B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "A", pid);
    addCaseInSensitivePattern(patternInit, "k", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("2B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "At", pid);
    addCaseInSensitivePattern(patternInit, "ck", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("3B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Att", pid);
    addCaseInSensitivePattern(patternInit, "ack", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }
  SECTION("4B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Atta", pid);
    addCaseInSensitivePattern(patternInit, "tack", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("5B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attac", pid);
    addCaseInSensitivePattern(patternInit, "ttack", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("6B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);
    addCaseInSensitivePattern(patternInit, "attack", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("7B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attacks and Crash");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attacks", pid);
    addCaseInSensitivePattern(patternInit, "d CRASH", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("8B pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attackers and Crash");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attacker", pid);
    addCaseInSensitivePattern(patternInit, "nd CRASH", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("Long pattern works") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "This is a very long input");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "This is a very long", pid);
    addCaseInSensitivePattern(patternInit, "is a VERY long input", pid + 1);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  SECTION("Many patternInit work") {
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

    char* input = DFC_NewInput(100);
    // 4 random passwords
    strcpy(input, "blue twf skar23 hunter2 1spyder");

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

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
  SECTION("Bounds checking done for short patternInit") {
    PID_TYPE pid = 0;

    char* input = DFC_NewInput(100);
    strcpy(input, "lu");
    auto pattern = "alu";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, pattern, pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 0);
  }

  SECTION("Bounds checking done for long patternInit") {
    PID_TYPE pid = 0;

    char* input = DFC_NewInput(100);
    strcpy(input, "lu");
    auto pattern = "longlu";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, pattern, pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 0);
  }

  SECTION("A pattern may match whole string") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 1);
  }

  SECTION("Two patternInit with equal last 4 characters both get matched") {
    PID_TYPE pid = 0;
    char* input = DFC_NewInput(100);
    strcpy(input, "Attack");

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);
    addCaseSensitivePattern(patternInit, "ttack", pid);

    DFC_PATTERNS* dfcPatterns = DFC_PATTERNS_New(patternInit->numPatterns);
    DFC_CompilePatterns(patternInit, dfcPatterns);

    DFC_STRUCTURE* dfc = DFC_New();
    DFC_Compile(dfc, patternInit);

    auto matchCount = DFC_Search();

    DFC_FreePatternsInit(patternInit);
    DFC_FreePatterns();
    DFC_FreeStructure();
    DFC_FreeInput();

    REQUIRE(matchCount == 2);
  }

  DFC_ReleaseEnvironment();
}
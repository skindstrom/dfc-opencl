#include <stdio.h>
#include <chrono>
#include <thread>

#include "catch.hpp"

#include "dfc.h"
#include "timer.h"

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

struct Pattern {
  std::vector<PID_TYPE> ids;
  std::string pattern;
};

std::string input;
bool didRead = 0;
int readInput(int maxLength, int maxPatternLength, char* inputBuffer) {
  if (didRead) {
    return 0;
  }

  REQUIRE(maxPatternLength == MAX_PATTERN_LENGTH);
  REQUIRE(maxLength >= input.size());
  memcpy(inputBuffer, input.data(), input.size());
  didRead = true;

  return input.size();
}

std::vector<Pattern> matches;
void onMatch(DFC_FIXED_PATTERN* pattern) {
  std::vector<PID_TYPE> ids;
  for (int i = 0; i < pattern->external_id_count; ++i) {
    ids.emplace_back(pattern->external_ids[i]);
  }
  matches.emplace_back(Pattern{
      std::move(ids), std::string((const char*)pattern->original_pattern,
                                  pattern->pattern_length)});
}

TEST_CASE("DFC") {
  // released at the end
  DFC_SetupEnvironment();

  input.clear();
  didRead = false;
  matches.clear();

  SECTION("Matches if input and pattern is equal") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, input, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].pattern == "attack");
  }

  SECTION("No matches if pattern is not equal") {
    PID_TYPE pid = 0;
    input = "safe";
    auto pattern = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, pattern, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 0);
  }

  SECTION("Matches multiple patterns") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseSensitivePattern(patternInit, "ck", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("Does not match part of pattern") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "attack123", pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 0);
  }

  SECTION("Multiple equal patterns counts as single even if different pid") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "attack", pid);
    addCaseSensitivePattern(patternInit, "attack", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
  }

  SECTION("Does not match if case sensitive") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 0);
  }

  SECTION("Does match if case insensitive") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseInSensitivePattern(patternInit, "Attack", pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
  }

  SECTION("Can match both case sensitive and insensitive") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseInSensitivePattern(patternInit, "Tk", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
  }

  SECTION("Equal case sensitive and insensitive pattern counts separately") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseInSensitivePattern(patternInit, "At", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("1B pattern works") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "A", pid);
    addCaseInSensitivePattern(patternInit, "k", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("2B pattern works") {
    PID_TYPE pid = 0;

    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "At", pid);
    addCaseInSensitivePattern(patternInit, "ck", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("3B pattern works") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Att", pid);
    addCaseInSensitivePattern(patternInit, "ack", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }
  SECTION("4B pattern works") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Atta", pid);
    addCaseInSensitivePattern(patternInit, "tack", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("5B pattern works") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attac", pid);
    addCaseInSensitivePattern(patternInit, "ttack", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("6B pattern works") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);
    addCaseInSensitivePattern(patternInit, "attack", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("7B pattern works") {
    PID_TYPE pid = 0;
    input = "Attacks and Crash";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attacks", pid);
    addCaseInSensitivePattern(patternInit, "d CRASH", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("8B pattern works") {
    PID_TYPE pid = 0;
    input = "Attackers and Crash";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attacker", pid);
    addCaseInSensitivePattern(patternInit, "nd CRASH", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("Long pattern works") {
    PID_TYPE pid = 0;
    input = "This is a very long input";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "This is a very long", pid);
    addCaseInSensitivePattern(patternInit, "is a VERY long input", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("Many patterns work") {
    // passwords from some old password leak
    // https://github.com/danielmiessler/SecLists/blob/aad07fff50ca37af2926de4d07ff670bf3416fbc/Passwords/elitehacker.txt
    std::vector<std::string> patterns{
#include "elitehacker.txt"
    };

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    for (size_t i = 0; i < patterns.size(); ++i) {
      addCaseSensitivePattern(patternInit, patterns[i], i);
    }

    DFC_Compile(patternInit);

    // 4 random passwords
    input = "blue twf skar23 hunter2 1spyder";

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

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
  SECTION("Bounds checking done for short patterns") {
    PID_TYPE pid = 0;
    input = "lu";
    auto pattern = "alu";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, pattern, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 0);
  }

  SECTION("Bounds checking done for long patterns") {
    PID_TYPE pid = 0;
    input = "lu";
    auto pattern = "longlu";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, pattern, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 0);
  }

  SECTION("A pattern may match whole string") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
  }

  SECTION("Two patterns with equal last 4 characters both get matched") {
    PID_TYPE pid = 0;
    input = "Attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "Attack", pid);
    addCaseSensitivePattern(patternInit, "ttack", pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
  }

  SECTION("Calls onMatch on match") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "at", pid);
    addCaseSensitivePattern(patternInit, "ck", pid + 1);

    DFC_Compile(patternInit);

    DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matches.size() == 2);

    REQUIRE(matches[0].pattern == "at");
    REQUIRE(matches[0].ids[0] == 0);

    REQUIRE(matches[1].pattern == "ck");
    REQUIRE(matches[1].ids[0] == 1);
  }

  SECTION("Multiple equal patterns calls onMatch once") {
    PID_TYPE pid = 0;
    input = "attack";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "attack", pid);
    addCaseSensitivePattern(patternInit, "attack", pid + 1);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].pattern == "attack");
    REQUIRE(matches[0].ids[0] == 0);
    REQUIRE(matches[0].ids[1] == 1);
  }

  SECTION("Matches binary pattern without null") {
    PID_TYPE pid = 0;
    char binary_input[3] = {0x05, 0x10, 0x20};
    input = std::string(binary_input, 3);

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, input, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].pattern[0] == binary_input[0]);
    REQUIRE(matches[0].pattern[1] == binary_input[1]);
    REQUIRE(matches[0].pattern[2] == binary_input[2]);
  }

  SECTION("Matches input with null character") {
    PID_TYPE pid = 0;
    
    input = "start0end";
    input[5] = 0x00;

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, "start", pid);
    addCaseSensitivePattern(patternInit, "end", pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 2);
    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0].pattern == "start");
    REQUIRE(matches[1].pattern == "end");
  }

  SECTION("Matches binary pattern with null") {
    PID_TYPE pid = 0;
    char binary_input[3] = {0x05, 0x00, 0x20};
    input = std::string(binary_input, 3);

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, input, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].pattern[0] == binary_input[0]);
    REQUIRE(matches[0].pattern[1] == binary_input[1]);
    REQUIRE(matches[0].pattern[2] == binary_input[2]);
  }

  SECTION("Matches binary pattern with only null") {
    PID_TYPE pid = 0;
    char binary_pattern[3] = {0x00, 0x00, 0x00};
    std::string binary_pattern_string(binary_pattern, 3);
    input = "start" + binary_pattern_string + "end";

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    addCaseSensitivePattern(patternInit, binary_pattern_string, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].pattern[0] == binary_pattern[0]);
    REQUIRE(matches[0].pattern[1] == binary_pattern[1]);
    REQUIRE(matches[0].pattern[2] == binary_pattern[2]);
  }

  SECTION("Null pattern with same prefix does not override") {
    PID_TYPE pid = 0;
    char binary_pattern[3] = {0x00, 0x00, 0x00};
    std::string binary_pattern_string_long(binary_pattern, 3);
    std::string binary_pattern_string_short(binary_pattern, 2);

    input = binary_pattern_string_short;

    DFC_PATTERN_INIT* patternInit = DFC_PATTERN_INIT_New();
    // long first!
    addCaseSensitivePattern(patternInit, binary_pattern_string_long, pid);
    addCaseSensitivePattern(patternInit, binary_pattern_string_short, pid);

    DFC_Compile(patternInit);

    auto matchCount = DFC_Search(readInput, onMatch);

    DFC_FreePatternsInit(patternInit);
    DFC_FreeStructure();

    REQUIRE(matchCount == 1);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].pattern[0] == 0x00);
    REQUIRE(matches[0].pattern[1] == 0x00);
  }

  DFC_ReleaseEnvironment();
}

TEST_CASE("Timer") {
  const int timer = 0;
  resetTimer(timer);

  SECTION("Tracks duration") {
    REQUIRE(readTimerMs(timer) == 0.0);

    startTimer(timer);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    stopTimer(timer);

    REQUIRE(readTimerMs(timer) == Approx(10.0).epsilon(1.0));
  }
  SECTION("May be reset") {
    startTimer(timer);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    stopTimer(timer);

    REQUIRE(readTimerMs(timer) != Approx(0));

    resetTimer(timer);
    REQUIRE(readTimerMs(timer) == 0.0);
  }
}
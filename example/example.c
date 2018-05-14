/* Microbenchmark for DFC */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dfc.h"

#define INPUT \
  ("This input includes an attack pattern. It might CRASH your machine.")

int readInput(int maxRead, int maxPatternLength, char *input);
void printResult(DFC_FIXED_PATTERN *pattern);

int main(void) {
  DFC_SetupEnvironment();

  char *pat1 = "attack";
  char *pat2 = "crash";
  char *pat3 = "Piolink";
  char *pat4 = "ATTACK";

  printf("\n* Text & Patterns Info\n");
  printf(" - (Text) %s\n", INPUT);
  printf(" - (Pattern) ID: 0, pat: %s, case-sensitive\n", pat1);
  printf(" - (Pattern) ID: 1, pat: %s, case-insensitive\n", pat2);
  printf(" - (Pattern) ID: 2, pat: %s, case-insensitive\n", pat3);
  printf(" - (Pattern) ID: 3, pat: %s, case-insensitive\n", pat4);
  printf("\n");

  DFC_PATTERN_INIT *patternInit = DFC_PATTERN_INIT_New();

  DFC_AddPattern(patternInit, (unsigned char *)pat1, strlen(pat1),
                 0 /*case-sensitive pattern*/, 0 /*Pattern ID*/);
  DFC_AddPattern(patternInit, (unsigned char *)pat2, strlen(pat2),
                 1 /*case-insensitive pattern*/, 1 /*Pattern ID*/);
  DFC_AddPattern(patternInit, (unsigned char *)pat3, strlen(pat3),
                 1 /*case-insensitive pattern*/, 2 /*Pattern ID*/);
  DFC_AddPattern(patternInit, (unsigned char *)pat4, strlen(pat4),
                 1 /*case-insensitive pattern*/, 3 /*Pattern ID*/);

  DFC_Compile(patternInit);

  printf("* Result:\n");
  int res = DFC_Search(readInput, printResult);
  printf("\n* Total match count: %d\n", res);

  DFC_FreePatternsInit(patternInit);
  DFC_FreeStructure();

  DFC_ReleaseEnvironment();

  return 0;
}

bool didRead = 0;
int readInput(int maxLength, int maxPatternLength, char *input) {
  if (didRead) {
    return 0;
  }

  assert(maxPatternLength == MAX_PATTERN_LENGTH);
  assert(maxLength >= (int)strlen(INPUT));
  (void)(maxLength);  // to make release build happy
  strcpy(input, INPUT);
  didRead = true;

  return strlen(INPUT);
}

void printResult(DFC_FIXED_PATTERN *pattern) {
  printf("Matched %.*s ", pattern->pattern_length, pattern->original_pattern);
  for (int i = 0; i < pattern->external_id_count; ++i) {
    printf(" %d,", pattern->external_ids[i]);
  }
  printf("\n");
}
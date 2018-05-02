/* Microbenchmark for DFC */

#include <stdio.h>
#include <string.h>

#include "dfc.h"

void printResult(DFC_FIXED_PATTERN *pattern);

int main(void) {
  DFC_SetupEnvironment();

  char *buf = DFC_NewInput(200);
  strcpy(buf,
         "This input includes an attack pattern. It might CRASH your machine.");

  char *pat1 = "attack";
  char *pat2 = "crash";
  char *pat3 = "Piolink";
  char *pat4 = "ATTACK";

  printf("\n* Text & Patterns Info\n");
  printf(" - (Text) %s\n", buf);
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
  int res = DFC_Search(printResult);
  printf("\n* Total match count: %d\n", res);

  DFC_FreePatternsInit(patternInit);
  DFC_FreeInput();
  DFC_FreeStructure();

  DFC_ReleaseEnvironment();

  return 0;
}

void printResult(DFC_FIXED_PATTERN *pattern) {
  printf("Matched %.*s ", pattern->pattern_length, pattern->original_pattern);
  for (int i = 0; i < pattern->external_id_count; ++i) {
    printf(" %d,", pattern->external_ids[i]);
  }
  printf("\n");
}
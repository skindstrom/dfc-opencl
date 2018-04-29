#include <stdio.h>

#include "parser.h"
#include "timer.h"

void printResult(DFC_FIXED_PATTERN *pattern);

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: benchmark PATTERN-FILE DATA-FILE\n");
    return 1;
  }

  DFC_SetupEnvironment();

  char *pattern_file = argv[1];
  char *data_file = argv[2];

  DFC_PATTERN_INIT *init_struct = DFC_PATTERN_INIT_New();

  startTimer(TIMER_ADD_PATTERNS);
  parse_pattern_file(pattern_file, init_struct, DFC_AddPattern);
  stopTimer(TIMER_ADD_PATTERNS);

  startTimer(TIMER_READ_DATA);
  char *input = read_data_file(data_file, DFC_NewInput);
  stopTimer(TIMER_READ_DATA);

  DFC_STRUCTURE *dfc = DFC_New(init_struct->numPatterns);

  startTimer(TIMER_COMPILE_DFC);
  DFC_Compile(dfc, init_struct);
  stopTimer(TIMER_COMPILE_DFC);

  int matchCount = DFC_Search(printResult);
  printf("\n* Total match count: %d\n", matchCount);

  DFC_FreeStructure();
  DFC_FreeInput(input);
  DFC_FreePatternsInit(init_struct);

  DFC_ReleaseEnvironment();
}

void printResult(DFC_FIXED_PATTERN *pattern) {
  printf("Matched %.*s ", pattern->pattern_length, pattern->original_pattern);
  for (int i = 0; i < pattern->external_id_count; ++i) {
    printf(" %d,", pattern->external_ids[i]);
  }
  printf("\n");
}
#include <stdio.h>

#include "parser.h"
#include "timer.h"

char *readDataFile(char *data_file);
DFC_PATTERN_INIT *addPatterns(char *pattern_file);
DFC_STRUCTURE *compilePatterns(DFC_PATTERN_INIT *init_struct);

int search();
void printResult(DFC_FIXED_PATTERN *pattern);

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: benchmark PATTERN-FILE DATA-FILE\n");
    return 1;
  }

  DFC_SetupEnvironment();

  char *input = readDataFile(argv[2]);

  DFC_PATTERN_INIT *init_struct = addPatterns(argv[1]);
  DFC_STRUCTURE *dfc = compilePatterns(init_struct);

  int matchCount = search(dfc);
  printf("\n* Total match count: %d\n", matchCount);

  DFC_FreeStructure();
  DFC_FreeInput(input);
  DFC_FreePatternsInit(init_struct);

  DFC_ReleaseEnvironment();
}

char *readDataFile(char *data_file) {
  startTimer(TIMER_READ_DATA);
  char *input = read_data_file(data_file, DFC_NewInput);
  stopTimer(TIMER_READ_DATA);

  return input;
}

DFC_PATTERN_INIT *addPatterns(char *pattern_file) {
  DFC_PATTERN_INIT *init_struct = DFC_PATTERN_INIT_New();

  startTimer(TIMER_ADD_PATTERNS);
  parse_pattern_file(pattern_file, init_struct, DFC_AddPattern);
  stopTimer(TIMER_ADD_PATTERNS);

  return init_struct;
}

DFC_STRUCTURE *compilePatterns(DFC_PATTERN_INIT *init_struct) {
  DFC_STRUCTURE *dfc = DFC_New(init_struct->numPatterns);

  startTimer(TIMER_COMPILE_DFC);
  DFC_Compile(dfc, init_struct);
  stopTimer(TIMER_COMPILE_DFC);

  return dfc;
}

int search() {
  startTimer(TIMER_SEARCH);
  int matchCount = DFC_Search(printResult);
  stopTimer(TIMER_SEARCH);

  return matchCount;
}

void printResult(DFC_FIXED_PATTERN *pattern) {
  printf("Matched %.*s ", pattern->pattern_length, pattern->original_pattern);
  for (int i = 0; i < pattern->external_id_count; ++i) {
    printf(" %d,", pattern->external_ids[i]);
  }
  printf("\n");
}

#include <stdio.h>

#include "parser.h"
#include "timer.h"

int readDataFile(int to_read_count, char *buffer);
DFC_PATTERN_INIT *addPatterns(char *pattern_file);
DFC_STRUCTURE *compilePatterns(DFC_PATTERN_INIT *init_struct);

int benchmarkSearch();
void printResult(DFC_FIXED_PATTERN *pattern);

FILE *data_file;

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: benchmark PATTERN-FILE DATA-FILE\n");
    return 1;
  }


  data_file = fopen(argv[2], "rb");
  if (data_file == NULL) {
    fprintf(stderr, "Data file not found\n");
    exit(1);
  }

  DFC_SetupEnvironment();
  DFC_PATTERN_INIT *init_struct = addPatterns(argv[1]);
  compilePatterns(init_struct);

  int matchCount = benchmarkSearch();
  printf("\n* Total match count: %d\n", matchCount);

  DFC_FreeStructure();
  DFC_FreePatternsInit(init_struct);

  DFC_ReleaseEnvironment();

  fclose(data_file);
}

int readDataFile(int to_read_count, char *buffer) {
  startTimer(TIMER_READ_DATA);
  int actually_read_count = fread(buffer, sizeof(char), to_read_count, data_file);
  stopTimer(TIMER_READ_DATA);

  return actually_read_count;
}

DFC_PATTERN_INIT *addPatterns(char *pattern_file) {
  DFC_PATTERN_INIT *init_struct = DFC_PATTERN_INIT_New();

  startTimer(TIMER_ADD_PATTERNS);
  parse_pattern_file(pattern_file, init_struct, DFC_AddPattern);
  stopTimer(TIMER_ADD_PATTERNS);

  return init_struct;
}

DFC_STRUCTURE *compilePatterns(DFC_PATTERN_INIT *init_struct) {
  startTimer(TIMER_COMPILE_DFC);
  DFC_STRUCTURE *dfc = DFC_Compile(init_struct);
  stopTimer(TIMER_COMPILE_DFC);

  return dfc;
}

int benchmarkSearch() {
  startTimer(TIMER_SEARCH);
  int matchCount = DFC_Search(readDataFile, printResult);
  stopTimer(TIMER_SEARCH);

  return matchCount;
}

void printResult(DFC_FIXED_PATTERN *pattern) {
  (void)(pattern);
  // printf("Matched %.*s ", pattern->pattern_length,
  // pattern->original_pattern); for (int i = 0; i < pattern->external_id_count;
  // ++i) {
  //  printf(" %d,", pattern->external_ids[i]);
  //}
  // printf("\n");
}

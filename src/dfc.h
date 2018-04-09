/*********************************/
/* Author  - Byungkwon Choi      */
/* Contact - cbkbrad@kaist.ac.kr */
/*********************************/
#ifndef DFC_H
#define DFC_H

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CompactTableSmallEntry_ {
  uint8_t pattern;
  uint8_t pidCount;
  PID_TYPE pids[MAX_PID_PER_ENTRY];
} CompactTableSmallEntry;

typedef struct CompactTableSmall_ {
  CompactTableSmallEntry entries[MAX_ENTRIES_PER_BUCKET];
} CompactTableSmall;

typedef struct CompactTableLargeEntry_ {
  uint32_t pattern;
  uint8_t pidCount;
  PID_TYPE pids[MAX_PID_PER_ENTRY];
} CompactTableLargeEntry;

typedef struct CompactTableLarge_ {
  CompactTableLargeEntry entries[MAX_ENTRIES_PER_BUCKET];
} CompactTableLarge;

typedef struct _dfc_pattern {
  struct _dfc_pattern *next;

  unsigned char *patrn;      // upper case pattern
  unsigned char *casepatrn;  // original pattern
  int n;                     // Patternlength
  int is_case_insensitive;

  uint32_t sids_size;
  PID_TYPE *sids;  // external id (unique)
  PID_TYPE iid;    // internal id (used in DFC library only)

} DFC_PATTERN;

typedef struct _dfc_fixed_pattern {
  int pattern_length;
  int is_case_insensitive;

  uint8_t upper_case_pattern[MAX_PATTERN_LENGTH];
  uint8_t original_pattern[MAX_PATTERN_LENGTH];

  int external_id_count;
  PID_TYPE external_ids[MAX_EQUAL_PATTERNS];
} DFC_FIXED_PATTERN;

typedef struct {
  int numPatterns;
  DFC_PATTERN **init_hash;  // To cull duplicate patterns
  DFC_PATTERN *dfcPatterns;
} DFC_PATTERN_INIT;

typedef struct {
  int numPatterns;
  DFC_FIXED_PATTERN *dfcMatchList;
} DFC_PATTERNS;

typedef struct {
  uint8_t directFilterSmall[DF_SIZE_REAL];
  CompactTableSmallEntry compactTableSmall[COMPACT_TABLE_SIZE_SMALL];

  uint8_t directFilterLarge[DF_SIZE_REAL];
  // Indexed by hashing more bytes of the input
  uint8_t directFilterLargeHash[DF_SIZE_REAL];
  CompactTableLarge compactTableLarge[COMPACT_TABLE_SIZE_LARGE];
} DFC_STRUCTURE;

DFC_STRUCTURE *DFC_New();
DFC_PATTERN_INIT *DFC_PATTERN_INIT_New();
DFC_PATTERNS *DFC_PATTERNS_New(int numPatterns);
void DFC_AddPattern(DFC_PATTERN_INIT *dfc, unsigned char *pat, int n,
                    int is_case_insensitive, PID_TYPE sid);
void DFC_CompilePatterns(DFC_PATTERN_INIT *init, DFC_PATTERNS *patterns);
int DFC_Compile(DFC_STRUCTURE *dfc, DFC_PATTERN_INIT *patterns);

int DFC_Search(DFC_STRUCTURE *dfc, DFC_PATTERNS *patterns, uint8_t *input,
               int inputLength);
void DFC_PrintInfo(DFC_STRUCTURE *dfc);
void DFC_FreePatternsInit(DFC_PATTERN_INIT *patterns);
void DFC_FreePatterns(DFC_PATTERNS *patterns);
void DFC_FreeStructure(DFC_STRUCTURE *dfc);

#ifdef __cplusplus
}
#endif

#endif
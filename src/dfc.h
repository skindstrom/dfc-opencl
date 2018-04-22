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
#include "shared.h"

#ifdef __cplusplus
extern "C" {
#endif

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
  DFC_PATTERNS *patterns;

  uint8_t *directFilterSmall;
  uint8_t *directFilterLarge;
  // Indexed by hashing more bytes of the input
  uint8_t *directFilterLargeHash;

  CompactTableSmallEntry *compactTableSmall;
  CompactTableLarge *compactTableLarge;
} DFC_STRUCTURE;

void DFC_AddPattern(DFC_PATTERN_INIT *dfc, unsigned char *pat, int n,
                    int is_case_insensitive, PID_TYPE sid);
void DFC_Compile(DFC_STRUCTURE *dfc, DFC_PATTERN_INIT *patterns);

/**
 * Accessing the input, DFC_STRUCTURE or DFC_PATTERNS is not safe after a call
 * to DFC_SEARCH The reason is that when MAP_MEMORY is used, the host memory has
 * to be unmapped to ensure that any write made on the host is also seen by the
 * device. Unmapping will cause the previous pointers to be invalidated
 */

int DFC_Search();
void DFC_PrintInfo(DFC_STRUCTURE *dfc);

char *DFC_NewInput(int size);
DFC_STRUCTURE *DFC_New(int numPatterns);
DFC_PATTERN_INIT *DFC_PATTERN_INIT_New();

void DFC_FreeInput();
void DFC_FreePatternsInit(DFC_PATTERN_INIT *patterns);
void DFC_FreeStructure();

void DFC_SetupEnvironment();
void DFC_ReleaseEnvironment();

#ifdef __cplusplus
}
#endif

#endif
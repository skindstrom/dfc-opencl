#include <assert.h>

#include "dfc.h"
#include "memory.h"
#include "search.h"
#include "shared-functions.h"
#include "timer.h"
#include "utility.h"

static unsigned char xlatcase[256];

typedef struct DynamicCtSmallEntry_ {
  uint8_t pattern;
  int32_t pidCount;
  PID_TYPE *pids;
} DynamicCtSmallEntry;

typedef struct DynamicCtLargeEntry_ {
  uint32_t pattern;
  int32_t pidCount;
  PID_TYPE *pids;
} DynamicCtLargeEntry;

typedef struct DynamicCtLarge_ {
  int entryCount;
  DynamicCtLargeEntry *entries;
} DynamicCtLarge;

static void *DFC_REALLOC(void *p, uint16_t n, dfcDataType type);
static void *DFC_MALLOC(int n);
static inline DFC_PATTERN *DFC_InitHashLookup(DFC_PATTERN_INIT *ctx,
                                              uint8_t *pat, uint16_t patlen);
static inline int DFC_InitHashAdd(DFC_PATTERN_INIT *ctx, DFC_PATTERN *p);

static void setupPatternListFromHash(DFC_PATTERN_INIT *init);
static void setupMatchList(DFC_PATTERN_INIT *init, DFC_PATTERNS *patterns);

static void setupDirectFilters(DFC_STRUCTURE *dfc, DFC_PATTERN_INIT *patterns);
static void addPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern);
static void addPatternToLargeDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern);
static void addPatternToLargeDirectFilterHash(DFC_STRUCTURE *dfc,
                                              DFC_PATTERN *pattern);
static void createPermutations(uint8_t *pattern, int patternLength,
                               int permutationCount, uint8_t *permutations);
static void setupCompactTables(DFC_PATTERN_INIT *patterns,
                               DynamicCtSmallEntry **ctSmall,
                               DynamicCtLarge **ctLarge);

static uint8_t toggleCharacterCase(uint8_t);

void DFC_SetupEnvironment() { setupExecutionEnvironment(); }
void DFC_ReleaseEnvironment() { releaseExecutionEnvironment(); }

DFC_PATTERN_INIT *DFC_PATTERN_INIT_New(void) {
  DFC_PATTERN_INIT *p;

  init_xlatcase(xlatcase);

  p = (DFC_PATTERN_INIT *)DFC_MALLOC(sizeof(DFC_PATTERN_INIT));
  MEMASSERT_DFC(p, "DFC_PATTERN_INIT_New");

  if (p) {
    p->init_hash =
        (DFC_PATTERN **)malloc(sizeof(DFC_PATTERN *) * INIT_HASH_SIZE);
    if (p->init_hash == NULL) {
      exit(1);
    }
    memset(p->init_hash, 0, sizeof(DFC_PATTERN *) * INIT_HASH_SIZE);
  }

  return p;
}

static void DFC_FreePattern(DFC_PATTERN *p) {
  free(p->patrn);
  free(p->casepatrn);
  free(p->sids);
}

void DFC_FreePatternsInit(DFC_PATTERN_INIT *patterns) {
  DFC_PATTERN *plist;
  DFC_PATTERN *p_next;

  for (plist = patterns->dfcPatterns; plist != NULL;) {
    DFC_FreePattern(plist);
    p_next = plist->next;
    free(plist);
    plist = p_next;
  }

  free(patterns->init_hash);
  free(patterns);
}

void DFC_FreeStructure() { freeDfcStructure(); }

void DFC_FreeInput() { freeDfcInput(); }

void DFC_AddPattern(DFC_PATTERN_INIT *dfc, unsigned char *pat, int n,
                    int is_case_insensitive, PID_TYPE sid) {
  DFC_PATTERN *plist = DFC_InitHashLookup(dfc, pat, n);

  if (plist == NULL) {
    plist = (DFC_PATTERN *)DFC_MALLOC(sizeof(DFC_PATTERN));
    MEMASSERT_DFC(plist, "DFC_AddPattern");
    memset(plist, 0, sizeof(DFC_PATTERN));

    plist->patrn = (unsigned char *)DFC_MALLOC(n);
    MEMASSERT_DFC(plist->patrn, "DFC_AddPattern");

    ConvertCaseEx(plist->patrn, pat, n, xlatcase);

    plist->casepatrn = (unsigned char *)DFC_MALLOC(n);
    MEMASSERT_DFC(plist->casepatrn, "DFC_AddPattern");

    memcpy(plist->casepatrn, pat, n);

    plist->n = n;
    plist->is_case_insensitive = is_case_insensitive;
    plist->iid = dfc->numPatterns;  // internal id
    plist->next = NULL;

    DFC_InitHashAdd(dfc, plist);

    /* sid update */
    plist->sids_size = 1;
    plist->sids = (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE));
    MEMASSERT_DFC(plist->sids, "DFC_AddPattern");
    plist->sids[0] = sid;

    /* Add this pattern to the list */
    dfc->numPatterns++;
  } else {
    int found = 0;
    uint32_t x = 0;

    for (x = 0; x < plist->sids_size; x++) {
      if (plist->sids[x] == sid) {
        found = 1;
        break;
      }
    }

    if (!found) {
      PID_TYPE *sids = (PID_TYPE *)DFC_REALLOC(
          plist->sids, plist->sids_size + 1, DFC_PID_TYPE);
      plist->sids = sids;
      plist->sids[plist->sids_size] = sid;
      plist->sids_size++;
    }
  }
}

int countNumberOfPidsInSmallCt(DynamicCtSmallEntry *ct) {
  int count = 0;
  for (int i = 0; i < COMPACT_TABLE_SIZE_SMALL; ++i) {
    count += ct[i].pidCount;
  }

  return count;
}

int countNumberOfEntriesInLargeCt(DynamicCtLarge *ct) {
  int count = 0;
  for (int i = 0; i < COMPACT_TABLE_SIZE_LARGE; ++i) {
    count += ct[i].entryCount;
  }
  return count;
}

int countNumberOfPidsInLargeCt(DynamicCtLarge *ct) {
  int count = 0;
  for (int i = 0; i < COMPACT_TABLE_SIZE_LARGE; ++i) {
    for (int j = 0; j < ct[i].entryCount; ++j) {
      count += ct[i].entries[j].pidCount;
    }
  }

  return count;
}

static void flattenSmallCt(DynamicCtSmallEntry *dynamicCt,
                           CompactTableSmallEntry *staticCt, PID_TYPE *pids) {
  int offset = 0;
  for (int i = 0; i < COMPACT_TABLE_SIZE_SMALL; ++i) {
    DynamicCtSmallEntry *dynamicEntry = dynamicCt + i;
    CompactTableSmallEntry *staticEntry = staticCt + i;

    staticEntry->pattern = dynamicEntry->pattern;
    staticEntry->pidCount = dynamicEntry->pidCount;
    staticEntry->offset = offset;

    for (int j = 0; j < dynamicEntry->pidCount; ++j) {
      *(pids + offset) = dynamicEntry->pids[j];
      ++offset;
    }
  }
}

static void freeDynamicSmallCt(DynamicCtSmallEntry *ct) {
  for (int i = 0; i < COMPACT_TABLE_SIZE_SMALL; ++i) {
    DynamicCtSmallEntry *entry = ct + i;
    free(entry->pids);
  }
  free(ct);
}

static void flattenLargeCt(DynamicCtLarge *dynamicCt,
                           CompactTableLargeBucket *staticCt,
                           CompactTableLargeEntry *entries, PID_TYPE *pids) {
  int entryOffset = 0;
  int pidOffset = 0;
  for (int i = 0; i < COMPACT_TABLE_SIZE_LARGE; ++i) {
    DynamicCtLarge *dynamicBucket = dynamicCt + i;
    CompactTableLargeBucket *staticBucket = staticCt + i;

    staticBucket->entryOffset = entryOffset;
    staticBucket->entryCount = dynamicBucket->entryCount;
    for (int j = 0; j < dynamicBucket->entryCount; ++j) {
      DynamicCtLargeEntry *dynamicEntry = dynamicBucket->entries + j;
      CompactTableLargeEntry *staticEntry =
          entries + staticBucket->entryOffset + j;

      staticEntry->pattern = dynamicEntry->pattern;
      staticEntry->pidCount = dynamicEntry->pidCount;
      staticEntry->pidOffset = pidOffset;

      for (int k = 0; k < dynamicEntry->pidCount; ++k) {
        pids[pidOffset] = dynamicEntry->pids[k];
        ++pidOffset;
      }

      ++entryOffset;
    }
  }
}

static void freeDynamicLargeCt(DynamicCtLarge *ct) {
  for (int i = 0; i < COMPACT_TABLE_SIZE_LARGE; ++i) {
    DynamicCtLarge *bucket = ct + i;
    for (int j = 0; j < bucket->entryCount; ++j) {
      DynamicCtLargeEntry *entry = bucket->entries + j;
      free(entry->pids);
    }

    free(bucket->entries);
  }

  free(ct);
}

DFC_STRUCTURE *DFC_Compile(DFC_PATTERN_INIT *patterns) {
  startTimer(TIMER_COMPILE_DFC);

  DynamicCtSmallEntry *ctSmall;
  DynamicCtLarge *ctLarge;

  setupPatternListFromHash(patterns);

  setupCompactTables(patterns, &ctSmall, &ctLarge);

  {
    int ctSmallPidCount = countNumberOfPidsInSmallCt(ctSmall);
    int ctLargeEntryCount = countNumberOfEntriesInLargeCt(ctLarge);
    int ctLargePidCount = countNumberOfPidsInLargeCt(ctLarge);

    DfcMemoryRequirements requirements = {
        .patternCount = patterns->numPatterns,
        .ctSmallPidCount = ctSmallPidCount,
        .ctLargeEntryCount = ctLargeEntryCount,
        .ctLargePidCount = ctLargePidCount};
    allocateDfcStructure(requirements);
  }

  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;

  setupDirectFilters(dfc, patterns);
  setupMatchList(patterns, dfc->patterns);

  flattenSmallCt(ctSmall, dfc->ctSmallEntries, dfc->ctSmallPids);
  flattenLargeCt(ctLarge, dfc->ctLargeBuckets, dfc->ctLargeEntries,
                 dfc->ctLargePids);

  freeDynamicSmallCt(ctSmall);
  freeDynamicLargeCt(ctLarge);

  stopTimer(TIMER_COMPILE_DFC);

  return dfc;
}

int DFC_Search(ReadFunction read, MatchFunction onMatch) {
  return search(read, onMatch);
}

static void *DFC_REALLOC(void *p, uint16_t n, dfcDataType type) {
  switch (type) {
    case DFC_PID_TYPE:
      p = realloc((PID_TYPE *)p, sizeof(PID_TYPE) * n);
      return p;
    default:
      fprintf(stderr, "ERROR! Data Type is not correct!\n");
      break;
  }
  return NULL;
}

static void *DFC_MALLOC(int n) {
  void *p = calloc(1, n);  // initialize it to 0
  return p;
}

static inline uint32_t DFC_InitHashRaw(uint8_t *pat, uint16_t patlen) {
  uint32_t hash = patlen * pat[0];
  if (patlen > 1) hash += pat[1];

  return (hash % INIT_HASH_SIZE);
}

static inline DFC_PATTERN *DFC_InitHashLookup(DFC_PATTERN_INIT *ctx,
                                              uint8_t *pat, uint16_t patlen) {
  uint32_t hash = DFC_InitHashRaw(pat, patlen);

  if (ctx->init_hash == NULL) {
    return NULL;
  }

  DFC_PATTERN *t = ctx->init_hash[hash];
  for (; t != NULL; t = t->next) {
    if (!strncmp((char *)t->casepatrn, (char *)pat, patlen)) return t;
  }

  return NULL;
}

static inline int DFC_InitHashAdd(DFC_PATTERN_INIT *ctx, DFC_PATTERN *p) {
  uint32_t hash = DFC_InitHashRaw(p->casepatrn, p->n);

  if (ctx->init_hash == NULL) {
    return 0;
  }

  if (ctx->init_hash[hash] == NULL) {
    ctx->init_hash[hash] = p;
    return 0;
  }

  DFC_PATTERN *tt = NULL;
  DFC_PATTERN *t = ctx->init_hash[hash];

  /* get the list tail */
  do {
    tt = t;
    t = t->next;
  } while (t != NULL);

  tt->next = p;

  return 0;
}

static DFC_FIXED_PATTERN createFixed(DFC_PATTERN *original) {
  if (original->n >= MAX_PATTERN_LENGTH) {
    fprintf(stderr,
            "Pattern %d \"%s\" is too long with length %d. Please remove it or "
            "increase "
            "MAX_PATTERN_LENGTH. (Currently %d)\n",
            original->iid, original->casepatrn, original->n,
            MAX_PATTERN_LENGTH);
    exit(PATTERN_TOO_LARGE_EXIT_CODE);
  }
  if (original->sids_size >= MAX_EQUAL_PATTERNS) {
    fprintf(stderr,
            "Too many patterns that are equal, but with different ID. Please "
            "either manually cull duplicates or increase MAX_EQUAL_PATTERNS. "
            "(Currently %d)\n",
            MAX_EQUAL_PATTERNS);
    exit(TOO_MANY_EQUAL_PATTERNS_EXIT_CODE);
  }

  DFC_FIXED_PATTERN new;

  new.pattern_length = original->n;
  new.is_case_insensitive = original->is_case_insensitive;

  for (int i = 0; i < original->n; ++i) {
    new.upper_case_pattern[i] = original->patrn[i];
    new.original_pattern[i] = original->casepatrn[i];
  }

  new.external_id_count = original->sids_size;
  for (uint32_t i = 0; i < original->sids_size; ++i) {
    new.external_ids[i] = original->sids[i];
  }

  return new;
}

static void setupPatternListFromHash(DFC_PATTERN_INIT *init) {
  int begin_node_flag = 1;
  for (int i = 0; i < INIT_HASH_SIZE; i++) {
    DFC_PATTERN *node = init->init_hash[i], *prev_node;
    int first_node_flag = 1;
    while (node != NULL) {
      if (begin_node_flag) {
        begin_node_flag = 0;
        init->dfcPatterns = node;
      } else {
        if (first_node_flag) {
          first_node_flag = 0;
          prev_node->next = node;
        }
      }
      prev_node = node;
      node = node->next;
    }
  }
}

static void setupMatchList(DFC_PATTERN_INIT *init, DFC_PATTERNS *patterns) {
  patterns->numPatterns = init->numPatterns;

  for (DFC_PATTERN *plist = init->dfcPatterns; plist != NULL;
       plist = plist->next) {
    patterns->dfcMatchList[plist->iid] = createFixed(plist);
  }
}

static void setupDirectFilters(DFC_STRUCTURE *dfc, DFC_PATTERN_INIT *patterns) {
  for (DFC_PATTERN *plist = patterns->dfcPatterns; plist != NULL;
       plist = plist->next) {
    if (plist->n >= SMALL_DF_MIN_PATTERN_SIZE &&
        plist->n <= SMALL_DF_MAX_PATTERN_SIZE) {
      addPatternToSmallDirectFilter(dfc, plist);
    } else {
      addPatternToLargeDirectFilter(dfc, plist);
      addPatternToLargeDirectFilterHash(dfc, plist);
    }
  }
}

static void createPermutations(uint8_t *pattern, int patternLength,
                               int permutationCount, uint8_t *permutations) {
  uint8_t *shouldToggleCase = (uint8_t *)malloc(patternLength);
  for (int i = 0; i < permutationCount; ++i) {
    for (int j = patternLength - 1, k = 0; j >= 0; --j, ++k) {
      shouldToggleCase[k] = (i >> j) & 1;
    }

    for (int j = 0; j < patternLength; ++j) {
      if (shouldToggleCase[j]) {
        permutations[i * patternLength + j] = toggleCharacterCase(pattern[j]);
      } else {
        permutations[i * patternLength + j] = pattern[j];
      }
    }
  }

  free(shouldToggleCase);
}

static void maskPatternIntoDirectFilter(uint8_t *df, uint8_t *pattern) {
  uint16_t fragment_16 = (pattern[1] << 8) | pattern[0];
  uint16_t byteIndex = BINDEX(fragment_16 & DF_MASK);
  uint16_t bitMask = BMASK(fragment_16 & DF_MASK);

  df[byteIndex] |= bitMask;
}

static void add1BPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                            uint8_t pattern) {
  uint8_t newPattern[2];
  newPattern[0] = pattern;
  for (int j = 0; j < 256; j++) {
    newPattern[1] = j;

    maskPatternIntoDirectFilter(dfc->directFilterSmall, newPattern);
  }
}

static void addPatternToDirectFilter(uint8_t *directFilter,
                                     bool isCaseInsensitive, uint8_t *pattern,
                                     int patternLength) {
  if (isCaseInsensitive) {
    int permutationCount = 2 << (patternLength - 1);
    uint8_t *patternPermutations =
        (uint8_t *)malloc(permutationCount * patternLength);

    createPermutations(pattern, patternLength, permutationCount,
                       patternPermutations);

    for (int i = 0; i < permutationCount; ++i) {
      maskPatternIntoDirectFilter(directFilter,
                                  patternPermutations + (i * patternLength));
    }

    free(patternPermutations);
  } else {
    maskPatternIntoDirectFilter(directFilter, pattern);
  }
}

static void addLongerPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                                DFC_PATTERN *pattern) {
  addPatternToDirectFilter(dfc->directFilterSmall, pattern->is_case_insensitive,
                           pattern->casepatrn, 2);
}

static void addPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern) {
  assert(pattern->n >= SMALL_DF_MIN_PATTERN_SIZE);
  assert(pattern->n <= SMALL_DF_MAX_PATTERN_SIZE);

  if (pattern->n == 1) {
    add1BPatternToSmallDirectFilter(dfc, pattern->casepatrn[0]);
  } else {
    addLongerPatternToSmallDirectFilter(dfc, pattern);
  }
}

static void addPatternToLargeDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern) {
  assert(pattern->n > SMALL_DF_MAX_PATTERN_SIZE);

  addPatternToDirectFilter(dfc->directFilterLarge, pattern->is_case_insensitive,
                           pattern->casepatrn, 2);
}

static void maskPatternIntoDirectFilterHash(uint8_t *df, uint8_t *pattern) {
  uint32_t data =
      pattern[3] << 24 | pattern[2] << 16 | pattern[1] << 8 | pattern[0];
  uint16_t byteIndex = directFilterHash(data);
  uint16_t bitMask = BMASK(data & DF_MASK);

  df[byteIndex] |= bitMask;
}

static void addPatternToLargeDirectFilterHash(DFC_STRUCTURE *dfc,
                                              DFC_PATTERN *pattern) {
  int patternLength = 4;
  assert(pattern->n >= patternLength);

  if (pattern->is_case_insensitive) {
    int permutationCount = 2 << (patternLength - 1);
    uint8_t *patternPermutations =
        (uint8_t *)malloc(permutationCount * patternLength);

    createPermutations(pattern->casepatrn, patternLength, permutationCount,
                       patternPermutations);

    for (int i = 0; i < permutationCount; ++i) {
      maskPatternIntoDirectFilterHash(
          dfc->directFilterLargeHash,
          patternPermutations + (i * patternLength));
    }

    free(patternPermutations);
  } else {
    maskPatternIntoDirectFilterHash(dfc->directFilterLargeHash,
                                    pattern->casepatrn);
  }
}

static void pushPatternToSmallCompactTable(DynamicCtSmallEntry *ct,
                                           uint8_t pattern, PID_TYPE pid) {
  DynamicCtSmallEntry *entry = &ct[pattern];

  entry->pattern = pattern;
  entry->pids = realloc(entry->pids, (entry->pidCount + 1) * sizeof(PID_TYPE));
  if (!entry->pids) {
    fprintf(stderr, "Could not allocate memory for dynamic CT");
    exit(1);
  }

  entry->pids[entry->pidCount] = pid;
  ++entry->pidCount;
}

static void addPatternToSmallCompactTable(DynamicCtSmallEntry *ct,
                                          DFC_PATTERN *pattern) {
  assert(pattern->n >= SMALL_DF_MIN_PATTERN_SIZE);
  assert(pattern->n <= SMALL_DF_MAX_PATTERN_SIZE);

  uint8_t character = pattern->casepatrn[0];
  pushPatternToSmallCompactTable(ct, character, pattern->iid);

  if (pattern->is_case_insensitive) {
    pushPatternToSmallCompactTable(ct, toggleCharacterCase(character),
                                   pattern->iid);
  }
}

static DynamicCtLargeEntry *getEmptyOrEqualLargeCompactTableEntry(
    uint32_t pattern, int *entryCount, DynamicCtLargeEntry **entry) {
  int entryIndex = 0;
  while (entryIndex < *entryCount && (*entry + entryIndex)->pidCount &&
         (*entry + entryIndex)->pattern != pattern) {
    ++entryIndex;
  }

  if (entryIndex == *entryCount) {
    ++(*entryCount);
    *entry = realloc(*entry, (*entryCount) * sizeof(DynamicCtLargeEntry));

    if (!*entry) {
      fprintf(stderr, "Could not allocate dynamic large CT");
      exit(1);
    }

    (*entry + entryIndex)->pidCount = 0;
    (*entry + entryIndex)->pids = NULL;
  }

  return *entry + entryIndex;
}

static bool hasPid(DynamicCtLargeEntry *entry, PID_TYPE pid) {
  for (int i = 0; i < entry->pidCount; ++i) {
    if (entry->pids[i] == pid) {
      return true;
    }
  }

  return false;
}

static void pushPatternToLargeCompactTable(DynamicCtLarge *ct, uint32_t pattern,
                                           PID_TYPE pid) {
  uint32_t hash = hashForLargeCompactTable(pattern);
  DynamicCtLarge *bucket = &ct[hash];
  DynamicCtLargeEntry *entry = getEmptyOrEqualLargeCompactTableEntry(
      pattern, &bucket->entryCount, &bucket->entries);

  if (!hasPid(entry, pid)) {
    entry->pattern = pattern;
    entry->pids =
        realloc(entry->pids, (entry->pidCount + 1) * sizeof(PID_TYPE));

    if (!entry->pids) {
      fprintf(stderr, "Could not allocate memory for large dynamic CT");
      exit(1);
    }

    entry->pids[entry->pidCount] = pid;
    ++entry->pidCount;
  }
}

static void addPatternToLargeCompactTable(DynamicCtLarge *ct,
                                          DFC_PATTERN *pattern) {
  int patternLength = SMALL_DF_MAX_PATTERN_SIZE + 1;
  assert(pattern->n >= patternLength);

  uint8_t *firstCharactersOfPattern = pattern->casepatrn;

  if (pattern->is_case_insensitive) {
    int permutationCount = 2 << (patternLength - 1);
    uint8_t *patternPermutations =
        (uint8_t *)malloc(permutationCount * patternLength);

    createPermutations(firstCharactersOfPattern, patternLength,
                       permutationCount, patternPermutations);

    for (int i = 0; i < permutationCount; ++i) {
      uint32_t data = patternPermutations[i * patternLength + 3] << 24 |
                      patternPermutations[i * patternLength + 2] << 16 |
                      patternPermutations[i * patternLength + 1] << 8 |
                      patternPermutations[i * patternLength + 0];
      pushPatternToLargeCompactTable(ct, data, pattern->iid);
    }

    free(patternPermutations);
  } else {
    uint32_t data =
        firstCharactersOfPattern[3] << 24 | firstCharactersOfPattern[2] << 16 |
        firstCharactersOfPattern[1] << 8 | firstCharactersOfPattern[0];
    pushPatternToLargeCompactTable(ct, data, pattern->iid);
  }
}

static void setupCompactTables(DFC_PATTERN_INIT *patterns,
                               DynamicCtSmallEntry **ctSmall,
                               DynamicCtLarge **ctLarge) {
  *ctSmall = calloc(1, COMPACT_TABLE_SIZE_SMALL * sizeof(DynamicCtSmallEntry));
  *ctLarge = calloc(1, COMPACT_TABLE_SIZE_LARGE * sizeof(DynamicCtLarge));

  for (DFC_PATTERN *plist = patterns->dfcPatterns; plist != NULL;
       plist = plist->next) {
    if (plist->n >= SMALL_DF_MIN_PATTERN_SIZE &&
        plist->n <= SMALL_DF_MAX_PATTERN_SIZE) {
      addPatternToSmallCompactTable(*ctSmall, plist);
    } else {
      addPatternToLargeCompactTable(*ctLarge, plist);
    }
  }
}

static uint8_t toggleCharacterCase(uint8_t character) {
  if (character >= 97 /*a*/ && character <= 122 /*z*/) {
    return toupper(character);
  } else {
    return tolower(character);
  }
  return character;
}
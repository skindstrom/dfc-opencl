/*********************************/
/* Author  - Byungkwon Choi      */
/* Contact - cbkbrad@kaist.ac.kr */
/*********************************/

#include "dfc.h"
#include "assert.h"
#include "search.h"
#include "utility.h"

static unsigned char xlatcase[256];

static void *DFC_REALLOC(void *p, uint16_t n, dfcDataType type);
static void *DFC_MALLOC(int n);
static inline DFC_PATTERN *DFC_InitHashLookup(DFC_STRUCTURE *ctx, uint8_t *pat,
                                              uint16_t patlen);
static inline int DFC_InitHashAdd(DFC_STRUCTURE *ctx, DFC_PATTERN *p);

static void setupMatchList(DFC_STRUCTURE *dfc);

static void setupDirectFilters(DFC_STRUCTURE *dfc);
static void addPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern);
static void addPatternToLargeDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern);
static void addPatternToLargeDirectFilterHash(DFC_STRUCTURE *dfc,
                                              DFC_PATTERN *pattern);
static void createPermutations(uint8_t *pattern, int patternLength,
                               int permutationCount, uint8_t *permutations);
static void setupCompactTables(DFC_STRUCTURE *dfc);

static uint8_t toggleCharacterCase(uint8_t);

/*
 *  Create a new DFC state machine
 */
DFC_STRUCTURE *DFC_New(void) {
  DFC_STRUCTURE *p;

  init_xlatcase(xlatcase);

  p = (DFC_STRUCTURE *)DFC_MALLOC(sizeof(DFC_STRUCTURE));
  MEMASSERT_DFC(p, "DFC_New");

  if (p) {
    memset(p, 0, sizeof(DFC_STRUCTURE));
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
  if (p->patrn != NULL) {
    free(p->patrn);
  }

  if (p->casepatrn != NULL) {
    free(p->casepatrn);
  }

  return;
}

static void DFC_FreePatternList(DFC_STRUCTURE *dfc) {
  DFC_PATTERN *plist;
  DFC_PATTERN *p_next;

  for (plist = dfc->dfcPatterns; plist != NULL;) {
    DFC_FreePattern(plist);
    p_next = plist->next;
    free(plist);
    plist = p_next;
  }

  return;
}

void DFC_FreeStructure(DFC_STRUCTURE *dfc) {
  if (dfc == NULL) return;

  if (dfc->dfcPatterns != NULL) {
    DFC_FreePatternList(dfc);
  }

  if (dfc->dfcMatchList != NULL) {
    free(dfc->dfcMatchList);
  }

  free(dfc);
}

/*
 *  Add a pattern to the list of patterns
 *
 *
 * \param dfc    Pointer to the DFC structure
 * \param pat    Pointer to the pattern
 * \param n      Pattern length
 * \param is_case_insensitive Flag for case-sensitivity (0 means
 * case-sensitive, 1 means the opposite) \param sid    External id
 *
 * \retval   0 On success to add new pattern.
 * \retval   1 On success to add sid.
 */
int DFC_AddPattern(DFC_STRUCTURE *dfc, unsigned char *pat, int n,
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

    return 0;
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

    return 1;
  }
}

int DFC_Compile(DFC_STRUCTURE *dfc) {
  setupMatchList(dfc);
  setupDirectFilters(dfc);
  setupCompactTables(dfc);

  return 0;
}

int DFC_Search(DFC_STRUCTURE *dfc, uint8_t *input, int inputLength) {
  return search(dfc, input, inputLength);
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

/**
 * \internal
 * \brief Creates a hash of the pattern.  We use it for the hashing
 * process during the initial pattern insertion time, to cull duplicate
 * sigs.
 *
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 *
 * \retval hash A 32 bit unsigned hash.
 */
static inline uint32_t DFC_InitHashRaw(uint8_t *pat, uint16_t patlen) {
  uint32_t hash = patlen * pat[0];
  if (patlen > 1) hash += pat[1];

  return (hash % INIT_HASH_SIZE);
}

/**
 * \internal
 * \brief Looks up a pattern.  We use it for the hashing process during
 * the the initial pattern insertion time, to cull duplicate sigs.
 *
 * \param ctx    Pointer to the DFC structure.
 * \param pat    Pointer to the pattern.
 * \param patlen Pattern length.
 * \param pid    Pattern ID
 *
 */
static inline DFC_PATTERN *DFC_InitHashLookup(DFC_STRUCTURE *ctx, uint8_t *pat,
                                              uint16_t patlen) {
  uint32_t hash = DFC_InitHashRaw(pat, patlen);

  if (ctx->init_hash == NULL) {
    return NULL;
  }

  DFC_PATTERN *t = ctx->init_hash[hash];
  for (; t != NULL; t = t->next) {
    if (!strcmp((char *)t->casepatrn, (char *)pat)) return t;
  }

  return NULL;
}

static inline int DFC_InitHashAdd(DFC_STRUCTURE *ctx, DFC_PATTERN *p) {
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
            "Pattern \"%s\" is too long. Please remove it or increase "
            "MAX_PATTERN_LENGTH. (Currently %d)\n",
            original->casepatrn, MAX_PATTERN_LENGTH);
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

static void setupMatchList(DFC_STRUCTURE *dfc) {
  int begin_node_flag = 1;
  for (int i = 0; i < INIT_HASH_SIZE; i++) {
    DFC_PATTERN *node = dfc->init_hash[i], *prev_node;
    int first_node_flag = 1;
    while (node != NULL) {
      if (begin_node_flag) {
        begin_node_flag = 0;
        dfc->dfcPatterns = node;
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

  free(dfc->init_hash);
  dfc->init_hash = NULL;

  dfc->dfcMatchList = (DFC_FIXED_PATTERN *)DFC_MALLOC(
      sizeof(DFC_FIXED_PATTERN) * dfc->numPatterns);
  MEMASSERT_DFC(dfc->dfcMatchList, "setupMatchList");

  for (DFC_PATTERN *plist = dfc->dfcPatterns; plist != NULL;
       plist = plist->next) {
    dfc->dfcMatchList[plist->iid] = createFixed(plist);
  }
}

static void setupDirectFilters(DFC_STRUCTURE *dfc) {
  for (DFC_PATTERN *plist = dfc->dfcPatterns; plist != NULL;
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
  uint8_t *lastCharactersOfPattern = pattern->casepatrn + ((pattern->n - 2));
  addPatternToDirectFilter(dfc->directFilterSmall, pattern->is_case_insensitive,
                           lastCharactersOfPattern, 2);
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

  uint8_t *lastCharactersOfPattern = pattern->casepatrn + ((pattern->n - 2));
  addPatternToDirectFilter(dfc->directFilterLarge, pattern->is_case_insensitive,
                           lastCharactersOfPattern, 2);
}

static void maskPatternIntoDirectFilterHash(uint8_t *df, uint8_t *pattern) {
  uint32_t data = *(uint32_t *)pattern;
  uint16_t byteIndex = directFilterHash(data);
  uint16_t bitMask = BMASK(data & DF_MASK);

  df[byteIndex] |= bitMask;
}

static void addPatternToLargeDirectFilterHash(DFC_STRUCTURE *dfc,
                                              DFC_PATTERN *pattern) {
  int patternLength = 4;
  assert(pattern->n >= patternLength);

  uint8_t *lastCharactersOfPattern =
      pattern->casepatrn + ((pattern->n - patternLength));
  if (pattern->is_case_insensitive) {
    int permutationCount = 2 << (patternLength - 1);
    uint8_t *patternPermutations =
        (uint8_t *)malloc(permutationCount * patternLength);

    createPermutations(lastCharactersOfPattern, patternLength, permutationCount,
                       patternPermutations);

    for (int i = 0; i < permutationCount; ++i) {
      maskPatternIntoDirectFilterHash(
          dfc->directFilterLargeHash,
          patternPermutations + (i * patternLength));
    }

    free(patternPermutations);
  } else {
    maskPatternIntoDirectFilterHash(dfc->directFilterLargeHash,
                                    lastCharactersOfPattern);
  }
}

static void pushPatternToSmallCompactTable(DFC_STRUCTURE *dfc, uint8_t pattern,
                                           PID_TYPE pid) {
  CompactTableSmallEntry *entry = &dfc->compactTableSmall[pattern];

  if (entry->pidCount == MAX_PID_PER_ENTRY) {
    fprintf(stderr,
            "Too many equal patterns in the small compact hash table."
            "Please increase MAX_PID_PER_ENTRY, MAX_ENTRIES_PER_BUCKET or "
            "update the hash function");
    exit(TOO_MANY_PID_IN_SMALL_CT_EXIT_CODE);
  }

  entry->pattern = pattern;
  entry->pids[entry->pidCount] = pid;
  ++entry->pidCount;
}

static void addPatternToSmallCompactTable(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern) {
  assert(pattern->n >= SMALL_DF_MIN_PATTERN_SIZE);
  assert(pattern->n <= SMALL_DF_MAX_PATTERN_SIZE);

  uint8_t character = pattern->n == 1 ? pattern->casepatrn[0]
                                      : pattern->casepatrn[pattern->n - 2];
  pushPatternToSmallCompactTable(dfc, character, pattern->iid);

  if (pattern->is_case_insensitive) {
    pushPatternToSmallCompactTable(dfc, toggleCharacterCase(character),
                                   pattern->iid);
  }
}

static void getEmptyOrEqualLargeCompactTableEntry(
    uint32_t pattern, CompactTableLargeEntry **entry) {
  int entryCount = 0;
  while ((*entry)->pidCount && (*entry)->pattern != pattern &&
         entryCount < MAX_ENTRIES_PER_BUCKET) {
    *entry += sizeof(CompactTableLargeEntry);
    ++entryCount;
  }

  if (entryCount == MAX_ENTRIES_PER_BUCKET) {
    fprintf(stderr,
            "Too many entries with the same hash in the large compact table."
            "Please increase MAX_ENTRIES_PER_BUCKET or update the hash "
            "function");
    exit(TOO_MANY_ENTRIES_IN_LARGE_CT_EXIT_CODE);
  }

  if ((*entry)->pidCount == MAX_PID_PER_ENTRY) {
    fprintf(stderr,
            "Too many equal patterns in the large compact hash table."
            "Please increase MAX_PID_PER_ENTRY, MAX_ENTRIES_PER_BUCKET or "
            "update the hash function");
    exit(TOO_MANY_PID_IN_LARGE_CT_EXIT_CODE);
  }
}

static bool hasPid(CompactTableLargeEntry *entry, PID_TYPE pid) {
  for (int i = 0; i < entry->pidCount; ++i) {
    if (entry->pids[i] == pid) {
      return true;
    }
  }

  return false;
}

static void pushPatternToLargeCompactTable(DFC_STRUCTURE *dfc, uint32_t pattern,
                                           PID_TYPE pid) {
  uint32_t hash = hashForLargeCompactTable(pattern);
  CompactTableLargeEntry *entry = dfc->compactTableLarge[hash].entries;

  getEmptyOrEqualLargeCompactTableEntry(pattern, &entry);

  if (!hasPid(entry, pid)) {
    entry->pattern = pattern;
    entry->pids[entry->pidCount] = pid;
    ++entry->pidCount;
  }
}

static void addPatternToLargeCompactTable(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern) {
  int patternLength = SMALL_DF_MAX_PATTERN_SIZE + 1;
  assert(pattern->n >= patternLength);

  uint8_t *lastCharactersOfPattern =
      pattern->casepatrn + (pattern->n - patternLength);

  if (pattern->is_case_insensitive) {
    int permutationCount = 2 << (patternLength - 1);
    uint8_t *patternPermutations =
        (uint8_t *)malloc(permutationCount * patternLength);

    createPermutations(lastCharactersOfPattern, patternLength, permutationCount,
                       patternPermutations);

    for (int i = 0; i < permutationCount; ++i) {
      pushPatternToLargeCompactTable(
          dfc, *(uint32_t *)(patternPermutations + (i * patternLength)),
          pattern->iid);
    }

    free(patternPermutations);
  } else {
    pushPatternToLargeCompactTable(dfc, *(uint32_t *)lastCharactersOfPattern,
                                   pattern->iid);
  }
}

static void setupCompactTables(DFC_STRUCTURE *dfc) {
  for (DFC_PATTERN *plist = dfc->dfcPatterns; plist != NULL;
       plist = plist->next) {
    if (plist->n >= SMALL_DF_MIN_PATTERN_SIZE &&
        plist->n <= SMALL_DF_MAX_PATTERN_SIZE) {
      addPatternToSmallCompactTable(dfc, plist);
    } else {
      addPatternToLargeCompactTable(dfc, plist);
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
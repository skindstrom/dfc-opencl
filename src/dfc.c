/*********************************/
/* Author  - Byungkwon Choi      */
/* Contact - cbkbrad@kaist.ac.kr */
/*********************************/

#include "dfc.h"
#include "assert.h"
#include "feature-flags.h"
#include "utility.h"

static unsigned char xlatcase[256];

/* For extracting position */
static const int pattern_interval = 32;
static const int min_pattern_interval = 32;

static int dfc_total_memory = 0;
static int dfc_pattern_memory = 0;
static int dfc_memory_dfs = (sizeof(uint8_t) * DF_SIZE_REAL) * 14;
static int dfc_memory_ct1 = 0;
static int dfc_memory_ct2 = 0;
static int dfc_memory_ct3 = 0;
static int dfc_memory_ct4 = 0;
static int dfc_memory_ct8 = 0;

static void *DFC_REALLOC(void *p, uint16_t n, dfcDataType type,
                         dfcMemoryType type2);
static void *DFC_MALLOC(int n, dfcMemoryType type);
static void Build_pattern(DFC_PATTERN *p, uint8_t *flag, uint8_t *temp, int j,
                          int k);
static inline DFC_PATTERN *DFC_InitHashLookup(DFC_STRUCTURE *ctx, uint8_t *pat,
                                              uint16_t patlen);
static inline int DFC_InitHashAdd(DFC_STRUCTURE *ctx, DFC_PATTERN *p);

static void initializeMemoryTracking();
static void setupMatchList(DFC_STRUCTURE *dfc);

static void initializeDirectFilterMemory(DFC_STRUCTURE *dfc);
static void initializeSmallDirectFilterMemory(DFC_STRUCTURE *dfc);

static void setupDirectFilters(DFC_STRUCTURE *dfc);
static void setup1BDirectFilter(DFC_STRUCTURE *dfc, DFC_PATTERN *plist);
static void setup1BDirectFilterWithPattern(DFC_STRUCTURE *dfc, uint8_t pattern);
static void setup1BPlusDirectFilter(DFC_STRUCTURE *dfc, DFC_PATTERN *plist);
static void addPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern);
static void addPatternToLargeDirectFilter(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern);
static void createPermutations(uint8_t *pattern, int patternLength,
                               int permutationCount, uint8_t *permutations);
static void initializeCompactTableMemory(DFC_STRUCTURE *dfc);
static void setupCompactTables(DFC_STRUCTURE *dfc);

static uint8_t toggleCharacterCase(uint8_t);

/*
 *  Create a new DFC state machine
 */
DFC_STRUCTURE *DFC_New(void) {
  DFC_STRUCTURE *p;

  init_xlatcase(xlatcase);

  dfc_total_memory = 0;
  dfc_pattern_memory = 0;

  p = (DFC_STRUCTURE *)DFC_MALLOC(sizeof(DFC_STRUCTURE), DFC_MEMORY_TYPE__DFC);
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
  BUC_CNT_TYPE j;
  int i;
  if (dfc == NULL) return;

  if (dfc->dfcPatterns != NULL) {
    DFC_FreePatternList(dfc);
  }

  if (dfc->dfcMatchList != NULL) {
    free(dfc->dfcMatchList);
  }

  for (i = 0; i < CT2_TABLE_SIZE; i++) {
    for (j = 0; j < dfc->CompactTable2[i].cnt; j++) {
      free(dfc->CompactTable2[i].array[j].pid);
    }
  }

  for (i = 0; i < CT4_TABLE_SIZE; i++) {
    for (j = 0; j < dfc->CompactTable4[i].cnt; j++) {
      free(dfc->CompactTable4[i].array[j].pid);
    }
  }

  for (i = 0; i < CT8_TABLE_SIZE; i++) {
    for (j = 0; j < dfc->CompactTable8[i].cnt; j++) {
      free(dfc->CompactTable8[i].array[j].pid);
    }
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
    plist = (DFC_PATTERN *)DFC_MALLOC(sizeof(DFC_PATTERN),
                                      DFC_MEMORY_TYPE__PATTERN);
    MEMASSERT_DFC(plist, "DFC_AddPattern");
    memset(plist, 0, sizeof(DFC_PATTERN));

    plist->patrn = (unsigned char *)DFC_MALLOC(n, DFC_MEMORY_TYPE__PATTERN);
    MEMASSERT_DFC(plist->patrn, "DFC_AddPattern");

    ConvertCaseEx(plist->patrn, pat, n, xlatcase);

    plist->casepatrn = (unsigned char *)DFC_MALLOC(n, DFC_MEMORY_TYPE__PATTERN);
    MEMASSERT_DFC(plist->casepatrn, "DFC_AddPattern");

    memcpy(plist->casepatrn, pat, n);

    plist->n = n;
    plist->is_case_insensitive = is_case_insensitive;
    plist->iid = dfc->numPatterns;  // internal id
    plist->next = NULL;

    DFC_InitHashAdd(dfc, plist);

    /* sid update */
    plist->sids_size = 1;
    plist->sids =
        (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__PATTERN);
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
      PID_TYPE *sids =
          (PID_TYPE *)DFC_REALLOC(plist->sids, plist->sids_size + 1,
                                  DFC_PID_TYPE, DFC_MEMORY_TYPE__PATTERN);
      plist->sids = sids;
      plist->sids[plist->sids_size] = sid;
      plist->sids_size++;
    }

    return 1;
  }
}

void DFC_PrintInfo(DFC_STRUCTURE *dfc) {
  DFC_PATTERN *plist;
  BUC_CNT_TYPE j;
  int i;
  int nl1 = 0, nl2 = 0, nl3 = 0, nl4 = 0, nl8 = 0;

  int is_case_insensitive_pat_cnt = 0;
  for (plist = dfc->dfcPatterns; plist != NULL; plist = plist->next) {
    if (plist->n == 1) {
      nl1++;
    } else if (plist->n == 2) {
      nl2++;
    } else if (plist->n == 3) {
      nl3++;
    } else if (plist->n >= 4 && plist->n < 8) {
      nl4++;
    } else if (plist->n >= 8) {
      nl8++;
    }

    if (plist->is_case_insensitive) {
      is_case_insensitive_pat_cnt++;
    }
  }

  int nb1 = 0, nb2 = 0, nb3 = 0, nb4 = 0, nb5 = 0, nb8 = 0;
  int nb9 = 0;
  for (i = 0; i < DF_SIZE_REAL; i++)
    nb1 += __builtin_popcount(dfc->DirectFilter1[i]);
  for (i = 0; i < DF_SIZE_REAL; i++)
    nb2 += __builtin_popcount(dfc->ADD_DF_4_plus[i]);
  for (i = 0; i < DF_SIZE_REAL; i++)
    nb3 += __builtin_popcount(dfc->ADD_DF_8_1[i]);
  for (i = 0; i < 256; i++) nb4 += __builtin_popcount(dfc->cDF0[i]);
  for (i = 0; i < DF_SIZE_REAL; i++)
    nb8 += __builtin_popcount(dfc->ADD_DF_4_1[i]);
  for (i = 0; i < DF_SIZE_REAL; i++) nb9 += __builtin_popcount(dfc->cDF2[i]);

  /* CT 1*/
  int ct1_array_cnt = 0;
  int ct1_pid_cnt = 0;
  float ct1_pid_std_dev = 0;
  for (i = 0; i < CT1_TABLE_SIZE; i++) {
    ct1_pid_cnt += dfc->CompactTable1[i].cnt;
    if (dfc->CompactTable1[i].cnt != 0) ct1_array_cnt++;
  }
  for (i = 0; i < CT1_TABLE_SIZE; i++) {
    ct1_pid_std_dev +=
        (dfc->CompactTable1[i].cnt - ((float)ct1_pid_cnt / CT1_TABLE_SIZE)) *
        (dfc->CompactTable1[i].cnt - ((float)ct1_pid_cnt / CT1_TABLE_SIZE));
  }
  ct1_pid_std_dev /= ct1_pid_cnt;
  ct1_pid_std_dev = my_sqrtf(ct1_pid_std_dev, ct1_pid_std_dev);

  /* CT 2*/
  BUC_CNT_TYPE ct2_array_min = 9999999;
  BUC_CNT_TYPE ct2_array_max = 0;
  PID_CNT_TYPE ct2_pid_min = 9999999;
  PID_CNT_TYPE ct2_pid_max = 0;
  int ct2_array_cnt = 0;
  int ct2_array_tot_cnt = 0;
  int ct2_pid_cnt = 0;
  int ct2_pid_tot_cnt = 0;
  float ct2_array_std_dev = 0;
  float ct2_pid_std_dev = 0;
  for (i = 0; i < CT2_TABLE_SIZE; i++) {
    ct2_array_tot_cnt += dfc->CompactTable2[i].cnt;
    if (ct2_array_min > dfc->CompactTable2[i].cnt)
      ct2_array_min = dfc->CompactTable2[i].cnt;
    if (ct2_array_max < dfc->CompactTable2[i].cnt)
      ct2_array_max = dfc->CompactTable2[i].cnt;
    if (dfc->CompactTable2[i].cnt != 0) ct2_array_cnt++;
    for (j = 0; j < dfc->CompactTable2[i].cnt; j++) {
      ct2_pid_tot_cnt += dfc->CompactTable2[i].array[j].cnt;
      if (ct2_pid_min > dfc->CompactTable2[i].array[j].cnt)
        ct2_pid_min = dfc->CompactTable2[i].array[j].cnt;
      if (ct2_pid_max < dfc->CompactTable2[i].array[j].cnt)
        ct2_pid_max = dfc->CompactTable2[i].array[j].cnt;
      if (dfc->CompactTable2[i].array[j].cnt != 0) ct2_pid_cnt++;
    }
  }
  for (i = 0; i < CT2_TABLE_SIZE; i++) {
    if (dfc->CompactTable2[i].cnt != 0) {
      ct2_array_std_dev += (dfc->CompactTable2[i].cnt -
                            ((float)ct2_array_tot_cnt / ct2_array_cnt)) *
                           (dfc->CompactTable2[i].cnt -
                            ((float)ct2_array_tot_cnt / ct2_array_cnt));
    }
    for (j = 0; j < dfc->CompactTable2[i].cnt; j++) {
      if (dfc->CompactTable2[i].array[j].cnt != 0) {
        ct2_pid_std_dev += (dfc->CompactTable2[i].array[j].cnt -
                            ((float)ct2_pid_tot_cnt / ct2_pid_cnt)) *
                           (dfc->CompactTable2[i].array[j].cnt -
                            ((float)ct2_pid_tot_cnt / ct2_pid_cnt));
      }
    }
  }

  ct2_array_std_dev /= ct2_array_cnt;
  ct2_pid_std_dev /= ct2_pid_cnt;

  ct2_array_std_dev = my_sqrtf(ct2_array_std_dev, ct2_array_std_dev);
  ct2_pid_std_dev = my_sqrtf(ct2_pid_std_dev, ct2_pid_std_dev);

  /* CT4 */
  BUC_CNT_TYPE ct4_array_min = 9999999;
  BUC_CNT_TYPE ct4_array_max = 0;
  PID_CNT_TYPE ct4_pid_min = 9999999;
  PID_CNT_TYPE ct4_pid_max = 0;
  int ct4_array_cnt = 0;
  int ct4_mt2f_array_cnt = 0;  // # of buckets that has more than 2 frags (mt2f
                               // : more than 2 fragments)
  int ct4_mt2f_frag_cnt = 0;
  int ct4_array_tot_cnt = 0;
  int ct4_pid_cnt = 0;
  int ct4_pid_tot_cnt = 0;
  float ct4_array_std_dev = 0;
  float ct4_pid_std_dev = 0;
  for (i = 0; i < CT4_TABLE_SIZE; i++) {
    ct4_array_tot_cnt += dfc->CompactTable4[i].cnt;
    if (ct4_array_min > dfc->CompactTable4[i].cnt)
      ct4_array_min = dfc->CompactTable4[i].cnt;
    if (ct4_array_max < dfc->CompactTable4[i].cnt)
      ct4_array_max = dfc->CompactTable4[i].cnt;
    if (dfc->CompactTable4[i].cnt != 0) ct4_array_cnt++;
    if (dfc->CompactTable4[i].cnt >= 2) {
      ct4_mt2f_array_cnt++;
      ct4_mt2f_frag_cnt += (dfc->CompactTable4[i].cnt - 1);
    }
    for (j = 0; j < dfc->CompactTable4[i].cnt; j++) {
      ct4_pid_tot_cnt += dfc->CompactTable4[i].array[j].cnt;
      if (ct4_pid_min > dfc->CompactTable4[i].array[j].cnt)
        ct4_pid_min = dfc->CompactTable4[i].array[j].cnt;
      if (ct4_pid_max < dfc->CompactTable4[i].array[j].cnt)
        ct4_pid_max = dfc->CompactTable4[i].array[j].cnt;
      if (dfc->CompactTable4[i].array[j].cnt != 0) ct4_pid_cnt++;
    }
  }
  for (i = 0; i < CT4_TABLE_SIZE; i++) {
    if (dfc->CompactTable4[i].cnt != 0) {
      ct4_array_std_dev += (dfc->CompactTable4[i].cnt -
                            ((float)ct4_array_tot_cnt / ct4_array_cnt)) *
                           (dfc->CompactTable4[i].cnt -
                            ((float)ct4_array_tot_cnt / ct4_array_cnt));
    }
    for (j = 0; j < dfc->CompactTable4[i].cnt; j++) {
      if (dfc->CompactTable4[i].array[j].cnt != 0) {
        ct4_pid_std_dev += (dfc->CompactTable4[i].array[j].cnt -
                            ((float)ct4_pid_tot_cnt / ct4_pid_cnt)) *
                           (dfc->CompactTable4[i].array[j].cnt -
                            ((float)ct4_pid_tot_cnt / ct4_pid_cnt));
      }
    }
  }

  ct4_array_std_dev /= ct4_array_cnt;
  ct4_pid_std_dev /= ct4_pid_cnt;

  ct4_array_std_dev = my_sqrtf(ct4_array_std_dev, ct4_array_std_dev);
  ct4_pid_std_dev = my_sqrtf(ct4_pid_std_dev, ct4_pid_std_dev);

  /* CT8 */
  BUC_CNT_TYPE ct8_array_min = 9999999;
  BUC_CNT_TYPE ct8_array_max = 0;
  PID_CNT_TYPE ct8_pid_min = 9999999;
  PID_CNT_TYPE ct8_pid_max = 0;
  int ct8_array_cnt = 0;
  int ct8_mt2f_array_cnt = 0;  // # of buckets that has more than 2 frags (mt2f
                               // : more than 2 fragments)
  int ct8_mt2f_frag_cnt = 0;
  int ct8_array_tot_cnt = 0;
  int ct8_pid_cnt = 0;
  int ct8_pid_tot_cnt = 0;
  float ct8_array_std_dev = 0;
  float ct8_pid_std_dev = 0;
  for (i = 0; i < CT8_TABLE_SIZE; i++) {
    ct8_array_tot_cnt += dfc->CompactTable8[i].cnt;
    if (ct8_array_min > dfc->CompactTable8[i].cnt)
      ct8_array_min = dfc->CompactTable8[i].cnt;
    if (ct8_array_max < dfc->CompactTable8[i].cnt)
      ct8_array_max = dfc->CompactTable8[i].cnt;
    if (dfc->CompactTable8[i].cnt != 0) ct8_array_cnt++;
    if (dfc->CompactTable8[i].cnt >= 2) {
      ct8_mt2f_array_cnt++;
      ct8_mt2f_frag_cnt += (dfc->CompactTable8[i].cnt - 1);
    }
    for (j = 0; j < dfc->CompactTable8[i].cnt; j++) {
      ct8_pid_tot_cnt += dfc->CompactTable8[i].array[j].cnt;
      if (ct8_pid_min > dfc->CompactTable8[i].array[j].cnt)
        ct8_pid_min = dfc->CompactTable8[i].array[j].cnt;
      if (ct8_pid_max < dfc->CompactTable8[i].array[j].cnt)
        ct8_pid_max = dfc->CompactTable8[i].array[j].cnt;
      if (dfc->CompactTable8[i].array[j].cnt != 0) ct8_pid_cnt++;
    }
  }
  for (i = 0; i < CT8_TABLE_SIZE; i++) {
    if (dfc->CompactTable8[i].cnt != 0) {
      ct8_array_std_dev += (dfc->CompactTable8[i].cnt -
                            ((float)ct8_array_tot_cnt / ct8_array_cnt)) *
                           (dfc->CompactTable8[i].cnt -
                            ((float)ct8_array_tot_cnt / ct8_array_cnt));
    }
    for (j = 0; j < dfc->CompactTable8[i].cnt; j++) {
      if (dfc->CompactTable8[i].array[j].cnt != 0) {
        ct8_pid_std_dev += (dfc->CompactTable8[i].array[j].cnt -
                            ((float)ct8_pid_tot_cnt / ct8_pid_cnt)) *
                           (dfc->CompactTable8[i].array[j].cnt -
                            ((float)ct8_pid_tot_cnt / ct8_pid_cnt));
      }
    }
  }

  ct8_array_std_dev /= ct8_array_cnt;
  ct8_pid_std_dev /= ct8_pid_cnt;

  ct8_array_std_dev = my_sqrtf(ct8_array_std_dev, ct8_array_std_dev);
  ct8_pid_std_dev = my_sqrtf(ct8_pid_std_dev, ct8_pid_std_dev);

  printf("\n");
  printf(
      "+- [ Direct Filter + Compact Table(DFC) Summary ] "
      "-------------------------------------\n");
  printf("| Patterns: %5d (Case-Insensitive patterns: %d)\n", dfc->numPatterns,
         is_case_insensitive_pat_cnt);
  printf("|   - 1B      : %5d\n", nl1);
  printf("|   - 2B      : %5d\n", nl2);
  printf("|   - 3B      : %5d\n", nl3);
  printf("|   - 4B ~ 7B : %5d\n", nl4);
  printf("|   - 8B ~    : %5d\n", nl8);
  printf("|\n");
  printf("| < Direct Filter Density > \n");
  printf("|   1. All patterns(DF1)            : %.6f (%4d)\n",
         (double)nb1 / DF_SIZE, nb1);
  printf("|     1) 1B patterns(DF4)           : %.6f (%4d)\n",
         (double)nb4 / DF_SIZE, nb4);
  printf("|     2) 2B patterns(DF5)           : %.6f (%4d)\n",
         (double)nb5 / DF_SIZE, nb5);
  printf("|     4) 4B ~ patterns(DF2)         : %.6f (%4d)\n",
         (double)nb2 / DF_SIZE, nb2);
  printf("|       (1) 4B ~ 7B patterns(DF8)   : %.6f (%4d)\n",
         (double)nb8 / DF_SIZE, nb8);
  printf("|          I. 4B ~ 7B patterns(DF9) : %.6f (%4d)\n",
         (double)nb9 / DF_SIZE, nb9);
  printf("|       (2) 8B ~ patterns(DF3)      : %.6f (%4d)\n",
         (double)nb3 / DF_SIZE, nb3);
  printf("|\n");
  printf("| < Compact Table 1 Density >\n");
  printf("|   - Compact Table 1(CT1) Size   : %u\n", CT1_TABLE_SIZE);
  printf("|   - Number of non-empty buckets : %d\n", ct1_array_cnt);
  printf("|   - Total num of PIDs in CT1    : %d\n", ct1_pid_cnt);
  printf("|   - Number of PIDs per bucket   : %.3f\n",
         (double)ct1_pid_cnt / ct1_array_cnt);
  printf("|   - Std Deviation for # of PID  : %.3f\n", ct1_pid_std_dev);
  printf("|\n");
  printf("| < Compact Table 2 Density >\n");
  printf("|   - Compact Table 2(CT2) Size   : %u\n", CT2_TABLE_SIZE);
  printf("|   - Total # of buckets in CT2   : %d\n", ct2_array_cnt);
  printf("|   - Avr # of fragments/bucket   : %.3f (none zero)\n",
         (double)ct2_array_tot_cnt / ct2_array_cnt);
  printf("|   - Std Dev for # of fragments  : %.3f\n", ct2_array_std_dev);
  printf("|   - Min/Max for # of fragments  : %u / %u\n", ct2_array_min,
         ct2_array_max);
  printf("|\n");
  printf("|     - Total # of PIDs in CT2    : %d\n", ct2_pid_tot_cnt);
  printf("|     - Avr # of PIDs per frag    : %.3f (none zero)\n",
         (double)ct2_pid_tot_cnt / ct2_pid_cnt);
  printf("|     - Std Dev for # of PID      : %.3f\n", ct2_pid_std_dev);
  printf("|     - Min/Max for # of PID      : %u / %u\n", ct2_pid_min,
         ct2_pid_max);
  printf("|\n");
  printf("| < Compact Table 4 Density >\n");
  printf("|   - Compact Table 4(CT4) Size   : %u\n", CT4_TABLE_SIZE);
  printf("|   - Total # of buckets in CT4   : %d\n", ct4_array_cnt);
  printf(
      "|   - Avr # of fragments/bucket   : %.3f (collision rate: %.5f "
      "(%d/%d))\n",
      (double)ct4_array_tot_cnt / ct4_array_cnt,
      (double)ct4_mt2f_frag_cnt / ct4_array_cnt, ct4_mt2f_frag_cnt,
      ct4_array_cnt);
  printf("|   - Std Dev for # of fragments  : %.3f\n", ct4_array_std_dev);
  printf("|   - Min/Max for # of fragments  : %u / %u\n", ct4_array_min,
         ct4_array_max);
  printf("|\n");
  printf("|     - Total # of PIDs in CT4    : %d\n", ct4_pid_tot_cnt);
  printf("|     - Avr # of PIDs per frag    : %.3f (none zero)\n",
         (double)ct4_pid_tot_cnt / ct4_pid_cnt);
  printf("|     - Std Dev for # of PID      : %.3f\n", ct4_pid_std_dev);
  printf("|     - Min/Max for # of PID      : %u / %u\n", ct4_pid_min,
         ct4_pid_max);
  printf("|\n");
  printf("| < Compact Table 8 Density >\n");
  printf("|   - Compact Table 8(CT8) Size   : %u\n", CT8_TABLE_SIZE);
  printf("|   - Total # of buckets in CT8   : %d\n", ct8_array_cnt);
  printf(
      "|   - Avr # of fragments/bucket   : %.3f (collision rate: %.5f "
      "(%d/%d))\n",
      (double)ct8_array_tot_cnt / ct8_array_cnt,
      (double)ct8_mt2f_frag_cnt / ct8_array_cnt, ct8_mt2f_frag_cnt,
      ct8_array_cnt);
  printf("|   - Std Dev for # of fragments  : %.3f\n", ct8_array_std_dev);
  printf("|   - Min/Max for # of fragments  : %u / %u\n", ct8_array_min,
         ct8_array_max);
  printf("|\n");
  printf("|     - Total # of PIDs in CT8    : %d\n", ct8_pid_tot_cnt);
  printf("|     - Avr # of PIDs per frag    : %.3f (none zero)\n",
         (double)ct8_pid_tot_cnt / ct8_pid_cnt);
  printf("|     - Std Dev for # of PID      : %.3f\n", ct8_pid_std_dev);
  printf("|     - Min/Max for # of PID      : %u / %u\n", ct8_pid_min,
         ct8_pid_max);
  printf("|\n");

  if (dfc_total_memory < 1024 * 1024)
    printf("| Total Memory (KB) : %.2f\n", (float)dfc_total_memory / 1024);
  else
    printf("| Total Memory (MB) : %.2f\n",
           (float)dfc_total_memory / (1024 * 1024));

  if (dfc_pattern_memory < 1024 * 1024)
    printf("|   - Pattern Memory (KB) : %.2f\n",
           (float)dfc_pattern_memory / 1024);
  else
    printf("|   - Pattern Memory (MB) : %.2f\n",
           (float)dfc_pattern_memory / (1024 * 1024));

  if (dfc_memory_dfs < 1024 * 1024)
    printf("|   - DF Memory (KB) : %.2f\n", (float)dfc_memory_dfs / 1024);
  else
    printf("|   - DF Memory (MB) : %.2f\n",
           (float)dfc_memory_dfs / (1024 * 1024));

  if (dfc_memory_ct1 < 1024 * 1024)
    printf("|   - CT1 Memory (KB) : %.2f\n", (float)dfc_memory_ct1 / 1024);
  else
    printf("|   - CT1 Memory (MB) : %.2f\n",
           (float)dfc_memory_ct1 / (1024 * 1024));

  if (dfc_memory_ct2 < 1024 * 1024)
    printf("|   - CT2 Memory (KB) : %.2f\n", (float)dfc_memory_ct2 / 1024);
  else
    printf("|   - CT2 Memory (MB) : %.2f\n",
           (float)dfc_memory_ct2 / (1024 * 1024));

  if (dfc_memory_ct4 < 1024 * 1024)
    printf("|   - CT4 Memory (KB) : %.2f\n", (float)dfc_memory_ct4 / 1024);
  else
    printf("|   - CT4 Memory (MB) : %.2f\n",
           (float)dfc_memory_ct4 / (1024 * 1024));

  if (dfc_memory_ct8 < 1024 * 1024)
    printf("|   - CT8 Memory (KB) : %.2f\n", (float)dfc_memory_ct8 / 1024);
  else
    printf("|   - CT8 Memory (MB) : %.2f\n",
           (float)dfc_memory_ct8 / (1024 * 1024));
  printf(
      "+---------------------------------------------------------------------"
      "--"
      "---------------\n\n");

  return;
}

int DFC_Compile(DFC_STRUCTURE *dfc) {
  initializeMemoryTracking();

  setupMatchList(dfc);
  setupDirectFilters(dfc);
  setupCompactTables(dfc);

  return 0;
}

static int Verification_CT1(VERIFI_ARGUMENT) {
  (void)(starting_point);  // knowingly doesn't use the variable
  int i;
  for (i = 0; i < dfc->CompactTable1[*(buf - 2)].cnt; i++) {
    PID_TYPE pid = dfc->CompactTable1[*(buf - 2)].pid[i];
    DFC_PATTERN *mlist = dfc->dfcMatchList[pid];

    ACTION_FOR_MATCH;
  }
  return matches;
}

static int Verification_CT2(VERIFI_ARGUMENT) {
  uint32_t crc = _mm_crc32_u16(0, *(uint16_t *)(buf - 2));

  // 2. calculate index
  crc &= CT2_TABLE_SIZE_MASK;

  BUC_CNT_TYPE i;
  for (i = 0; i < dfc->CompactTable2[crc].cnt; i++) {
    if (dfc->CompactTable2[crc].array[i].pat == *(uint16_t *)(buf - 2)) {
      PID_CNT_TYPE j;
      for (j = 0; j < dfc->CompactTable2[crc].array[i].cnt; j++) {
        PID_TYPE pid = dfc->CompactTable2[crc].array[i].pid[j];

        DFC_PATTERN *mlist = dfc->dfcMatchList[pid];
        if (buf - starting_point >= mlist->n) {
          if (mlist->is_case_insensitive) {
            if (!my_strncasecmp(buf - (mlist->n), mlist->casepatrn,
                                mlist->n - 2)) {
              ACTION_FOR_MATCH;
            }
          } else {
            if (!my_strncmp(buf - (mlist->n), mlist->casepatrn, mlist->n - 2)) {
              ACTION_FOR_MATCH;
            }
          }
        }
      }
      break;
    }
  }
  return matches;
}

static int Verification_CT4_7(VERIFI_ARGUMENT) {
  // 1. Convert payload to uppercase
  unsigned char *temp = buf - 2;

  // 2. calculate crc
  uint32_t crc = _mm_crc32_u32(0, *(uint32_t *)temp);

  // 3. calculate index
  crc &= CT4_TABLE_SIZE_MASK;

  // 4.
  BUC_CNT_TYPE i;
  for (i = 0; i < dfc->CompactTable4[crc].cnt; i++) {
    if (dfc->CompactTable4[crc].array[i].pat == *(uint32_t *)temp) {
      PID_CNT_TYPE j;
      for (j = 0; j < dfc->CompactTable4[crc].array[i].cnt; j++) {
        PID_TYPE pid = dfc->CompactTable4[crc].array[i].pid[j];

        DFC_PATTERN *mlist = dfc->dfcMatchList[pid];
        if (buf - starting_point >= mlist->n - 2) {
          if (mlist->is_case_insensitive) {
            if (!my_strncasecmp(buf - (mlist->n - 2), mlist->casepatrn,
                                mlist->n - 4)) {
              ACTION_FOR_MATCH;
            }
          } else {
            if (!my_strncmp(buf - (mlist->n - 2), mlist->casepatrn,
                            mlist->n - 4)) {
              ACTION_FOR_MATCH;
            }
          }
        }
      }
    }
    break;
  }
  return matches;
}

static int Verification_CT8_plus(VERIFI_ARGUMENT) {
// 1. Convert payload to uppercase
#ifdef CT8_SWITCH
  unsigned char temp[8];
  ConvertCaseEx(temp, buf - 2, 8, xlatcase);
#else
  unsigned char *temp = buf - 2;
#endif

  // 2. calculate crc
  uint32_t fragment_32 =
      (temp[7] << 24) | (temp[6] << 16) | (temp[5] << 8) | temp[4];
  uint64_t fragment_64 = ((uint64_t)fragment_32 << 32) | (temp[3] << 24) |
                         (temp[2] << 16) | (temp[1] << 8) | temp[0];
  uint64_t crc = _mm_crc32_u64(0, fragment_64);

  // 3. calculate index
  crc &= CT8_TABLE_SIZE_MASK;

  BUC_CNT_TYPE i;
  for (i = 0; i < dfc->CompactTable8[crc].cnt; i++) {
    if (dfc->CompactTable8[crc].array[i].pat == fragment_64) {
      // matches++;	break;

      PID_CNT_TYPE j;
      for (j = 0; j < dfc->CompactTable8[crc].array[i].cnt; j++) {
        PID_TYPE pid = dfc->CompactTable8[crc].array[i].pid[j];

        DFC_PATTERN *mlist = dfc->dfcMatchList[pid];
        int comparison_requirement =
            min_pattern_interval * (mlist->n - 8) / pattern_interval + 2;
        if (buf - starting_point >= comparison_requirement) {
          if (mlist->is_case_insensitive) {
            if (!my_strncasecmp(buf - comparison_requirement, mlist->casepatrn,
                                mlist->n)) {
              ACTION_FOR_MATCH;
            }
          } else {
            if (!my_strncmp(buf - comparison_requirement, mlist->casepatrn,
                            mlist->n)) {
              ACTION_FOR_MATCH;
            }
          }
        }
      }
    }
    break;
  }

  return matches;
}

static inline int Progressive_Filtering(PROGRE_ARGUMENT) {
#ifdef ENABLE_PROGRESSIVE_FILTERING
  if (dfc->cDF0[*(buf - 2)]) {
    matches = Verification_CT1(VERIFI_PARAMETER);
  }

  if (unlikely(dfc->cDF1[idx] & msk)) {
    matches = Verification_CT2(VERIFI_PARAMETER);
  }

  if (rest_len >= 4) {
    DTYPE data = *(uint16_t *)(buf);
    BTYPE index = BINDEX(data);
    BTYPE mask = BMASK(data);

    if (unlikely((mask & dfc->ADD_DF_4_plus[index]))) {
      if (unlikely(mask & dfc->ADD_DF_4_1[index])) {
        matches = Verification_CT4_7(VERIFI_PARAMETER);
      }
      if ((rest_len >= 8)) {
        matches = Verification_CT8_plus(VERIFI_PARAMETER);
      }
    }
  }
#else
  matches = Verification_CT1(VERIFI_PARAMETER);
  matches = Verification_CT2(VERIFI_PARAMETER);
  matches = Verification_CT4_7(VERIFI_PARAMETER);
  matches = Verification_CT8_plus(VERIFI_PARAMETER);
#endif

  return matches;
}

int DFC_Search(SEARCH_ARGUMENT) {
  int i;
  int matches = 0;

  if (unlikely(buflen <= 0)) return 0;

  uint8_t *DirectFilter1 = dfc->DirectFilter1;

  for (i = 0; i < buflen - 1; i++) {
    DTYPE data = *(uint16_t *)(&buf[i]);
    BTYPE index = BINDEX(data);
    BTYPE mask = BMASK(data);

    if (unlikely(DirectFilter1[index] & mask)) {
      matches = Progressive_Filtering(PROGRE_PARAMETER);
    }
  }

  /* It is needed to check last 1 byte from payload, because above every 2
   * bytes are used. That means that the last case when there is only 1 byte
   * left is not handled. */
  if (dfc->cDF0[buf[buflen - 1]]) {
    int i;
    for (i = 0; i < dfc->CompactTable1[buf[buflen - 1]].cnt; i++) {
      PID_TYPE pid = dfc->CompactTable1[buf[buflen - 1]].pid[i];
      DFC_PATTERN *mlist = dfc->dfcMatchList[pid];

      ACTION_FOR_MATCH;
    }
  }

  return matches;
}

static void *DFC_REALLOC(void *p, uint16_t n, dfcDataType type,
                         dfcMemoryType type2) {
  switch (type) {
    case DFC_PID_TYPE:
      p = realloc((PID_TYPE *)p, sizeof(PID_TYPE) * n);
      dfc_total_memory += sizeof(PID_TYPE);
      switch (type2) {
        case DFC_MEMORY_TYPE__PATTERN:
          dfc_pattern_memory += sizeof(PID_TYPE);
          break;
        case DFC_MEMORY_TYPE__CT2:
          dfc_memory_ct2 += sizeof(PID_TYPE);
          break;
        case DFC_MEMORY_TYPE__CT3:
          dfc_memory_ct3 += sizeof(PID_TYPE);
          break;
        case DFC_MEMORY_TYPE__CT4:
          dfc_memory_ct4 += sizeof(PID_TYPE);
          break;
        case DFC_MEMORY_TYPE__CT8:
          dfc_memory_ct8 += sizeof(PID_TYPE);
          break;
        default:
          break;
      }
      return p;
    case DFC_CT_Type_2_Array:
      p = realloc((CT_Type_2_Array *)p, sizeof(CT_Type_2_Array) * n);
      dfc_total_memory += sizeof(CT_Type_2_Array);
      switch (type2) {
        case DFC_MEMORY_TYPE__CT2:
          dfc_memory_ct2 += sizeof(CT_Type_2_Array);
          break;
        case DFC_MEMORY_TYPE__CT3:
          dfc_memory_ct3 += sizeof(CT_Type_2_Array);
          break;
        case DFC_MEMORY_TYPE__CT4:
          dfc_memory_ct4 += sizeof(CT_Type_2_Array);
          break;
        case DFC_MEMORY_TYPE__CT8:
          dfc_memory_ct8 += sizeof(CT_Type_2_Array);
          break;
        default:
          break;
      }
      return p;
    case DFC_CT_Type_2_2B_Array:
      p = realloc((CT_Type_2_2B_Array *)p, sizeof(CT_Type_2_2B_Array) * n);
      dfc_total_memory += sizeof(CT_Type_2_2B_Array);
      switch (type2) {
        case DFC_MEMORY_TYPE__CT2:
          dfc_memory_ct2 += sizeof(CT_Type_2_2B_Array);
          break;
        case DFC_MEMORY_TYPE__CT3:
          dfc_memory_ct3 += sizeof(CT_Type_2_2B_Array);
          break;
        case DFC_MEMORY_TYPE__CT4:
          dfc_memory_ct4 += sizeof(CT_Type_2_2B_Array);
          break;
        case DFC_MEMORY_TYPE__CT8:
          dfc_memory_ct8 += sizeof(CT_Type_2_2B_Array);
          break;
        default:
          break;
      }
      return p;
    case DFC_CT_Type_2_8B_Array:
      p = realloc((CT_Type_2_8B_Array *)p, sizeof(CT_Type_2_8B_Array) * n);
      dfc_total_memory += sizeof(CT_Type_2_8B_Array);
      switch (type2) {
        case DFC_MEMORY_TYPE__CT2:
          dfc_memory_ct2 += sizeof(CT_Type_2_8B_Array);
          break;
        case DFC_MEMORY_TYPE__CT3:
          dfc_memory_ct3 += sizeof(CT_Type_2_8B_Array);
          break;
        case DFC_MEMORY_TYPE__CT4:
          dfc_memory_ct4 += sizeof(CT_Type_2_8B_Array);
          break;
        case DFC_MEMORY_TYPE__CT8:
          dfc_memory_ct8 += sizeof(CT_Type_2_8B_Array);
          break;
        default:
          break;
      }
      return p;
    default:
      fprintf(stderr, "ERROR! Data Type is not correct!\n");
      break;
  }
  return NULL;
}

static void *DFC_MALLOC(int n, dfcMemoryType type) {
  void *p = calloc(1, n);  // initialize it to 0

  if (p != NULL) {
    switch (type) {
      case DFC_MEMORY_TYPE__DFC:
        break;
      case DFC_MEMORY_TYPE__PATTERN:
        dfc_pattern_memory += n;
        break;
      case DFC_MEMORY_TYPE__CT2:
        dfc_memory_ct2 += n;
        break;
      case DFC_MEMORY_TYPE__CT3:
        dfc_memory_ct3 += n;
        break;
      case DFC_MEMORY_TYPE__CT4:
        dfc_memory_ct4 += n;
        break;
      case DFC_MEMORY_TYPE__CT8:
        dfc_memory_ct8 += n;
        break;
      case DFC_MEMORY_TYPE__NONE:
        break;
      default:
        printf("%s(%d) Invalid memory type\n", __FILE__, __LINE__);
        break;
    }
    dfc_total_memory += n;
  }
  return p;
}

static void Build_pattern(DFC_PATTERN *p, uint8_t *flag, uint8_t *temp, int j,
                          int k) {
  if (p->is_case_insensitive) {
    if ((p->patrn[j] >= 65 && p->patrn[j] <= 90) ||
        (p->patrn[j] >= 97 && p->patrn[j] <= 122)) {
      if (flag[k] == 0)
        temp[k] = tolower(p->patrn[j]);
      else
        temp[k] = toupper(p->patrn[j]);
    } else {
      temp[k] = p->patrn[j];
    }
  } else {
    temp[k] = p->casepatrn[j];  // original pattern
  }
  return;
}

/**
 * \internal
 * \brief Creates a hash of the pattern.  We use it for the hashing process
 *        during the initial pattern insertion time, to cull duplicate sigs.
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
 * \brief Looks up a pattern.  We use it for the hashing process during the
 *        the initial pattern insertion time, to cull duplicate sigs.
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

static void initializeMemoryTracking() {
  dfc_memory_ct1 = sizeof(CT_Type_1) * CT1_TABLE_SIZE;
  dfc_memory_ct2 = 0;
  dfc_memory_ct3 = 0;
  dfc_memory_ct4 = 0;
  dfc_memory_ct8 = 0;
  dfc_total_memory = sizeof(DFC_STRUCTURE) + dfc_pattern_memory;
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

  dfc->dfcMatchList = (DFC_PATTERN **)DFC_MALLOC(
      sizeof(DFC_PATTERN *) * dfc->numPatterns, DFC_MEMORY_TYPE__PATTERN);
  MEMASSERT_DFC(dfc->dfcMatchList, "setupMatchList");

  for (DFC_PATTERN *plist = dfc->dfcPatterns; plist != NULL;
       plist = plist->next) {
    if (dfc->dfcMatchList[plist->iid] != NULL) {
      fprintf(stderr, "Internal ID ERROR : %u\n", plist->iid);
    }
    dfc->dfcMatchList[plist->iid] = plist;
  }
}

static void setupDirectFilters(DFC_STRUCTURE *dfc) {
  initializeDirectFilterMemory(dfc);

  uint8_t temp[8], flag[8];

  for (DFC_PATTERN *plist = dfc->dfcPatterns; plist != NULL;
       plist = plist->next) {
    if (plist->n == 1) {
      setup1BDirectFilter(dfc, plist);
    }

    if (plist->n >= SMALL_DF_MIN_PATTERN_SIZE &&
        plist->n <= SMALL_DF_MAX_PATTERN_SIZE) {
      addPatternToSmallDirectFilter(dfc, plist);
    } else {
      addPatternToLargeDirectFilter(dfc, plist);
    }

    /* 1. Initialization for DF1 */
    if (plist->n > 1) {
      setup1BPlusDirectFilter(dfc, plist);
    }

    /* Initializing 4B DF, 8B DF */
    if (plist->n >= 4) {
      uint32_t alpha_cnt = 0;

      do {
        for (int j = 3, k = 0; j >= 0; --j, k++) {
          flag[k] = (alpha_cnt >> j) & 1;
        }

        if (plist->n < 8) {
          for (int j = plist->n - 4, k = 0; j < plist->n; j++, k++) {
            Build_pattern(plist, flag, temp, j, k);
          }
        } else {
          for (int j = min_pattern_interval * (plist->n - 8) / pattern_interval,
                   k = 0;
               j < min_pattern_interval * (plist->n - 8) / pattern_interval + 4;
               j++, k++) {
            Build_pattern(plist, flag, temp, j, k);
          }
        }

        // the last 2 bytes
        uint32_t byteIndex = BINDEX((*(((uint16_t *)temp) + 1)) & DF_MASK);
        uint32_t bitMask = BMASK((*(((uint16_t *)temp) + 1)) & DF_MASK);

        dfc->ADD_DF_4_plus[byteIndex] |= bitMask;
        if (plist->n >= 4 && plist->n < 8) {
          dfc->ADD_DF_4_1[byteIndex] |= bitMask;

          // the first 2 bytes
          uint32_t fragment_16 = (temp[1] << 8) | temp[0];
          uint32_t byteIndex = BINDEX(fragment_16 & DF_MASK);
          uint32_t bitMask = BMASK(fragment_16 & DF_MASK);

          dfc->cDF2[byteIndex] |= bitMask;
        }
        alpha_cnt++;
      } while (alpha_cnt < 16);
    }

    if (plist->n >= 8) {
      uint32_t alpha_cnt = 0;

      do {
        for (int j = 7, k = 0; j >= 0; --j, k++) {
          flag[k] = (alpha_cnt >> j) & 1;
        }

        for (int j = min_pattern_interval * (plist->n - 8) / pattern_interval,
                 k = 0;
             j < min_pattern_interval * (plist->n - 8) / pattern_interval + 8;
             j++, k++) {
          Build_pattern(plist, flag, temp, j, k);
        }

        uint32_t byteIndex = BINDEX((*(((uint16_t *)temp) + 3)) & DF_MASK);
        uint32_t bitMask = BMASK((*(((uint16_t *)temp) + 3)) & DF_MASK);

        dfc->ADD_DF_8_1[byteIndex] |= bitMask;

        byteIndex = BINDEX((*(((uint16_t *)temp) + 2)) & DF_MASK);
        bitMask = BMASK((*(((uint16_t *)temp) + 2)) & DF_MASK);

        dfc->ADD_DF_8_2[byteIndex] |= bitMask;

        alpha_cnt++;
      } while (alpha_cnt < 256);
    }
  }
}

static void setup1BDirectFilter(DFC_STRUCTURE *dfc, DFC_PATTERN *plist) {
  uint8_t pattern = plist->casepatrn[0];
  setup1BDirectFilterWithPattern(dfc, pattern);

  if (plist->is_case_insensitive) {
    setup1BDirectFilterWithPattern(dfc, toggleCharacterCase(pattern));
  }
}

static void setup1BDirectFilterWithPattern(DFC_STRUCTURE *dfc,
                                           uint8_t pattern) {
  dfc->cDF0[pattern] = 1;
  for (int j = 0; j < 256; j++) {
    uint16_t fragment_16 = (j << 8) | pattern;
    uint32_t byteIndex = (uint32_t)BINDEX(fragment_16 & DF_MASK);
    uint32_t bitMask = BMASK(fragment_16 & DF_MASK);

    dfc->DirectFilter1[byteIndex] |= bitMask;
  }
}

static void setup1BPlusDirectFilter(DFC_STRUCTURE *dfc, DFC_PATTERN *plist) {
  uint8_t temp[8], flag[8];
  uint32_t alpha_cnt = 0;

  do {
    for (int j = 1, k = 0; j >= 0; --j, k++) {
      flag[k] = (alpha_cnt >> j) & 1;
    }

    if (plist->n == 2 || plist->n == 3) {
      for (int j = plist->n - 2, k = 0; j < plist->n; j++, k++) {
        Build_pattern(plist, flag, temp, j, k);
      }
    } else if (plist->n < 8) {
      for (int j = plist->n - 4, k = 0; j < plist->n - 2; j++, k++) {
        Build_pattern(plist, flag, temp, j, k);
      }
    } else {  // len >= 8
      for (int j = min_pattern_interval * (plist->n - 8) / pattern_interval,
               k = 0;
           j < min_pattern_interval * (plist->n - 8) / pattern_interval + 2;
           j++, k++) {
        Build_pattern(plist, flag, temp, j, k);
      }
    }

    uint16_t fragment_16 = (temp[1] << 8) | temp[0];
    uint32_t byteIndex = (uint32_t)BINDEX(fragment_16 & DF_MASK);
    uint32_t bitMask = BMASK(fragment_16 & DF_MASK);

    dfc->DirectFilter1[byteIndex] |= bitMask;

    if (plist->n == 2 || plist->n == 3) dfc->cDF1[byteIndex] |= bitMask;

    alpha_cnt++;
  } while (alpha_cnt < 4);
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
  uint16_t fragment_16 = (pattern[0] << 8) | pattern[1];
  uint32_t byteIndex = (uint32_t)BINDEX(fragment_16 & DF_MASK);
  uint32_t bitMask = BMASK(fragment_16 & DF_MASK);

  df[byteIndex] |= bitMask;
}

static void add1BPatternToSmallDirectFilter(DFC_STRUCTURE *dfc,
                                            uint8_t pattern) {
  uint8_t newPattern[2];
  newPattern[1] = pattern;
  for (int j = 0; j < 256; j++) {
    newPattern[0] = j;

    maskPatternIntoDirectFilter(dfc->directFilterSmall, newPattern);
  }
}

static void addPatternToDirectFilter(uint8_t *directFilter,
                                     bool isCaseInsensitive, uint8_t *pattern,
                                     int patternLength) {
  if (isCaseInsensitive) {
    int permutationCount = 2 << patternLength;
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
  assert(pattern->n > 3);

  uint8_t *lastCharactersOfPattern = pattern->casepatrn + ((pattern->n - 2));
  addPatternToDirectFilter(dfc->directFilterLarge, pattern->is_case_insensitive,
                           lastCharactersOfPattern, 2);
}

static void initializeDirectFilterMemory(DFC_STRUCTURE *dfc) {
  memset(dfc->CompactTable1, 0, sizeof(CT_Type_1) * CT1_TABLE_SIZE);

  for (int i = 0; i < DF_SIZE_REAL; i++) {
    dfc->DirectFilter1[i] = 0;
    dfc->ADD_DF_4_plus[i] = 0;
    dfc->ADD_DF_8_1[i] = 0;
    dfc->ADD_DF_4_1[i] = 0;
    dfc->cDF2[i] = 0;
    dfc->ADD_DF_8_2[i] = 0;
    dfc->cDF1[i] = 0;
  }

  for (int i = 0; i < 256; i++) {
    dfc->cDF0[i] = 0;
  }

  initializeSmallDirectFilterMemory(dfc);
}

static void initializeSmallDirectFilterMemory(DFC_STRUCTURE *dfc) {
  for (int i = 0; i < DIRECT_FILTER_SIZE_SMALL; ++i) {
    dfc->directFilterSmall[i] = 0;
  }
}

static void initializeCompactTableMemory(DFC_STRUCTURE *dfc) {
  dfc_memory_ct2 += sizeof(CT_Type_2) * CT2_TABLE_SIZE;
  memset(dfc->CompactTable2, 0, sizeof(CT_Type_2) * CT2_TABLE_SIZE);

  dfc_memory_ct4 += sizeof(CT_Type_2) * CT4_TABLE_SIZE;
  memset(dfc->CompactTable4, 0, sizeof(CT_Type_2) * CT4_TABLE_SIZE);

  dfc_memory_ct8 += sizeof(CT_Type_2_8B) * CT8_TABLE_SIZE;
  memset(dfc->CompactTable8, 0, sizeof(CT_Type_2_8B) * CT8_TABLE_SIZE);
}

static void getEmptyOrEqualSmallCompactTableEntry(
    uint8_t pattern, CompactTableSmallEntry *entry) {
  int entryCount = 0;
  while (entry->pidCount && entry->pattern != pattern &&
         entryCount < MAX_ENTRIES_PER_BUCKET) {
    entry += sizeof(CompactTableSmallEntry);
    ++entryCount;
  }

  if (entryCount == MAX_ENTRIES_PER_BUCKET) {
    fprintf(stderr,
            "Too many entries with the same hash in the small compact table."
            "Please increase MAX_ENTRIES_PER_BUCKET or update the hash "
            "function");
    exit(TOO_MANY_ENTRIES_IN_SMALL_CT);
  }

  if (entry->pidCount == MAX_PID_PER_ENTRY) {
    fprintf(stderr,
            "Too many equal patterns in the small compact hash table."
            "Please increase MAX_PID_PER_ENTRY, MAX_ENTRIES_PER_BUCKET or "
            "update the hash function");
    exit(TOO_MANY_PID_IN_SMALL_CT);
  }
}

static void pushPatternToSmallCompactTable(DFC_STRUCTURE *dfc, uint8_t pattern,
                                           PID_TYPE pid) {
  CompactTableSmallEntry *entry = dfc->compactTableSmall[pattern].entries;

  getEmptyOrEqualSmallCompactTableEntry(pattern, entry);

  entry->pattern = pattern;
  entry->pids[entry->pidCount] = pid;
  ++entry->pidCount;
}

static void addPatternToSmallCompactTable(DFC_STRUCTURE *dfc,
                                          DFC_PATTERN *pattern) {
  assert(pattern->n >= SMALL_DF_MIN_PATTERN_SIZE);
  assert(pattern->n <= SMALL_DF_MAX_PATTERN_SIZE);

  uint8_t lastCharacterOfPattern = pattern->casepatrn[pattern->n - 1];
  pushPatternToSmallCompactTable(dfc, lastCharacterOfPattern, pattern->iid);

  if (pattern->is_case_insensitive) {
    pushPatternToSmallCompactTable(
        dfc, toggleCharacterCase(lastCharacterOfPattern), pattern->iid);
  }
}

static void setupCompactTables(DFC_STRUCTURE *dfc) {
  initializeCompactTableMemory(dfc);

  uint8_t temp[8], flag[8];

  for (DFC_PATTERN *plist = dfc->dfcPatterns; plist != NULL;
       plist = plist->next) {
    if (plist->n >= SMALL_DF_MIN_PATTERN_SIZE &&
        plist->n <= SMALL_DF_MAX_PATTERN_SIZE) {
      addPatternToSmallCompactTable(dfc, plist);
    }

    if (plist->n == 1) {
      uint8_t pattern = plist->casepatrn[0];

      if (dfc->CompactTable1[pattern].cnt == 0) {
        dfc->CompactTable1[pattern].cnt++;
        dfc->CompactTable1[pattern].pid[0] = plist->iid;
      } else {
        int k;
        for (k = 0; k < dfc->CompactTable1[pattern].cnt; k++) {
          if (dfc->CompactTable1[pattern].pid[k] == plist->iid) break;
        }
        if (k == dfc->CompactTable1[pattern].cnt) {
          dfc->CompactTable1[pattern].pid[dfc->CompactTable1[pattern].cnt++] =
              plist->iid;
          if (dfc->CompactTable1[pattern].cnt >= CT_TYPE1_PID_CNT_MAX)
            printf("Too many PIDs in CT1. You should expand the size.\n");
        }
      }

      if (plist->is_case_insensitive) {
        pattern = toggleCharacterCase(pattern);

        if (dfc->CompactTable1[pattern].cnt == 0) {
          dfc->CompactTable1[pattern].cnt++;
          dfc->CompactTable1[pattern].pid[0] = plist->iid;
        } else {
          int k;
          for (k = 0; k < dfc->CompactTable1[pattern].cnt; k++) {
            if (dfc->CompactTable1[pattern].pid[k] == plist->iid) break;
          }
          if (k == dfc->CompactTable1[pattern].cnt) {
            dfc->CompactTable1[pattern].pid[dfc->CompactTable1[pattern].cnt++] =
                plist->iid;
            if (dfc->CompactTable1[pattern].cnt >= CT_TYPE1_PID_CNT_MAX)
              printf("Too many PIDs in CT1. You should expand the size.\n");
          }
        }
      }
    }

    if (plist->n == 2 || plist->n == 3) {
      uint32_t alpha_cnt = 0;

      do {
        for (int j = 1, k = 0; j >= 0; --j, k++) {
          flag[k] = (alpha_cnt >> j) & 1;
        }

        for (int j = plist->n - 2, k = 0; j < plist->n; j++, k++) {
          Build_pattern(plist, flag, temp, j, k);
        }

        // 2.
        uint16_t fragment_16 = (temp[1] << 8) | temp[0];
        uint32_t crc = _mm_crc32_u16(0, fragment_16);

        // 3.
        crc &= CT2_TABLE_SIZE_MASK;

        // 4.
        if (dfc->CompactTable2[crc].cnt != 0) {
          BUC_CNT_TYPE n;
          for (n = 0; n < dfc->CompactTable2[crc].cnt; n++) {
            if (dfc->CompactTable2[crc].array[n].pat == fragment_16) break;
          }

          if (n == dfc->CompactTable2[crc].cnt) {  // If not found,
            dfc->CompactTable2[crc].cnt++;
            dfc->CompactTable2[crc].array = (CT_Type_2_Array *)DFC_REALLOC(
                (void *)dfc->CompactTable2[crc].array,
                dfc->CompactTable2[crc].cnt, DFC_CT_Type_2_Array,
                DFC_MEMORY_TYPE__CT2);
            dfc->CompactTable2[crc].array[dfc->CompactTable2[crc].cnt - 1].pat =
                fragment_16;

            dfc->CompactTable2[crc].array[dfc->CompactTable2[crc].cnt - 1].cnt =
                1;
            dfc->CompactTable2[crc].array[dfc->CompactTable2[crc].cnt - 1].pid =
                (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__CT2);
            dfc->CompactTable2[crc]
                .array[dfc->CompactTable2[crc].cnt - 1]
                .pid[0] = plist->iid;
          } else {  // If found,
            BUC_CNT_TYPE m;
            for (m = 0; m < dfc->CompactTable2[crc].array[n].cnt; m++) {
              if (dfc->CompactTable2[crc].array[n].pid[m] == plist->iid) break;
            }
            if (m == dfc->CompactTable2[crc].array[n].cnt) {
              dfc->CompactTable2[crc].array[n].cnt++;
              dfc->CompactTable2[crc].array[n].pid = (PID_TYPE *)DFC_REALLOC(
                  (void *)dfc->CompactTable2[crc].array[n].pid,
                  dfc->CompactTable2[crc].array[n].cnt, DFC_PID_TYPE,
                  DFC_MEMORY_TYPE__CT2);
              dfc->CompactTable2[crc]
                  .array[n]
                  .pid[dfc->CompactTable2[crc].array[n].cnt - 1] = plist->iid;
            }
          }
        } else {  // If there is no elements in the CT4,
          dfc->CompactTable2[crc].cnt = 1;
          dfc->CompactTable2[crc].array = (CT_Type_2_Array *)DFC_MALLOC(
              sizeof(CT_Type_2_Array), DFC_MEMORY_TYPE__CT2);
          memset(dfc->CompactTable2[crc].array, 0, sizeof(CT_Type_2_Array));

          dfc->CompactTable2[crc].array[0].pat = fragment_16;
          dfc->CompactTable2[crc].array[0].cnt = 1;
          dfc->CompactTable2[crc].array[0].pid =
              (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__CT2);
          dfc->CompactTable2[crc].array[0].pid[0] = plist->iid;
        }

        alpha_cnt++;
      } while (alpha_cnt < 4);
    }

    /* CT4 initialization */
    if (plist->n >= 4 && plist->n < 8) {
      uint32_t alpha_cnt = 0;
      do {
        // 1.

        for (int j = 3, k = 0; j >= 0; --j, k++) {
          flag[k] = (alpha_cnt >> j) & 1;
        }

        for (int j = plist->n - 4, k = 0; j < plist->n; j++, k++) {
          Build_pattern(plist, flag, temp, j, k);
        }

        // 2.
        uint32_t fragment_32 =
            (temp[3] << 24) | (temp[2] << 16) | (temp[1] << 8) | temp[0];
        uint32_t crc = _mm_crc32_u32(0, fragment_32);

        // 3.
        crc &= CT4_TABLE_SIZE_MASK;

        // 4.
        if (dfc->CompactTable4[crc].cnt != 0) {
          BUC_CNT_TYPE n;
          for (n = 0; n < dfc->CompactTable4[crc].cnt; n++) {
            if (dfc->CompactTable4[crc].array[n].pat == fragment_32) break;
          }

          if (n == dfc->CompactTable4[crc].cnt) {  // If not found,
            dfc->CompactTable4[crc].cnt++;
            dfc->CompactTable4[crc].array = (CT_Type_2_Array *)DFC_REALLOC(
                (void *)dfc->CompactTable4[crc].array,
                dfc->CompactTable4[crc].cnt, DFC_CT_Type_2_Array,
                DFC_MEMORY_TYPE__CT4);
            dfc->CompactTable4[crc].array[dfc->CompactTable4[crc].cnt - 1].pat =
                fragment_32;

            dfc->CompactTable4[crc].array[dfc->CompactTable4[crc].cnt - 1].cnt =
                1;
            dfc->CompactTable4[crc].array[dfc->CompactTable4[crc].cnt - 1].pid =
                (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__CT4);
            dfc->CompactTable4[crc]
                .array[dfc->CompactTable4[crc].cnt - 1]
                .pid[0] = plist->iid;
          } else {  // If found,
            BUC_CNT_TYPE m;
            for (m = 0; m < dfc->CompactTable4[crc].array[n].cnt; m++) {
              if (dfc->CompactTable4[crc].array[n].pid[m] == plist->iid) break;
            }
            if (m == dfc->CompactTable4[crc].array[n].cnt) {
              dfc->CompactTable4[crc].array[n].cnt++;
              dfc->CompactTable4[crc].array[n].pid = (PID_TYPE *)DFC_REALLOC(
                  (void *)dfc->CompactTable4[crc].array[n].pid,
                  dfc->CompactTable4[crc].array[n].cnt, DFC_PID_TYPE,
                  DFC_MEMORY_TYPE__CT4);
              dfc->CompactTable4[crc]
                  .array[n]
                  .pid[dfc->CompactTable4[crc].array[n].cnt - 1] = plist->iid;
            }
          }
        } else {  // If there is no elements in the CT4,
          dfc->CompactTable4[crc].cnt = 1;
          dfc->CompactTable4[crc].array = (CT_Type_2_Array *)DFC_MALLOC(
              sizeof(CT_Type_2_Array), DFC_MEMORY_TYPE__CT4);
          memset(dfc->CompactTable4[crc].array, 0, sizeof(CT_Type_2_Array));

          dfc->CompactTable4[crc].array[0].pat = fragment_32;
          dfc->CompactTable4[crc].array[0].cnt = 1;
          dfc->CompactTable4[crc].array[0].pid =
              (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__CT4);
          dfc->CompactTable4[crc].array[0].pid[0] = plist->iid;
        }
        alpha_cnt++;
      } while (alpha_cnt < 16);
    }

    /* CT8 initialization */
    if (plist->n >= 8) {
      uint32_t alpha_cnt = 0;
      do {
        for (int j = 7, k = 0; j >= 0; --j, k++) {
          flag[k] = (alpha_cnt >> j) & 1;
        }

#ifdef CT8_SWITCH
        for (int j = min_pattern_interval * (plist->n - 8) / pattern_interval,
                 k = 0;
             j < min_pattern_interval * (plist->n - 8) / pattern_interval + 8;
             j++, k++) {
          temp[k] = plist->patrn[j];
        }
#else
        for (int j = plist->n - 8, k = 0; j < plist->n; j++, k++) {
          Build_pattern(plist, flag, temp, j, k);
        }
#endif

        // 1. Calulating Indice
        uint32_t fragment_32 =
            (temp[7] << 24) | (temp[6] << 16) | (temp[5] << 8) | temp[4];
        uint64_t fragment_64 = ((uint64_t)fragment_32 << 32) | (temp[3] << 24) |
                               (temp[2] << 16) | (temp[1] << 8) | temp[0];
        uint64_t crc = _mm_crc32_u64(0, fragment_64);
        crc &= CT8_TABLE_SIZE_MASK;

        if (dfc->CompactTable8[crc].cnt != 0) {
          BUC_CNT_TYPE n;
          for (n = 0; n < dfc->CompactTable8[crc].cnt; n++) {
            if (dfc->CompactTable8[crc].array[n].pat == fragment_64) break;
          }

          if (n == dfc->CompactTable8[crc].cnt) {  // If not found,
            dfc->CompactTable8[crc].cnt++;
            dfc->CompactTable8[crc].array = (CT_Type_2_8B_Array *)DFC_REALLOC(
                (void *)dfc->CompactTable8[crc].array,
                dfc->CompactTable8[crc].cnt, DFC_CT_Type_2_8B_Array,
                DFC_MEMORY_TYPE__CT8);
            dfc->CompactTable8[crc].array[dfc->CompactTable8[crc].cnt - 1].pat =
                fragment_64;

            dfc->CompactTable8[crc].array[dfc->CompactTable8[crc].cnt - 1].cnt =
                1;
            dfc->CompactTable8[crc].array[dfc->CompactTable8[crc].cnt - 1].pid =
                (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__CT8);
            dfc->CompactTable8[crc]
                .array[dfc->CompactTable8[crc].cnt - 1]
                .pid[0] = plist->iid;
          } else {  // If found,
            BUC_CNT_TYPE m;
            for (m = 0; m < dfc->CompactTable8[crc].array[n].cnt; m++) {
              if (dfc->CompactTable8[crc].array[n].pid[m] == plist->iid) break;
            }
            if (m == dfc->CompactTable8[crc].array[n].cnt) {
              dfc->CompactTable8[crc].array[n].cnt++;
              dfc->CompactTable8[crc].array[n].pid = (PID_TYPE *)DFC_REALLOC(
                  (void *)dfc->CompactTable8[crc].array[n].pid,
                  dfc->CompactTable8[crc].array[n].cnt, DFC_PID_TYPE,
                  DFC_MEMORY_TYPE__CT8);
              dfc->CompactTable8[crc]
                  .array[n]
                  .pid[dfc->CompactTable8[crc].array[n].cnt - 1] = plist->iid;
            }
          }
        } else {  // If there is no elements in the CT8,
          dfc->CompactTable8[crc].cnt = 1;
          dfc->CompactTable8[crc].array = (CT_Type_2_8B_Array *)DFC_MALLOC(
              sizeof(CT_Type_2_8B_Array), DFC_MEMORY_TYPE__CT8);
          memset(dfc->CompactTable8[crc].array, 0, sizeof(CT_Type_2_8B_Array));

          dfc->CompactTable8[crc].array[0].pat = fragment_64;
          dfc->CompactTable8[crc].array[0].cnt = 1;
          dfc->CompactTable8[crc].array[0].pid =
              (PID_TYPE *)DFC_MALLOC(sizeof(PID_TYPE), DFC_MEMORY_TYPE__CT8);
          dfc->CompactTable8[crc].array[0].pid[0] = plist->iid;
        }
        alpha_cnt++;
      } while (alpha_cnt < 256);
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
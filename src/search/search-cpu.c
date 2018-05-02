#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "memory.h"
#include "search.h"
#include "shared-functions.h"
#include "utility.h"

static int my_strncmp(unsigned char *a, unsigned char *b, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if (a[i] != b[i]) return -1;
  }
  return 0;
}

unsigned char my_tolower(unsigned char c) {
  if (c >= 65 && c <= 90) {
    return c + 32;
  }
  return c;
}

static int my_strncasecmp(unsigned char *a, unsigned char *b, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if (my_tolower(a[i]) != my_tolower(b[i])) return -1;
  }
  return 0;
}

static bool doesPatternMatch(uint8_t *start, uint8_t *pattern, int length,
                             bool isCaseInsensitive) {
  if (isCaseInsensitive) {
    return !my_strncasecmp(start, pattern, length);
  }
  return !my_strncmp(start, pattern, length);
}

static void verifySmall(CompactTableSmallEntry *ct, PID_TYPE *pids,
                        DFC_FIXED_PATTERN *patterns, uint8_t *input,
                        int currentPos, int inputLength, VerifyResult *result) {
  uint8_t hash = input[0];

  int offset = (ct + hash)->offset;
  pids += offset;

  for (int i = 0; i < (ct + hash)->pidCount; ++i) {
    PID_TYPE pid = pids[i];

    int patternLength = (patterns + pid)->pattern_length;

    if (inputLength - currentPos >= patternLength &&
        doesPatternMatch(input, (patterns + pid)->original_pattern,
                         patternLength,
                         (patterns + pid)->is_case_insensitive)) {
      result->matchesSmallCt[result->matchCountSmallCt] = pid;
      ++result->matchCountSmallCt;
    }
  }
}

static void verifyLarge(CompactTableLarge *ct, DFC_FIXED_PATTERN *patterns,
                        uint8_t *input, int currentPos, int inputLength,
                        VerifyResult *result) {
  uint32_t bytePattern =
      input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  uint32_t hash = hashForLargeCompactTable(bytePattern);

  uint8_t multiplier = 0;
  for (; GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++multiplier) {
    if (GET_ENTRY_LARGE_CT(hash, multiplier)->pattern == bytePattern) {
      for (int i = 0; i < GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++i) {
        PID_TYPE pid = GET_ENTRY_LARGE_CT(hash, multiplier)->pids[i];

        int patternLength = (patterns + pid)->pattern_length;
        if (inputLength - currentPos >= patternLength) {
          if (doesPatternMatch(input, (patterns + pid)->original_pattern,
                               patternLength,
                               (patterns + pid)->is_case_insensitive)) {
            result->matchesLargeCt[result->matchCountLargeCt] = pid;
            ++result->matchCountLargeCt;
          }
        }
      }

      break;
    }
  }
}

static bool isInHashDf(uint8_t *df, uint8_t *input) {
  /*
   the last two bytes are used to match,
   hence we are now at least 2 bytes into the pattern
   */
  uint32_t data = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  uint16_t byteIndex = directFilterHash(data);
  uint16_t bitMask = BMASK(data & DF_MASK);

  return df[byteIndex] & bitMask;
}

int searchCpuEmulateGpu(MatchFunction onMatch) {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;
  DFC_PATTERNS *patterns = dfc->patterns;
  uint8_t *input = (uint8_t *)DFC_HOST_MEMORY.input;
  int inputLength = DFC_HOST_MEMORY.inputLength;

  VerifyResult *result = malloc(sizeInBytesOfResultVector(inputLength));
  if (!result) {
    fprintf(stderr, "Could not allocate result vector\n");
    exit(1);
  }

  for (int i = 0; i < inputLength - 1; ++i) {
    int16_t data = input[i + 1] << 8 | input[i];
    int16_t byteIndex = BINDEX(data & DF_MASK);
    int16_t bitMask = BMASK(data & DF_MASK);

    result[i].matchCountSmallCt = 0;
    result[i].matchCountLargeCt = 0;

    if (dfc->directFilterSmall[byteIndex] & bitMask) {
      verifySmall(dfc->ctSmallEntries, dfc->ctSmallPids, patterns->dfcMatchList,
                  input + i, i, inputLength, result + i);
    }

    if (i < inputLength - 3 && (dfc->directFilterLarge[byteIndex] & bitMask) &&
        isInHashDf(dfc->directFilterLargeHash, input + i)) {
      verifyLarge(dfc->compactTableLarge, patterns->dfcMatchList, input + i, i,
                  inputLength, result + i);
    }
  }

  int matches = 0;
  for (int i = 0; i < inputLength - 1; ++i) {
    VerifyResult *res = &result[i];

    for (int j = 0; j < res->matchCountSmallCt; ++j) {
      onMatch(&patterns->dfcMatchList[res->matchesSmallCt[j]]);
      ++matches;
    }

    for (int j = 0; j < res->matchCountLargeCt; ++j) {
      onMatch(&patterns->dfcMatchList[res->matchesLargeCt[j]]);
      ++matches;
    }
  }

  free(result);

  return matches;
}

static int verifySmallRet(CompactTableSmallEntry *ct, PID_TYPE *pids,
                          DFC_FIXED_PATTERN *patterns, uint8_t *input,
                          int currentPos, int inputLength,
                          MatchFunction onMatch) {
  uint8_t hash = input[0];

  int offset = (ct + hash)->offset;
  pids += offset;

  int matches = 0;
  for (int i = 0; i < (ct + hash)->pidCount; ++i) {
    PID_TYPE pid = pids[i];

    int patternLength = (patterns + pid)->pattern_length;

    if (inputLength - currentPos >= patternLength &&
        doesPatternMatch(input, (patterns + pid)->original_pattern,
                         patternLength,
                         (patterns + pid)->is_case_insensitive)) {
      onMatch(&patterns[pid]);
      ++matches;
    }
  }

  return matches;
}

static int verifyLargeRet(CompactTableLarge *ct, DFC_FIXED_PATTERN *patterns,
                          uint8_t *input, int currentPos, int inputLength,
                          MatchFunction onMatch) {
  uint32_t bytePattern =
      input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  uint32_t hash = hashForLargeCompactTable(bytePattern);

  int matches = 0;
  uint8_t multiplier = 0;
  for (; GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++multiplier) {
    if (GET_ENTRY_LARGE_CT(hash, multiplier)->pattern == bytePattern) {
      for (int i = 0; i < GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++i) {
        PID_TYPE pid = GET_ENTRY_LARGE_CT(hash, multiplier)->pids[i];

        int patternLength = (patterns + pid)->pattern_length;
        if (inputLength - currentPos >= patternLength &&
            doesPatternMatch(input, (patterns + pid)->original_pattern,
                             patternLength,
                             (patterns + pid)->is_case_insensitive)) {
          onMatch(&patterns[pid]);
          ++matches;
        }
      }
    }
  }

  return matches;
}

int searchCpu(MatchFunction onMatch) {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;
  DFC_PATTERNS *patterns = dfc->patterns;
  uint8_t *input = (uint8_t *)DFC_HOST_MEMORY.input;
  int inputLength = DFC_HOST_MEMORY.inputLength;

  int matches = 0;
  for (int i = 0; i < inputLength - 1; ++i) {
    int16_t data = input[i + 1] << 8 | input[i];
    int16_t byteIndex = BINDEX(data & DF_MASK);
    int16_t bitMask = BMASK(data & DF_MASK);

    if (dfc->directFilterSmall[byteIndex] & bitMask) {
      matches += verifySmallRet(dfc->ctSmallEntries, dfc->ctSmallPids,
                                patterns->dfcMatchList, input + i, i,
                                inputLength, onMatch);
    }

    if (i < inputLength - 3 && (dfc->directFilterLarge[byteIndex] & bitMask) &&
        isInHashDf(dfc->directFilterLargeHash, input + i)) {
      matches += verifyLargeRet(dfc->compactTableLarge, patterns->dfcMatchList,
                                input + i, i, inputLength, onMatch);
    }
  }

  return matches;
}

int exactMatchingUponFiltering(uint8_t *result, int length,
                               DFC_PATTERNS *patterns, MatchFunction onMatch) {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;
  uint8_t *input = (uint8_t *)DFC_HOST_MEMORY.input;

  int matches = 0;

  for (int i = 0; i < length - 1; ++i) {
    if (result[i] & 0x01) {
      matches +=
          verifySmallRet(dfc->ctSmallEntries, dfc->ctSmallPids,
                         patterns->dfcMatchList, input + i, i, length, onMatch);
    }

    if (result[i] & 0x02) {
      matches += verifyLargeRet(dfc->compactTableLarge, patterns->dfcMatchList,
                                input + i, i, length, onMatch);
    }
  }

  return matches;
}
#ifndef DFC_SEARCH_CPU_H
#define DFC_SEARCH_CPU_H

#include <ctype.h>
#include <stdint.h>

#include "dfc.h"
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

static int verifySmall(CompactTableSmallEntry *ct, DFC_FIXED_PATTERN *patterns,
                       uint8_t *input, int remainingCharacters) {
  uint8_t hash = input[0];
  int matches = 0;
  for (int i = 0; i < (ct + hash)->pidCount; ++i) {
    PID_TYPE pid = (ct + hash)->pids[i];

    int patternLength = (patterns + pid)->pattern_length;
    if (remainingCharacters >= patternLength) {
      matches +=
          doesPatternMatch(patternLength > 2 ? input - 1 : input,
                           (patterns + pid)->original_pattern, patternLength,
                           (patterns + pid)->is_case_insensitive);
    }
  }

  return matches;
}

static int verifyLarge(DFC_STRUCTURE *dfc, uint8_t *input, int currentPos,
                       int inputLength) {
  uint32_t bytePattern = *(input + 1) << 24 | *(input + 0) << 16 |
                         *(input - 1) << 8 | *(input - 2);
  uint32_t hash = hashForLargeCompactTable(bytePattern);
  CompactTableLargeEntry *entry = dfc->compactTableLarge[hash].entries;

  int matches = 0;
  for (; entry->pidCount; entry += sizeof(CompactTableLargeEntry)) {
    if (entry->pattern == bytePattern) {
      for (int i = 0; i < entry->pidCount; ++i) {
        PID_TYPE pid = entry->pids[i];

        DFC_FIXED_PATTERN *pattern = &dfc->dfcMatchList[pid];
        int patternLength = pattern->pattern_length;
        if (currentPos >= patternLength - 2 && inputLength - currentPos > 1) {
          uint8_t *start = input - (patternLength - 2);
          matches +=
              doesPatternMatch(start, pattern->original_pattern, patternLength,
                               pattern->is_case_insensitive);
        }
      }
    }
  }

  return matches;
}

int search(DFC_STRUCTURE *dfc, uint8_t *input, int inputLength) {
  int matches = 0;

  for (int i = 0; i < inputLength; ++i) {
    int16_t data = *(input + i + 1) << 8 | *(input + i);
    int16_t byteIndex = BINDEX(data & DF_MASK);
    int16_t bitMask = BMASK(data & DF_MASK);

    if (dfc->directFilterSmall[byteIndex] & bitMask) {
      matches += verifySmall(dfc->compactTableSmall, dfc->dfcMatchList,
                             input + i, inputLength - i + 1);
    }

    if (dfc->directFilterLarge[byteIndex] & bitMask) {
      matches += verifyLarge(dfc, input + i, i, inputLength);
    }
  }

  return matches;
}

#endif
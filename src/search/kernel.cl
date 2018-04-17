#include "shared.h"

ushort directFilterHash(uint val) { return BINDEX((val * 8387) & DF_MASK); }
uint hashForLargeCompactTable(uint input) {
  return (input * 8389) & (COMPACT_TABLE_SIZE_LARGE - 1);
}

int my_strncmp(__global unsigned char *a, __global unsigned char *b, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if (a[i] != b[i]) return -1;
  }
  return 0;
}

uchar tolower(uchar c) {
  if (c >= 65 && c <= 90) {
    return c + 32;
  }
  return c;
}

int my_strncasecmp(__global unsigned char *a, __global unsigned char *b,
                   int n) {
  int i;
  for (i = 0; i < n; i++) {
    if (tolower(a[i]) != tolower(b[i])) return -1;
  }
  return 0;
}

bool doesPatternMatch(__global uchar *start, __global uchar *pattern,
                      int length, bool isCaseInsensitive) {
  if (isCaseInsensitive) {
    return !my_strncasecmp(start, pattern, length);
  }
  return !my_strncmp(start, pattern, length);
}

int verifySmall(__global CompactTableSmallEntry *ct,
                __global DFC_FIXED_PATTERN *patterns, __global uchar *input,
                int currentPos, int inputLength) {
  uchar hash = input[0];
  int matches = 0;
  for (int i = 0; i < (ct + hash)->pidCount; ++i) {
    PID_TYPE pid = (ct + hash)->pids[i];

    int patternLength = (patterns + pid)->pattern_length;
    if (patternLength == 3) {
      --input;
      --currentPos;
    }

    if (currentPos >= 0 && inputLength - currentPos >= patternLength) {
      matches += doesPatternMatch(input, (patterns + pid)->original_pattern,
                                  patternLength,
                                  (patterns + pid)->is_case_insensitive);
    }
  }

  return matches;
}

int verifyLarge(__global CompactTableLarge *ct,
                __global DFC_FIXED_PATTERN *patterns, __global uchar *input,
                int currentPos, int inputLength) {
  /*
   the last two bytes are used to match,
   hence we are now at least 2 bytes into the pattern
   */
  input -= 2;
  currentPos -= 2;
  uint bytePattern = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  uint hash = hashForLargeCompactTable(bytePattern);

  int matches = 0;
  uchar multiplier = 0;
  for (; GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++multiplier) {
    if (GET_ENTRY_LARGE_CT(hash, multiplier)->pattern == bytePattern) {
      for (int i = 0; i < GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++i) {
        PID_TYPE pid = GET_ENTRY_LARGE_CT(hash, multiplier)->pids[i];

        int patternLength = (patterns + pid)->pattern_length;
        int startOfRelativeInput = currentPos - (patternLength - 4);

        if (startOfRelativeInput >= 0 &&
            inputLength - startOfRelativeInput >= patternLength) {
          matches += doesPatternMatch(
              input - (patternLength - 4), (patterns + pid)->original_pattern,
              patternLength, (patterns + pid)->is_case_insensitive);
        }
      }
    }
  }

  return matches;
}

bool isInHashDf(__global uchar *df, __global uchar *input) {
  /*
   the last two bytes are used to match,
   hence we are now at least 2 bytes into the pattern
   */
  input -= 2;

  uint data = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  ushort byteIndex = directFilterHash(data);
  ushort bitMask = BMASK(data & DF_MASK);

  return df[byteIndex] & bitMask;
}

__kernel void search(int inputLength, __global uchar *input,
                     __global DFC_FIXED_PATTERN *patterns,
                     __global uchar *dfSmall, __global uchar *dfLarge,
                     __global uchar *dfLargeHash, __global uchar *ctSmall,
                     __global uchar *ctLarge, __global uchar *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           CHECK_COUNT_PER_THREAD;

  for (int j = 0; j < CHECK_COUNT_PER_THREAD && i < inputLength; ++j, ++i) {
    uchar matches = 0;
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    if (dfSmall[byteIndex] & bitMask) {
      matches += verifySmall(ctSmall, patterns, input + i, i, inputLength);
    }

    if (i >= 2 && (dfLarge[byteIndex] & bitMask) &&
        isInHashDf(dfLargeHash, input + i)) {
      matches += verifyLarge(ctLarge, patterns, input + i, i, inputLength);
    }

    result[i] = matches;
  }
}

typedef union {
  uchar scalar[TEXTURE_CHANNEL_BYTE_SIZE];
  uint4 vector;
} img_read;

#define SHIFT_BY_CHANNEL_SIZE(x) (x >> 4)

__kernel void search_with_image(
    int inputLength, __global uchar *input,
    __global DFC_FIXED_PATTERN *patterns, __read_only image1d_t dfSmall,
    __read_only image1d_t dfLarge, __global uchar *dfLargeHash,
    __global uchar *ctSmall, __global uchar *ctLarge, __global uchar *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           CHECK_COUNT_PER_THREAD;

  for (int j = 0; j < CHECK_COUNT_PER_THREAD && i < inputLength; ++j, ++i) {
    uchar matches = 0;
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    // divide by 16 as we actually just want a single byte, but we're getting 16
    img_read df =
        (img_read)read_imageui(dfSmall, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    if (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) {
      matches += verifySmall(ctSmall, patterns, input + i, i, inputLength);
    }

    df = (img_read)read_imageui(dfLarge, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    if (i >= 2 &&
        (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) &&
        isInHashDf(dfLargeHash, input + i)) {
      matches += verifyLarge(ctLarge, patterns, input + i, i, inputLength);
    }

    result[i] = matches;
  }
}
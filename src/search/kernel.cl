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

void verifySmall(__global CompactTableSmallEntry *ct,
                   __global DFC_FIXED_PATTERN *patterns, __global uchar *input,
                   int currentPos, int inputLength, __global VerifyResult *result) {
  uchar hash = input[0];
  int matches = 0;
  for (int i = 0; i < (ct + hash)->pidCount; ++i) {
    PID_TYPE pid = (ct + hash)->pids[i];

    int patternLength = (patterns + pid)->pattern_length;
    if (patternLength == 3) {
      --input;
      --currentPos;
    }

    if ((ct + hash)->pidCount > i && currentPos >= 0 &&
        inputLength - currentPos >= patternLength &&
        doesPatternMatch(input, (patterns + pid)->original_pattern,
                         patternLength,
                         (patterns + pid)->is_case_insensitive)) {
      result->matchesSmallCt[result->matchCountSmallCt] = pid;
      ++result->matchCountSmallCt;
    }
  }
}

void verifyLarge(__global CompactTableLarge *ct,
                __global DFC_FIXED_PATTERN *patterns, __global uchar *input,
                int currentPos, int inputLength, __global VerifyResult *result) {
  /*
   the last two bytes are used to match,
   hence we are now at least 2 bytes into the pattern
   */
  input -= 2;
  currentPos -= 2;
  uint bytePattern = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  uint hash = hashForLargeCompactTable(bytePattern);

  uchar multiplier = 0;
  for (; GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++multiplier) {
    if (GET_ENTRY_LARGE_CT(hash, multiplier)->pattern == bytePattern) {
      for (int i = 0; i < GET_ENTRY_LARGE_CT(hash, multiplier)->pidCount; ++i) {
        PID_TYPE pid = GET_ENTRY_LARGE_CT(hash, multiplier)->pids[i];

        int patternLength = (patterns + pid)->pattern_length;
        int startOfRelativeInput = currentPos - (patternLength - 4);

        if (startOfRelativeInput >= 0 &&
            inputLength - startOfRelativeInput >= patternLength &&
           doesPatternMatch(
              input - (patternLength - 4), (patterns + pid)->original_pattern,
              patternLength, (patterns + pid)->is_case_insensitive)) {
          result->matchesLargeCt[result->matchCountLargeCt] = pid;
          ++result->matchCountLargeCt;
        }
      }

      break;
    }
  }
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
                     __global uchar *ctLarge, __global VerifyResult *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    result[i].matchCountSmallCt = 0;
    result[i].matchCountLargeCt = 0;

    if (dfSmall[byteIndex] & bitMask) {
      verifySmall(ctSmall, patterns, input + i, i, inputLength,
                     result + i);
    }

    if (i >= 2 && (dfLarge[byteIndex] & bitMask) &&
        isInHashDf(dfLargeHash, input + i)) {
      verifyLarge(ctLarge, patterns, input + i, i, inputLength, result + i);
    }
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
    __global uchar *ctSmall, __global uchar *ctLarge, __global VerifyResult *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    result[i].matchCountSmallCt = 0;
    result[i].matchCountLargeCt = 0;

    // divide by 16 as we actually just want a single byte, but we're getting 16
    img_read df =
        (img_read)read_imageui(dfSmall, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    if (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) {
      verifySmall(ctSmall, patterns, input + i, i, inputLength, result + i);
    }

    df = (img_read)read_imageui(dfLarge, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    if (i >= 2 &&
        (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) &&
        isInHashDf(dfLargeHash, input + i)) {
      verifyLarge(ctLarge, patterns, input + i, i, inputLength, result + i);
    }
  }
}

bool isInHashDfLocal(__local uchar *df, __global uchar *input) {
  input -= 2;

  uint data = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  ushort byteIndex = directFilterHash(data);
  ushort bitMask = BMASK(data & DF_MASK);

  return df[byteIndex] & bitMask;
}

__kernel void search_with_local(
    int inputLength, __global uchar *input,
    __global DFC_FIXED_PATTERN *patterns, __global uchar *dfSmall,
    __global uchar *dfLarge, __global uchar *dfLargeHash,
    __global uchar *ctSmall, __global uchar *ctLarge, __global VerifyResult *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  __local uchar dfSmallLocal[DF_SIZE_REAL];
  __local uchar dfLargeLocal[DF_SIZE_REAL];
  __local uchar dfLargeHashLocal[DF_SIZE_REAL];

  for (int j = LOCAL_MEMORY_LOAD_PER_ITEM * get_local_id(0);
       j < (LOCAL_MEMORY_LOAD_PER_ITEM * (get_local_id(0) + 1)); ++j) {
    dfSmallLocal[j] = dfSmall[j];
    dfLargeLocal[j] = dfLarge[j];
    dfLargeHashLocal[j] = dfLargeHash[j];
  }

  barrier(CLK_LOCAL_MEM_FENCE);

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    uchar matches = 0;
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    result[i].matchCountSmallCt = 0;
    result[i].matchCountLargeCt = 0;

    if (dfSmall[byteIndex] & bitMask) {
      verifySmall(ctSmall, patterns, input + i, i, inputLength,
                     result + i);
    }

    if (i >= 2 && (dfLarge[byteIndex] & bitMask) &&
        isInHashDf(dfLargeHash, input + i)) {
      verifyLarge(ctLarge, patterns, input + i, i, inputLength, result + i);
    }
  }
}

__kernel void filter(int inputLength, __global uchar *input,
                     __global uchar *dfSmall, __global uchar *dfLarge,
                     __global uchar *dfLargeHash, __global uchar *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    // set the first bit
    // (important that it's not an OR as we need to set it to 0 since the memory
    // might be uninitialized)
    result[i] = (dfSmall[byteIndex] & bitMask) > 0;

    // set the second bit
    result[i] |= (i >= 2 && (dfLarge[byteIndex] & bitMask) &&
                  isInHashDf(dfLargeHash, input + i))
                 << 1;
  }
}

__kernel void filter_with_image(int inputLength, __global uchar *input,
                                __read_only image1d_t dfSmall,
                                __read_only image1d_t dfLarge,
                                __global uchar *dfLargeHash,
                                __global uchar *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    uchar matches = 0;
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    img_read df =
        (img_read)read_imageui(dfSmall, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    result[i] =
        (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) > 0;

    df = (img_read)read_imageui(dfLarge, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    result[i] |=
        (i >= 2 &&
         (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) &&
         isInHashDf(dfLargeHash, input + i))
        << 1;
  }
}

__kernel void filter_with_local(int inputLength, __global uchar *input,
                                __global uchar *dfSmall,
                                __global uchar *dfLarge,
                                __global uchar *dfLargeHash,
                                __global uchar *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  __local uchar dfSmallLocal[DF_SIZE_REAL];
  __local uchar dfLargeLocal[DF_SIZE_REAL];
  __local uchar dfLargeHashLocal[DF_SIZE_REAL];

  for (int j = LOCAL_MEMORY_LOAD_PER_ITEM * get_local_id(0);
       j < (LOCAL_MEMORY_LOAD_PER_ITEM * (get_local_id(0) + 1)); ++j) {
    dfSmallLocal[j] = dfSmall[j];
    dfLargeLocal[j] = dfLarge[j];
    dfLargeHashLocal[j] = dfLargeHash[j];
  }

  barrier(CLK_LOCAL_MEM_FENCE);

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    // set the first bit
    // (important that it's not an OR as we need to set it to 0 since the memory
    // might be uninitialized)
    result[i] = (dfSmallLocal[byteIndex] & bitMask) > 0;

    // set the second bit
    result[i] |= (i >= 2 && (dfLargeLocal[byteIndex] & bitMask) &&
                  isInHashDfLocal(dfLargeHashLocal, input + i))
                 << 1;
  }
}
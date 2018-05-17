#include "shared-functions.h"

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

void verifySmall(__global CompactTableSmallEntry *ct, __global PID_TYPE *pids,
                 __global DFC_FIXED_PATTERN *patterns, __global uchar *input,
                 int currentPos, int inputLength,
                 __global VerifyResult *result) {
  uchar hash = input[0];
  pids += (ct + hash)->offset;

  int matches = 0;
  for (int i = 0; i < (ct + hash)->pidCount; ++i) {
    PID_TYPE pid = pids[i];

    int patternLength = (patterns + pid)->pattern_length;

    if (inputLength - currentPos >= patternLength &&
        doesPatternMatch(input, (patterns + pid)->original_pattern,
                         patternLength,
                         (patterns + pid)->is_case_insensitive)) {
      if (result->matchCount < MAX_MATCHES_PER_THREAD) {
        result->matches[result->matchCount] = pid;
      }
      ++result->matchCount;
    }
  }
}

void verifyLarge(__global CompactTableLargeBucket *buckets,
                 __global CompactTableLargeEntry *entries,
                 __global PID_TYPE *pids, __global DFC_FIXED_PATTERN *patterns,
                 __global uchar *input, int currentPos, int inputLength,
                 __global VerifyResult *result) {
  uint bytePattern = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  uint hash = hashForLargeCompactTable(bytePattern);

  int entryOffset = (buckets + hash)->entryOffset;

  for (int i = 0; i < (buckets + hash)->entryCount; ++i) {
    if ((entries + entryOffset + i)->pattern == bytePattern) {
      int pidOffset = (entries + entryOffset + i)->pidOffset;

      for (int j = 0; j < (entries + entryOffset + i)->pidCount; ++j) {
        PID_TYPE pid = pids[pidOffset + j];

        int patternLength = (patterns + pid)->pattern_length;
        if (inputLength - currentPos >= patternLength) {
          if (doesPatternMatch(input, (patterns + pid)->original_pattern,
                               patternLength,
                               (patterns + pid)->is_case_insensitive)) {
            if (result->matchCount < MAX_MATCHES_PER_THREAD) {
              result->matches[result->matchCount] = pid;
            }
            ++result->matchCount;
          }
        }
      }

      break;
    }
  }
}

bool isInHashDf(__global uchar *df, __global uchar *input) {
  uint data = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  ushort byteIndex = directFilterHash(data);
  ushort bitMask = BMASK(data & DF_MASK);

  return df[byteIndex] & bitMask;
}

__kernel void search(int inputLength, __global uchar *input,
                     __global DFC_FIXED_PATTERN *patterns,
                     __global uchar *dfSmall, __global uchar *dfLarge,
                     __global uchar *dfLargeHash,
                     __global CompactTableSmallEntry *ctSmallEntries,
                     __global PID_TYPE *ctSmallPids,
                     __global CompactTableLargeBucket *ctLargeBuckets,
                     __global CompactTableLargeEntry *ctLargeEntries,
                     __global PID_TYPE *ctLargePids,
                     __global VerifyResult *result) {
  uint threadId = (get_group_id(0) * get_local_size(0) + get_local_id(0));
  result[threadId].matchCount = 0;

  for (int j = 0, i = threadId * THREAD_GRANULARITY; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    if (dfSmall[byteIndex] & bitMask) {
      verifySmall(ctSmallEntries, ctSmallPids, patterns, input + i, i,
                  inputLength, result + threadId);
    }

    if ((dfLarge[byteIndex] & bitMask) && i < inputLength - 3 &&
        isInHashDf(dfLargeHash, input + i)) {
      verifyLarge(ctLargeBuckets, ctLargeEntries, ctLargePids, patterns,
                  input + i, i, inputLength, result + threadId);
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
    __global CompactTableSmallEntry *ctSmallEntries,
    __global PID_TYPE *ctSmallPids,
    __global CompactTableLargeBucket *ctLargeBuckets,
    __global CompactTableLargeEntry *ctLargeEntries,
    __global PID_TYPE *ctLargePids, __global VerifyResult *result) {
  uint threadId = (get_group_id(0) * get_local_size(0) + get_local_id(0));
  result[threadId].matchCount = 0;

  for (int j = 0, i = threadId * THREAD_GRANULARITY; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    // divide by 16 as we actually just want a single byte, but we're getting 16
    img_read df =
        (img_read)read_imageui(dfSmall, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    if (df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) {
      verifySmall(ctSmallEntries, ctSmallPids, patterns, input + i, i,
                  inputLength, result + threadId);
    }

    df = (img_read)read_imageui(dfLarge, SHIFT_BY_CHANNEL_SIZE(byteIndex));
    if ((df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) &&
        i < inputLength - 3 && isInHashDf(dfLargeHash, input + i)) {
      verifyLarge(ctLargeBuckets, ctLargeEntries, ctLargePids, patterns,
                  input + i, i, inputLength, result + threadId);
    }
  }
}

bool isInHashDfLocal(__local uchar *df, __global uchar *input) {
  uint data = input[3] << 24 | input[2] << 16 | input[1] << 8 | input[0];
  ushort byteIndex = directFilterHash(data);
  ushort bitMask = BMASK(data & DF_MASK);

  return df[byteIndex] & bitMask;
}

__kernel void search_with_local(
    int inputLength, __global uchar *input,
    __global DFC_FIXED_PATTERN *patterns, __global uchar *dfSmall,
    __global uchar *dfLarge, __global uchar *dfLargeHash,
    __global CompactTableSmallEntry *ctSmallEntries,
    __global PID_TYPE *ctSmallPids,
    __global CompactTableLargeBucket *ctLargeBuckets,
    __global CompactTableLargeEntry *ctLargeEntries,
    __global PID_TYPE *ctLargePids, __global VerifyResult *result) {
  uint threadId = (get_group_id(0) * get_local_size(0) + get_local_id(0));
  result[threadId].matchCount = 0;

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

  for (int j = 0, i = threadId * THREAD_GRANULARITY; j < THREAD_GRANULARITY && i < inputLength; ++j, ++i) {
    uchar matches = 0;
    short data = *(input + i + 1) << 8 | *(input + i);
    short byteIndex = BINDEX(data & DF_MASK);
    short bitMask = BMASK(data & DF_MASK);

    if (dfSmall[byteIndex] & bitMask) {
      verifySmall(ctSmallEntries, ctSmallPids, patterns, input + i, i,
                  inputLength, result + threadId);
    }

    if ((dfLarge[byteIndex] & bitMask) && i < inputLength - 3 &&
        isInHashDf(dfLargeHash, input + i)) {
      verifyLarge(ctLargeBuckets, ctLargeEntries, ctLargePids, patterns,
                  input + i, i, inputLength, result + threadId);
    }
  }
}

typedef union {
  uchar16 vector_raw;
  short8 vector;
  short scalar[8];
} Vec8;

#define BINDEX_VEC(x) ((x) >> (short8)(3))
#define BMASK_VEC(x) ((short8)(1) << ((x) & (short8)(0x7)))

__kernel void search_vec(int inputLength, __global uchar *input,
                         __global DFC_FIXED_PATTERN *patterns,
                         __global uchar *dfSmall, __global uchar *dfLarge,
                         __global uchar *dfLargeHash,
                         __global CompactTableSmallEntry *ctSmallEntries,
                         __global PID_TYPE *ctSmallPids,
                         __global CompactTableLargeBucket *ctLargeBuckets,
                         __global CompactTableLargeEntry *ctLargeEntries,
                         __global PID_TYPE *ctLargePids,
                         __global VerifyResult *result) {
  uint threadId = (get_group_id(0) * get_local_size(0) + get_local_id(0));
  result[threadId].matchCount = 0;

  for (int j = 0, i = threadId * THREAD_GRANULARITY; j < THREAD_GRANULARITY && i < inputLength; j += 8, i += 8) {

    uchar8 dataThis = vload8(i >> 3, input);
    uchar8 dataNext = vload8((i >> 3) + 1, input);
    uchar16 shuffleMask =
        (uchar16)(0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 0);
    Vec8 overlappingData = (Vec8)shuffle(dataThis, shuffleMask);
    overlappingData.vector_raw.sf = dataNext.s0;

    overlappingData.vector = overlappingData.vector & (short8)(DF_MASK);
    short8 bitMasks = BMASK_VEC(overlappingData.vector);
    Vec8 bitIndices = (Vec8)BINDEX_VEC(overlappingData.vector);

    Vec8 dfGather;
    // Gather can't be vectorized on OpenCL
    for (int k = 0; k < 8; ++k) {
      dfGather.scalar[k] = dfSmall[bitIndices.scalar[k]];
    }

    Vec8 filterResult = (Vec8)(dfGather.vector & bitMasks);

    for (int k = 0; k < 8 && i + k < inputLength; ++k) {
      if (filterResult.scalar[k]) {
        verifySmall(ctSmallEntries, ctSmallPids, patterns, input + i + k, i + k,
                    inputLength, result + threadId);
      }
    }

    // Gather can't be vectorized on OpenCL
    for (int k = 0; k < 8; ++k) {
      dfGather.scalar[k] = dfLarge[bitIndices.scalar[k]];
    }

    filterResult = (Vec8)(dfGather.vector & bitMasks);

    for (int k = 0; k < 8 && i + k < inputLength; ++k) {
      if (filterResult.scalar[k] && i + k < inputLength - 3 &&
          isInHashDf(dfLargeHash, input + i + k)) {
        verifyLarge(ctLargeBuckets, ctLargeEntries, ctLargePids, patterns,
                    input + i + k, i + k, inputLength, result + threadId);
      }
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
    result[i] |= ((dfLarge[byteIndex] & bitMask) && i < inputLength - 3 &&
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
        ((df.scalar[byteIndex % TEXTURE_CHANNEL_BYTE_SIZE] & bitMask) &&
         i < inputLength - 3 && isInHashDf(dfLargeHash, input + i))
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
    result[i] |= ((dfLargeLocal[byteIndex] & bitMask) && i < inputLength - 3 &&
                  isInHashDfLocal(dfLargeHashLocal, input + i))
                 << 1;
  }
}

typedef union {
  uchar8 vector;
  uchar scalar[8];
} UChar8;

__kernel void filter_vec(int inputLength, __global uchar *input,
                         __global uchar *dfSmall, __global uchar *dfLarge,
                         __global uchar *dfLargeHash, __global uchar *result) {
  uint i = (get_group_id(0) * get_local_size(0) + get_local_id(0)) *
           THREAD_GRANULARITY;

  for (int j = 0; j < THREAD_GRANULARITY && i < inputLength; j += 8, i += 8) {
    uchar8 dataThis = vload8(i >> 3, input);
    uchar8 dataNext = vload8((i >> 3) + 1, input);
    uchar16 shuffleMask =
        (uchar16)(0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 0);
    Vec8 overlappingData = (Vec8)shuffle(dataThis, shuffleMask);
    overlappingData.vector_raw.sf = dataNext.s0;

    overlappingData.vector = overlappingData.vector & (short8)(DF_MASK);
    short8 bitMasks = BMASK_VEC(overlappingData.vector);
    Vec8 bitIndices = (Vec8)BINDEX_VEC(overlappingData.vector);

    Vec8 dfGatherSmall;
    Vec8 dfGatherLarge;

    for (int k = 0; k < 8; ++k) {
      dfGatherSmall.scalar[k] = dfSmall[bitIndices.scalar[k]];
      dfGatherLarge.scalar[k] = dfLarge[bitIndices.scalar[k]];
    }

    UChar8 filterResultSmall =
        (UChar8)convert_uchar8(dfGatherSmall.vector & bitMasks);
    UChar8 filterResultLarge =
        (UChar8)convert_uchar8(dfGatherLarge.vector & bitMasks);

    UChar8 resultVector =
        (UChar8)convert_uchar8(filterResultSmall.vector > (uchar)0);

    for (int k = 0; k < 8 && i + k < inputLength; ++k) {
      resultVector.scalar[k] |=
          (filterResultLarge.scalar[k] && i + k < inputLength - 3 &&
           isInHashDf(dfLargeHash, input + i + k))
          << 1;
    }

    vstore8(resultVector.vector, i >> 3, result);
  }
}

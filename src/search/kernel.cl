#define DF_SIZE 0x10000
#define DF_SIZE_REAL 0x2000

#define INIT_HASH_SIZE 65536

#define PID_TYPE uint

#define DIRECT_FILTER_SIZE_SMALL DF_SIZE_REAL

#define SMALL_DF_MIN_PATTERN_SIZE 1
#define SMALL_DF_MAX_PATTERN_SIZE 3

#define MAX_PID_PER_ENTRY 100
#define MAX_ENTRIES_PER_BUCKET 100
#define COMPACT_TABLE_SIZE_SMALL 0x100
#define COMPACT_TABLE_SIZE_LARGE 0x1000

#define MAX_EQUAL_PATTERNS 5
#define MAX_PATTERN_LENGTH 25

#define BINDEX(x) ((x) >> 3)
#define BMASK(x) (1 << ((x)&0x7))

#define DF_MASK (DF_SIZE - 1)

#define GET_ENTRY_LARGE_CT(hash, x) \
  ((ct + hash)->entries + sizeof(CompactTableLargeEntry) * x)

typedef struct CompactTableSmallEntry_ {
  uchar pattern;
  uchar pidCount;
  PID_TYPE pids[MAX_PID_PER_ENTRY];
} CompactTableSmallEntry;

typedef struct CompactTableLargeEntry_ {
  uint pattern;
  uchar pidCount;
  PID_TYPE pids[MAX_PID_PER_ENTRY];
} CompactTableLargeEntry;

typedef struct CompactTableLarge_ {
  CompactTableLargeEntry entries[MAX_ENTRIES_PER_BUCKET];
} CompactTableLarge;

typedef struct _dfc_fixed_pattern {
  int pattern_length;
  int is_case_insensitive;

  uchar upper_case_pattern[MAX_PATTERN_LENGTH];
  uchar original_pattern[MAX_PATTERN_LENGTH];

  int external_id_count;
  PID_TYPE external_ids[MAX_EQUAL_PATTERNS];
} DFC_FIXED_PATTERN;

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

ushort directFilterHash(uint val) { return BINDEX((val * 8387) & DF_MASK); }

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
                     __global uchar *dfSmall,
                     __global CompactTableSmallEntry *ctSmall,
                     __global uchar *dfLarge, __global uchar *dfLargeHash,
                     __global CompactTableLarge *ctLarge,
                     __global uchar *result) {
  uchar matches = 0;

  uint i = get_group_id(0) * get_local_size(0) + get_local_id(0);

  if (i >= inputLength) return;

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

#ifndef DFC_SHARED_H
#define DFC_SHARED_H

#ifdef DFC_OPENCL
#define uint8_t uchar
#define uint32_t uint
#define int32_t int
#define uint16_t ushort
#else
#include <stdint.h>
#endif

#define PID_TYPE int32_t

#define DF_SIZE 0x10000
#define DF_SIZE_REAL 0x2000

#define INIT_HASH_SIZE 65536

#define DIRECT_FILTER_SIZE_SMALL DF_SIZE_REAL

#define SMALL_DF_MIN_PATTERN_SIZE 1
#define SMALL_DF_MAX_PATTERN_SIZE 3

#define MAX_PID_PER_ENTRY_SMALL_CT 50

#define MAX_PID_PER_ENTRY_LARGE_CT 5
#define MAX_ENTRIES_PER_BUCKET 25

#define COMPACT_TABLE_SIZE_SMALL 0x100
#define COMPACT_TABLE_SIZE_LARGE 0x1000

#define MAX_EQUAL_PATTERNS 10
#define MAX_PATTERN_LENGTH 128

#define BINDEX(x) ((x) >> 3)
#define BMASK(x) (1 << ((x)&0x7))

#define DF_MASK (DF_SIZE - 1)

#define GET_ENTRY_LARGE_CT(hash, x) ((ct + hash)->entries + x)

#define TEXTURE_CHANNEL_BYTE_SIZE 16

typedef struct CompactTableSmallEntry_ {
  uint8_t pattern;
  int32_t pidCount;
  int32_t offset;
} CompactTableSmallEntry;

typedef struct CompactTableLargeEntry_ {
  uint32_t pattern;
  int32_t pidCount;
  int32_t pidOffset;
} CompactTableLargeEntry;

typedef struct CompactTableLargeBucket_ {
  int32_t entryCount;
  int32_t entryOffset;
} CompactTableLargeBucket;

typedef struct _dfc_fixed_pattern {
  int32_t pattern_length;
  int32_t is_case_insensitive;

  uint8_t upper_case_pattern[MAX_PATTERN_LENGTH];
  uint8_t original_pattern[MAX_PATTERN_LENGTH];

  int32_t external_id_count;
  PID_TYPE external_ids[MAX_EQUAL_PATTERNS];
} DFC_FIXED_PATTERN;

typedef struct VerifyResult_ {
  PID_TYPE matchesSmallCt[MAX_PID_PER_ENTRY_SMALL_CT];
  PID_TYPE matchesLargeCt[MAX_PID_PER_ENTRY_LARGE_CT];

  int32_t matchCountSmallCt;
  int32_t matchCountLargeCt;
} VerifyResult;

#endif
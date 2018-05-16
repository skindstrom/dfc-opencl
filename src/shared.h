#ifndef DFC_SHARED_H
#define DFC_SHARED_H

#ifdef DFC_OPENCL
#define uint8_t uchar
#define int8_t char
#define uint16_t ushort
#define int16_t short
#define uint32_t uint
#define int32_t int
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

#define COMPACT_TABLE_SIZE_SMALL 0x100
#define COMPACT_TABLE_SIZE_LARGE 0x1000

#define MAX_EQUAL_PATTERNS 220
#define MAX_PATTERN_LENGTH 64

#define MAX_MATCHES 5

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
  PID_TYPE matches[MAX_MATCHES];

  int8_t matchCount;
} VerifyResult;

#endif
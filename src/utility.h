#ifndef DFC_UTILITY_H
#define DFC_UTILITY_H

#include <ctype.h>
#include <stdint.h>
#include "constants.h"

#define BINDEX(x) ((x) >> 3)
#define BMASK(x) (1 << ((x)&0x7))

#define DF_MASK (DF_SIZE - 1)

#define CT2_TABLE_SIZE_MASK (CT2_TABLE_SIZE - 1)
#define CT3_TABLE_SIZE_MASK (CT3_TABLE_SIZE - 1)
#define CT4_TABLE_SIZE_MASK (CT4_TABLE_SIZE - 1)
#define CT8_TABLE_SIZE_MASK (CT8_TABLE_SIZE - 1)

#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif

#define MEMASSERT_DFC(p, s)            \
  if (!p) {                            \
    printf("DFC-No Memory: %s!\n", s); \
  }

typedef enum _dfcMemoryType {
  DFC_MEMORY_TYPE__NONE = 0,
  DFC_MEMORY_TYPE__DFC,
  DFC_MEMORY_TYPE__PATTERN,
} dfcMemoryType;

typedef enum _dfcDataType {
  DFC_NONE = 0,
  DFC_PID_TYPE,
} dfcDataType;

float my_sqrtf(float input, float x) {
  int i;
  if (x == 0 && input == 0) return 0;
  for (i = 0; i < 10; i++) {
    x = (x + (input / x)) / 2;
  }
  return x;
}

void init_xlatcase(unsigned char *out) {
  int i;
  for (i = 0; i < 256; i++) out[i] = (unsigned char)toupper(i);
}

void ConvertCaseEx(unsigned char *d, unsigned char *s, int m,
                   unsigned char *xlatcase) {
  int i;

  for (i = 0; i < m; i++) d[i] = xlatcase[s[i]];
}

uint32_t hashForLargeCompactTable(uint32_t input) {
  return (input * 8389) & (COMPACT_TABLE_SIZE_LARGE - 1);
}

uint16_t directFilterHash(uint32_t val) {
  return (val * 8387) & DF_MASK;
}

#endif

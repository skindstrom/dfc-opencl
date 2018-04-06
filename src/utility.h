#ifndef DFC_UTILITY_H
#define DFC_UTILITY_H

#include <ctype.h>
#include <stdint.h>
#include "constants.h"

#define BINDEX(x) ((x) >> 3)
#define BMASK(x) (1 << ((x)&0x7))

#define DF_MASK (DF_SIZE - 1)

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

float my_sqrtf(float input, float x);

void init_xlatcase(unsigned char *out);

void ConvertCaseEx(unsigned char *d, unsigned char *s, int m,
                   unsigned char *xlatcase);

uint32_t hashForLargeCompactTable(uint32_t input);

uint16_t directFilterHash(int32_t val);

#endif

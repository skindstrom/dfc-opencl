#ifndef DFC_SHARED_FUNCTIONS
#define DFC_SHARED_FUNCTIONS

#include "shared.h"

static uint32_t hashForLargeCompactTable(uint32_t input) {
  return (input * 8389) & (COMPACT_TABLE_SIZE_LARGE - 1);
}

static uint16_t directFilterHash(int32_t val) {
  return BINDEX((val * 8387) & DF_MASK);
}

#endif
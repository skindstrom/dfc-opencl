#ifndef DFC_SHARED_INTERNAL_H
#define DFC_SHARED_INTERNAL_H

#include "shared.h"

typedef struct VerifyResult_ {
  uint8_t matchCount;
  PID_TYPE matches[MAX_MATCHES_PER_THREAD];
} VerifyResult;

#endif
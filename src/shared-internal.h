#ifndef DFC_SHARED_INTERNAL_H
#define DFC_SHARED_INTERNAL_H

#include "shared.h"

typedef struct VerifyResult_ {
  PID_TYPE matches[MAX_MATCHES_PER_THREAD];

  uint8_t matchCount;
} VerifyResult;

#endif
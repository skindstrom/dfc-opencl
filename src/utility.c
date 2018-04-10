#include "utility.h"
#include "shared.h"

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
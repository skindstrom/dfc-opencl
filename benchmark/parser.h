#ifndef DFC_BENCHMARK_PARSER_H
#define DFC_BENCHMARK_PARSER_H

#include "dfc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AddPattern)(DFC_PATTERN_INIT *, unsigned char *, int, int,
                           PID_TYPE);

void parse_file(const char *file_name, DFC_PATTERN_INIT *pattern_init,
                AddPattern add_pattern);

#ifdef __cplusplus
}
#endif

#endif
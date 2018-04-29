#ifndef DFC_BENCHMARK_PARSER_H
#define DFC_BENCHMARK_PARSER_H

#include "dfc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*AddPattern)(DFC_PATTERN_INIT *, unsigned char *, int, int,
                           PID_TYPE);

typedef char *(*InputMalloc)(int);

void parse_pattern_file(const char *file_name, DFC_PATTERN_INIT *pattern_init,
                        AddPattern add_pattern);

char *read_data_file(const char *file_name, InputMalloc allocator);

#ifdef __cplusplus
}
#endif

#endif
#ifndef DFC_CONSTANTS_H
#define DFC_CONSTANTS_H

#define DF_SIZE 0x10000
#define DF_SIZE_REAL 0x2000

#define INIT_HASH_SIZE 65536

#define PID_TYPE uint32_t

#define DIRECT_FILTER_SIZE_SMALL DF_SIZE_REAL

#define SMALL_DF_MIN_PATTERN_SIZE 1
#define SMALL_DF_MAX_PATTERN_SIZE 3

#define MAX_PID_PER_ENTRY 100
#define MAX_ENTRIES_PER_BUCKET 100
#define COMPACT_TABLE_SIZE_SMALL 0x100
#define COMPACT_TABLE_SIZE_LARGE 0x1000

#define MAX_EQUAL_PATTERNS 5
#define MAX_PATTERN_LENGTH 25

#define TOO_MANY_ENTRIES_IN_SMALL_CT_EXIT_CODE 2
#define TOO_MANY_PID_IN_SMALL_CT_EXIT_CODE 3
#define TOO_MANY_ENTRIES_IN_LARGE_CT_EXIT_CODE 4
#define TOO_MANY_PID_IN_LARGE_CT_EXIT_CODE 5
#define PATTERN_TOO_LARGE_EXIT_CODE 6
#define TOO_MANY_EQUAL_PATTERNS_EXIT_CODE 7
#define OPENCL_NO_PLATFORM_EXIT_CODE 8
#define OPENCL_NO_DEVICE_EXIT_CODE 9
#define OPENCL_COULD_NOT_CREATE_CONTEXT 10
#define COULD_NOT_LOAD_KERNEL 11
#define OPENCL_COULD_NOT_CREATE_PROGRAM 12
#define OPENCL_COULD_NOT_BUILD_PROGRAM 13
#define OPENCL_COULD_NOT_CREATE_KERNEL 14

#define SEARCH_WITH_GPU

#endif
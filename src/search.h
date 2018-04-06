#ifndef DFC_SEARCH_H
#define DFC_SEARCH_H

#if SEARCH_WITH_GPU
#include "./search/search-gpu.h"
#else
#include "./search/search-cpu.h"
#endif

#endif
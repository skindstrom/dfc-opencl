#ifndef DFC_SEARCH_H
#define DFC_SEARCH_H

#ifndef SEARCH_WITH_GPU
#include "./search/search-cpu.h"
#else
#include "./search/search-gpu.h"
#endif

#endif
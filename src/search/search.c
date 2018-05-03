#include "search.h"

extern int searchCpu(ReadFunction, MatchFunction);
extern int searchCpuEmulateGpu(ReadFunction, MatchFunction);
extern int searchGpu(ReadFunction, MatchFunction);

int search(ReadFunction read, MatchFunction onMatch) {
  if (SEARCH_WITH_GPU || HETEROGENEOUS_DESIGN) {
    return searchGpu(read, onMatch);
  }
  return searchCpu(read, onMatch);
}
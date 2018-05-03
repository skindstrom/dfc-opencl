#include "search.h"

extern int searchCpu(MatchFunction);
extern int searchCpuEmulateGpu(MatchFunction);
extern int searchGpu(MatchFunction);

int search(MatchFunction onMatch) {
  if (SEARCH_WITH_GPU || HETEROGENEOUS_DESIGN) {
    return searchGpu(onMatch);
  }
  return searchCpu(onMatch);
}
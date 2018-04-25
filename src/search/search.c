#include "search.h"

extern int searchCpu();
extern int searchGpu();

int search(MatchFunction onMatch) {
  if (SEARCH_WITH_GPU || HETEROGENEOUS_DESIGN) {
    return searchGpu();
  } else {
    return searchCpu(onMatch);
  }
}
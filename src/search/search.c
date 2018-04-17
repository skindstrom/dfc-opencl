#include "search.h"

extern int searchCpu();
extern int searchGpu();

int search() {
  if (SEARCH_WITH_GPU) {
    return searchGpu();
  } else {
    return searchCpu();
  }
}
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "math.h"
#include "stdio.h"
#include "stdlib.h"

#include "memory.h"
#include "search.h"
#include "shared-internal.h"
#include "timer.h"

extern int exactMatchingUponFiltering(uint8_t *result, int length,
                                      DFC_PATTERNS *patterns, MatchFunction);
int getThreadCountForBytes(int size) {
  return ceil(size / (float)(THREAD_GRANULARITY));
}

void setKernelArgsNormalDesign(cl_kernel kernel, DfcOpenClBuffers *mem,
                               int readCount) {
  clSetKernelArg(kernel, 0, sizeof(int), &readCount);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &mem->input);

  clSetKernelArg(kernel, 2, sizeof(cl_mem), &mem->patterns);

  clSetKernelArg(kernel, 3, sizeof(cl_mem), &mem->dfSmall);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &mem->dfLarge);
  clSetKernelArg(kernel, 5, sizeof(cl_mem), &mem->dfLargeHash);

  clSetKernelArg(kernel, 6, sizeof(cl_mem), &mem->ctSmallEntries);
  clSetKernelArg(kernel, 7, sizeof(cl_mem), &mem->ctSmallPids);

  clSetKernelArg(kernel, 8, sizeof(cl_mem), &mem->ctLargeBuckets);
  clSetKernelArg(kernel, 9, sizeof(cl_mem), &mem->ctLargeEntries);
  clSetKernelArg(kernel, 10, sizeof(cl_mem), &mem->ctLargePids);

  clSetKernelArg(kernel, 11, sizeof(cl_mem), &mem->result);
}

void setKernelArgsHetDesign(cl_kernel kernel, DfcOpenClBuffers *mem,
                            int readCount) {
  clSetKernelArg(kernel, 0, sizeof(int), &readCount);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &mem->input);

  clSetKernelArg(kernel, 2, sizeof(cl_mem), &mem->dfSmall);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), &mem->dfLarge);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &mem->dfLargeHash);

  clSetKernelArg(kernel, 5, sizeof(cl_mem), &mem->result);
}

void setKernelArgs(cl_kernel kernel, DfcOpenClBuffers *mem, int readCount) {
  if (HETEROGENEOUS_DESIGN) {
    setKernelArgsHetDesign(kernel, mem, readCount);
  } else {
    setKernelArgsNormalDesign(kernel, mem, readCount);
  }
}

size_t getGlobalGroupSize(size_t localGroupSize, int inputLength) {
  const float threadCount = getThreadCountForBytes(inputLength);

  const size_t globalGroupSize =
      ceil(threadCount / localGroupSize) * localGroupSize;

  return globalGroupSize;
}

void startKernelForQueue(cl_kernel kernel, cl_command_queue queue,
                         int inputLength) {
  const size_t localGroupSize = WORK_GROUP_SIZE;
  const size_t globalGroupSize =
      getGlobalGroupSize(localGroupSize, inputLength);

  startTimer(TIMER_EXECUTE_KERNEL);
  int status = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalGroupSize,
                                      &localGroupSize, 0, NULL, NULL);
  if (BLOCKING_DEVICE_ACCESS) {
    clFinish(queue);  // only necessary for timing
  }
  stopTimer(TIMER_EXECUTE_KERNEL);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not start kernel: %d\n", status);
    exit(OPENCL_COULD_NOT_START_KERNEL);
  }
}

int handleMatches(uint8_t *result, int inputLength, DFC_PATTERNS *patterns,
                  MatchFunction onMatch) {
  VerifyResult *pidCounts = (VerifyResult *)result;

  int matches = 0;
  for (int i = 0; i < getThreadCountForBytes(inputLength); ++i) {
    VerifyResult *res = &pidCounts[i];

    for (uint8_t j = 0; j < res->matchCount && j < MAX_MATCHES_PER_THREAD;
         ++j) {
      onMatch(&patterns->dfcMatchList[res->matches[j]]);
      ++matches;
    }

    if (res->matchCount > MAX_MATCHES_PER_THREAD) {
      printf(
          "%d patterns matched at position %d, but space was only allocated "
          "for %d patterns\n",
          res->matchCount, i, MAX_MATCHES_PER_THREAD);
    }
  }
  return matches;
}

int handleResultsFromGpu(uint8_t *result, int inputLength,
                         DFC_PATTERNS *patterns, MatchFunction onMatch) {
  int matches;
  if (HETEROGENEOUS_DESIGN) {
    startTimer(TIMER_EXECUTE_HETEROGENEOUS);
    matches =
        exactMatchingUponFiltering(result, inputLength, patterns, onMatch);
    stopTimer(TIMER_EXECUTE_HETEROGENEOUS);
  } else {
    startTimer(TIMER_PROCESS_MATCHES);
    matches = handleMatches(result, inputLength, patterns, onMatch);
    stopTimer(TIMER_PROCESS_MATCHES);
  }

  return matches;
}

int readResultWithoutMap(DfcOpenClBuffers *mem, cl_command_queue queue,
                         DFC_PATTERNS *patterns, int readCount,
                         MatchFunction onMatch) {
  uint8_t *output = calloc(1, sizeInBytesOfResultVector(readCount + 1));

  startTimer(TIMER_READ_FROM_DEVICE);
  int status = clEnqueueReadBuffer(queue, mem->result, CL_BLOCKING, 0,
                                   sizeInBytesOfResultVector(readCount + 1),
                                   output, 0, NULL, NULL);
  stopTimer(TIMER_READ_FROM_DEVICE);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  int matches = handleResultsFromGpu(output, readCount, patterns, onMatch);

  free(output);

  return matches;
}

int readResultWithMap(DfcOpenClBuffers *mem, cl_command_queue queue,
                      DFC_PATTERNS *patterns, int readCount,
                      MatchFunction onMatch) {
  cl_int status;

  startTimer(TIMER_READ_FROM_DEVICE);
  uint8_t *output = clEnqueueMapBuffer(
      queue, mem->result, CL_BLOCKING, CL_MAP_READ, 0,
      sizeInBytesOfResultVector(readCount + 1), 0, NULL, NULL, &status);
  stopTimer(TIMER_READ_FROM_DEVICE);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  int matches = handleResultsFromGpu(output, readCount, patterns, onMatch);

  status = clEnqueueUnmapMemObject(queue, mem->result, output, 0, NULL, NULL);

  return matches;
}

int readResult(DfcOpenClBuffers *mem, cl_command_queue queue,
               DFC_PATTERNS *patterns, int readCount, MatchFunction onMatch) {
  if (MAP_MEMORY) {
    return readResultWithMap(mem, queue, patterns, readCount, onMatch);
  } else {
    return readResultWithoutMap(mem, queue, patterns, readCount, onMatch);
  }
}

int performSearch(ReadFunction read, MatchFunction onMatch) {
  char *input = allocateInput(INPUT_READ_CHUNK_BYTES);

  int matches = 0;
  int readCount = 0;
  // read fewer bytes to allow a bit of extra read on the GPU without
  // conditionals
  while ((readCount = read(INPUT_READ_CHUNK_BYTES - 8,
                           MAX_PATTERN_LENGTH, input))) {
    writeInputBufferToDevice(input, readCount);

    setKernelArgs(DFC_OPENCL_ENVIRONMENT.kernel, &DFC_OPENCL_BUFFERS,
                  readCount);
    startKernelForQueue(DFC_OPENCL_ENVIRONMENT.kernel,
                        DFC_OPENCL_ENVIRONMENT.queue, readCount);

    matches +=
        readResult(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue,
                   DFC_HOST_MEMORY.dfcStructure->patterns, readCount, onMatch);

    input = getOwnershipOfInputBuffer();
  }
  // should be nop since readCount == 0. Needed to make sure that buffer is not
  // owned
  writeInputBufferToDevice(input, readCount);

  return matches;
}

int searchGpu(ReadFunction read, MatchFunction onMatch) {
  prepareOpenClBuffersForSearch();

  int matches = performSearch(read, onMatch);

  freeOpenClBuffers();

  return matches;
}

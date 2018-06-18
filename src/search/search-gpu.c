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

cl_event resultEvent;
cl_event resultEvent2;

cl_event inputEvent;

extern int exactMatchingUponFiltering(uint8_t *input, uint8_t *result,
                                      int length, DFC_PATTERNS *patterns,
                                      MatchFunction);
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

int handleResultsFromGpu(uint8_t *input, uint8_t *result, int inputLength,
                         DFC_PATTERNS *patterns, MatchFunction onMatch) {
  int matches;
  if (HETEROGENEOUS_DESIGN) {
    startTimer(TIMER_EXECUTE_HETEROGENEOUS);
    matches = exactMatchingUponFiltering(input, result, inputLength, patterns,
                                         onMatch);
    stopTimer(TIMER_EXECUTE_HETEROGENEOUS);
  } else {
    startTimer(TIMER_PROCESS_MATCHES);
    matches = handleMatches(result, inputLength, patterns, onMatch);
    stopTimer(TIMER_PROCESS_MATCHES);
  }

  return matches;
}

void readResultWithoutMap(DfcOpenClBuffers *mem, cl_command_queue queue,
                          int readCount, uint8_t *output) {
  bool blocking = OVERLAPPING_EXECUTION ? 0 : CL_BLOCKING;

  startTimer(TIMER_READ_FROM_DEVICE);
  int status = clEnqueueReadBuffer(queue, mem->result, blocking, 0,
                                   sizeInBytesOfResultVector(readCount), output,
                                   0, NULL, &resultEvent);
  stopTimer(TIMER_READ_FROM_DEVICE);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }
}

uint8_t *readResultWithMap(DfcOpenClBuffers *mem, cl_command_queue queue,
                           int readCount) {
  cl_int status;
  bool blocking = OVERLAPPING_EXECUTION ? 0 : CL_BLOCKING;

  startTimer(TIMER_READ_FROM_DEVICE);
  uint8_t *output = clEnqueueMapBuffer(
      queue, mem->result, blocking, CL_MAP_READ, 0,
      sizeInBytesOfResultVector(readCount), 0, NULL, &resultEvent, &status);
  stopTimer(TIMER_READ_FROM_DEVICE);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  return output;
}

// make sure to clean output later
void readResult(DfcOpenClBuffers *mem, cl_command_queue queue, int readCount,
                uint8_t **output) {
  if (MAP_MEMORY) {
    *output = readResultWithMap(mem, queue, readCount);
  } else {
    if (*output == NULL) {
      *output = calloc(1, sizeInBytesOfResultVector(INPUT_READ_CHUNK_BYTES));
    }
    readResultWithoutMap(mem, queue, readCount, *output);
  }
}

void cleanResult(cl_mem result, cl_command_queue queue, uint8_t **output) {
  if (MAP_MEMORY) {
    unmapOpenClBuffer(queue, *output, result);
  } else {
    free(*output);
    *output = NULL;
  }
}

int readResultAndCountMatches(uint8_t *input, DfcOpenClBuffers *mem,
                              cl_command_queue queue, DFC_PATTERNS *patterns,
                              int readCount, MatchFunction onMatch) {
  uint8_t *output = NULL;
  readResult(mem, queue, readCount, &output);
  int matches =
      handleResultsFromGpu(input, output, readCount, patterns, onMatch);
  cleanResult(mem->result, queue, &output);

  return matches;
}

void swapReadEvents() {
  cl_event tmp = resultEvent;
  resultEvent = resultEvent2;
  resultEvent2 = tmp;
}

cl_event getPrevReadEvent() { return resultEvent2; }

void waitForReadEvent(cl_event e) {
  startTimer(TIMER_READ_FROM_DEVICE);
  clWaitForEvents(1, &e);
  stopTimer(TIMER_READ_FROM_DEVICE);
}

void waitForWriteEvent(cl_event e) {
  startTimer(TIMER_WRITE_TO_DEVICE);
  clWaitForEvents(1, &e);
  stopTimer(TIMER_WRITE_TO_DEVICE);
}

int performSearch(ReadFunction read, MatchFunction onMatch) {
  char *input = getInputPtr();

  uint8_t *output = NULL;

  uint8_t *prev_output = NULL;
  char *prev_input = NULL;
  int prev_readCount = 0;

  int matches = 0;
  int readCount = 0;
  while (
      (readCount = read(INPUT_READ_CHUNK_BYTES, MAX_PATTERN_LENGTH, input))) {
    writeInputBufferToDevice(input, readCount);

    setKernelArgs(DFC_OPENCL_ENVIRONMENT.kernel, &DFC_OPENCL_BUFFERS,
                  readCount);
    startKernelForQueue(DFC_OPENCL_ENVIRONMENT.kernel,
                        DFC_OPENCL_ENVIRONMENT.queue, readCount);

    if (shouldUseOverlappingExecution()) {
      swapOpenClInputBuffers();
      char *new_input = getOwnershipOfInputBufferAsync(&inputEvent);

      if (prev_input && prev_output) {
        waitForReadEvent(getPrevReadEvent());
        matches += handleResultsFromGpu(
            (uint8_t *)prev_input, output, prev_readCount,
            DFC_HOST_MEMORY.dfcStructure->patterns, onMatch);
        cleanResult(DFC_OPENCL_BUFFERS.result2, DFC_OPENCL_ENVIRONMENT.queue,
                    &output);
      }

      readResult(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue, readCount,
                 &output);

      prev_output = output;
      prev_input = input;
      prev_readCount = readCount;

      swapOpenClResultBuffers();
      swapReadEvents();

      waitForWriteEvent(inputEvent);
      input = new_input;
    } else {
      matches += readResultAndCountMatches(
          (uint8_t *)input, &DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue,
          DFC_HOST_MEMORY.dfcStructure->patterns, readCount, onMatch);
      input = getOwnershipOfInputBuffer();
    }
  }

  // have to handle the last one too
  if (shouldUseOverlappingExecution()) {
    if (prev_input && prev_output) {
      matches +=
          handleResultsFromGpu((uint8_t *)prev_input, output, prev_readCount,
                               DFC_HOST_MEMORY.dfcStructure->patterns, onMatch);
      cleanResult(DFC_OPENCL_BUFFERS.result2, DFC_OPENCL_ENVIRONMENT.queue,
                  &output);
    }
  }

  leaveOwnershipOfInputPointer(DFC_OPENCL_BUFFERS.input, input);

  return matches;
}

int searchGpu(ReadFunction read, MatchFunction onMatch) {
  return performSearch(read, onMatch);
}

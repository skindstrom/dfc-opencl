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
#include "timer.h"

extern int exactMatchingUponFiltering(uint8_t *result, int length);

void setKernelArgsNormalDesign(cl_kernel kernel, DfcOpenClBuffers *mem) {
  clSetKernelArg(kernel, 0, sizeof(int), &mem->inputLength);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &mem->input);

  clSetKernelArg(kernel, 2, sizeof(cl_mem), &mem->patterns);

  clSetKernelArg(kernel, 3, sizeof(cl_mem), &mem->dfSmall);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &mem->dfLarge);
  clSetKernelArg(kernel, 5, sizeof(cl_mem), &mem->dfLargeHash);

  clSetKernelArg(kernel, 6, sizeof(cl_mem), &mem->ctSmall);
  clSetKernelArg(kernel, 7, sizeof(cl_mem), &mem->ctLarge);

  clSetKernelArg(kernel, 8, sizeof(cl_mem), &mem->result);
}

void setKernelArgsHetDesign(cl_kernel kernel, DfcOpenClBuffers *mem) {
  clSetKernelArg(kernel, 0, sizeof(int), &mem->inputLength);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &mem->input);

  clSetKernelArg(kernel, 2, sizeof(cl_mem), &mem->dfSmall);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), &mem->dfLarge);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &mem->dfLargeHash);

  clSetKernelArg(kernel, 5, sizeof(cl_mem), &mem->result);
}

void setKernelArgs(cl_kernel kernel, DfcOpenClBuffers *mem) {
  if (HETEROGENEOUS_DESIGN) {
    setKernelArgsHetDesign(kernel, mem);
  } else {
    setKernelArgsNormalDesign(kernel, mem);
  }
}

size_t getGlobalGroupSize(size_t localGroupSize, int inputLength) {
  const float threadCount = ceil(((float)inputLength) / CHECK_COUNT_PER_THREAD);

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
  clFinish(queue); // only necessary for timing
  stopTimer(TIMER_EXECUTE_KERNEL);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not start kernel: %d\n", status);
    exit(OPENCL_COULD_NOT_START_KERNEL);
  }
}

int sumMatches(uint8_t *result, int length) {
  int sum = 0;
  for (int i = 0; i < length; ++i) {
    sum += result[i];
  }
  return sum;
}

int handleResultsFromGpu(uint8_t* result, int length) {
  int matches;
  if (HETEROGENEOUS_DESIGN) {
    startTimer(TIMER_EXECUTE_HETEROGENEOUS);
    matches = exactMatchingUponFiltering(result, length);
    stopTimer(TIMER_EXECUTE_HETEROGENEOUS);
  } else {
    startTimer(TIMER_PROCESS_MATCHES);
    matches = sumMatches(result, length);
    stopTimer(TIMER_PROCESS_MATCHES);
  }

  return matches;
}

int readResultWithoutMap(DfcOpenClBuffers *mem, cl_command_queue queue) {
  uint8_t *output = calloc(1, mem->inputLength);

  startTimer(TIMER_READ_FROM_DEVICE);
  int status = clEnqueueReadBuffer(queue, mem->result, CL_BLOCKING, 0,
                                   mem->inputLength, output, 0, NULL, NULL);
  stopTimer(TIMER_READ_FROM_DEVICE);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  int matches = handleResultsFromGpu(output, mem->inputLength);

  free(output);

  return matches;
}

int readResultWithMap(DfcOpenClBuffers *mem, cl_command_queue queue) {
  cl_int status;

  startTimer(TIMER_READ_FROM_DEVICE);
  uint8_t *output = clEnqueueMapBuffer(
      queue, mem->result, CL_BLOCKING, CL_MAP_READ | CL_MAP_WRITE, 0,
      mem->inputLength, 0, NULL, NULL, &status);
  stopTimer(TIMER_READ_FROM_DEVICE);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  int matches = handleResultsFromGpu(output, mem->inputLength);

  status = clEnqueueUnmapMemObject(queue, mem->result, output, 0, NULL, NULL);

  return matches;
}

int readResult(DfcOpenClBuffers *mem, cl_command_queue queue) {
  if (MAP_MEMORY) {
    return readResultWithMap(mem, queue);
  } else {
    return readResultWithoutMap(mem, queue);
  }
}

int searchGpu() {
  prepareOpenClBuffersForSearch();

  setKernelArgs(DFC_OPENCL_ENVIRONMENT.kernel, &DFC_OPENCL_BUFFERS);
  startKernelForQueue(DFC_OPENCL_ENVIRONMENT.kernel,
                      DFC_OPENCL_ENVIRONMENT.queue,
                      DFC_HOST_MEMORY.inputLength);

  int matches = readResult(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue);

  freeOpenClBuffers();

  return matches;
}
#ifndef DFC_SEARCH_CPU_H
#define DFC_SEARCH_CPU_H

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

const int BLOCKING = 1;

#if !MAP_MEMORY
DfcOpenClMemory createOpenClBuffers(DfcOpenClEnvironment *environment,
                                    DFC_PATTERNS *dfcPatterns,
                                    int inputLength) {
  cl_context context = environment->context;
  cl_mem kernelInput =
      clCreateBuffer(context, CL_MEM_READ_ONLY, inputLength, NULL, NULL);
  cl_mem patterns = clCreateBuffer(
      context, CL_MEM_READ_ONLY,
      sizeof(DFC_FIXED_PATTERN) * dfcPatterns->numPatterns, NULL, NULL);

  cl_mem dfcStructure = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                       sizeof(DFC_STRUCTURE), NULL, NULL);
  cl_mem result =
      clCreateBuffer(context, CL_MEM_READ_WRITE, inputLength, NULL, NULL);

  DfcOpenClMemory memory = {.inputLength = inputLength,
                            .input = kernelInput,
                            .patterns = patterns,
                            .dfcStructure = dfcStructure,
                            .result = result};

  return memory;
}

void writeOpenClBuffers(DfcOpenClMemory *deviceMemory, cl_command_queue queue,
                        DfcHostMemory *hostMemory) {
  clEnqueueWriteBuffer(queue, deviceMemory->input, BLOCKING, 0,
                       deviceMemory->inputLength, hostMemory->input, 0, NULL,
                       NULL);
  clEnqueueWriteBuffer(
      queue, deviceMemory->patterns, BLOCKING, 0,
      sizeof(DFC_FIXED_PATTERN) * hostMemory->patterns->numPatterns,
      hostMemory->patterns->dfcMatchList, 0, NULL, NULL);
  clEnqueueWriteBuffer(queue, deviceMemory->dfcStructure, BLOCKING, 0,
                       sizeof(DFC_STRUCTURE), hostMemory->dfcStructure, 0, NULL,
                       NULL);
}
#else
cl_mem createMappedResultBuffer(cl_context context, int inputLength) {
  return clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                        inputLength, NULL, NULL);
}
#endif

void freeOpenClMemory(DfcOpenClMemory *mem) {
  clReleaseMemObject(mem->input);
  clReleaseMemObject(mem->patterns);
  clReleaseMemObject(mem->dfcStructure);
  clReleaseMemObject(mem->result);
}

void setKernelArgs(cl_kernel kernel, DfcOpenClMemory *mem) {
  clSetKernelArg(kernel, 0, sizeof(int), &mem->inputLength);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &mem->input);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), &mem->patterns);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), &mem->dfcStructure);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &mem->result);
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

  int status = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalGroupSize,
                                      &localGroupSize, 0, NULL, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not start kernel: %d\n", status);
    exit(OPENCL_COULD_NOT_START_KERNEL);
  }
}

#if !MAP_MEMORY
int readResult(DfcOpenClMemory *mem, cl_command_queue queue) {
  uint8_t *output = calloc(1, mem->inputLength);
  int status = clEnqueueReadBuffer(queue, mem->result, BLOCKING, 0,
                                   mem->inputLength, output, 0, NULL, NULL);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  int matches = 0;
  for (int i = 0; i < mem->inputLength; ++i) {
    matches += output[i];
  }

  free(output);

  return matches;
}
#else
int readResult(DfcOpenClMemory *mem, cl_command_queue queue) {
  cl_int status;
  uint8_t *output = clEnqueueMapBuffer(
      DFC_OPENCL_ENVIRONMENT.queue, DFC_OPENCL_BUFFERS.result, BLOCKING,
      CL_MAP_READ | CL_MAP_WRITE, 0, mem->inputLength, 0, NULL, NULL, &status);

  if (status != CL_SUCCESS) {
    free(output);
    fprintf(stderr, "Could not read result: %d\n", status);
    exit(OPENCL_COULD_NOT_READ_RESULTS);
  }

  int matches = 0;
  for (int i = 0; i < mem->inputLength; ++i) {
    matches += output[i];
  }

  status = clEnqueueUnmapMemObject(queue, mem->result, output, 0, NULL, NULL);

  return matches;
}
#endif

int search() {
#if !MAP_MEMORY
  DFC_OPENCL_BUFFERS =
      createOpenClBuffers(&DFC_OPENCL_ENVIRONMENT, DFC_HOST_MEMORY.patterns,
                          DFC_HOST_MEMORY.inputLength);
  writeOpenClBuffers(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue,
                     &DFC_HOST_MEMORY);
#else
  unmapOpenClInputBuffers();
  DFC_OPENCL_BUFFERS.result = createMappedResultBuffer(
      DFC_OPENCL_ENVIRONMENT.context, DFC_OPENCL_BUFFERS.inputLength);
#endif

  setKernelArgs(DFC_OPENCL_ENVIRONMENT.kernel, &DFC_OPENCL_BUFFERS);
  startKernelForQueue(DFC_OPENCL_ENVIRONMENT.kernel,
                      DFC_OPENCL_ENVIRONMENT.queue,
                      DFC_HOST_MEMORY.inputLength);

  int matches = readResult(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue);

  freeOpenClMemory(&DFC_OPENCL_BUFFERS);

  return matches;
}

#endif

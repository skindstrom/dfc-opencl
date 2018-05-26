#ifndef DFC_MEMORY_H
#define DFC_MEMORY_H

#include "dfc.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

typedef struct {
  cl_mem input;

  cl_mem patterns;

  cl_mem dfSmall;
  cl_mem dfLarge;
  cl_mem dfLargeHash;

  cl_mem ctSmallEntries;
  cl_mem ctSmallPids;

  cl_mem ctLargeBuckets;
  cl_mem ctLargeEntries;
  cl_mem ctLargePids;

  cl_mem result;
} DfcOpenClBuffers;

typedef struct {
  cl_platform_id platform;
  cl_device_id device;
  cl_context context;
  cl_program program;
  cl_kernel kernel;
  cl_command_queue queue;
} DfcOpenClEnvironment;

extern DfcOpenClBuffers DFC_OPENCL_BUFFERS;
extern DfcOpenClEnvironment DFC_OPENCL_ENVIRONMENT;

void unmapOpenClInputBuffers();

typedef struct {
  char *input;
  DFC_STRUCTURE *dfcStructure;
} DfcHostMemory;

typedef struct {
  int patternCount;

  int ctSmallPidCount;

  int ctLargeEntryCount;
  int ctLargePidCount;
} DfcMemoryRequirements;

extern DfcHostMemory DFC_HOST_MEMORY;

void setupExecutionEnvironment();
void releaseExecutionEnvironment();

void allocateDfcStructure(DfcMemoryRequirements requirements);
char *allocateInput(int size);
char *getInputPtr();

bool shouldUseOpenCl();

void freeDfcStructure();
void freeDfcInput();

void prepareOpenClBuffersForSearch();
void freeOpenClBuffers();

int sizeInBytesOfResultVector(int inputLength);

char *getOwnershipOfInputBuffer();
void writeInputBufferToDevice(char *buffer, int count);

#endif

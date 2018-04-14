#ifndef DFC_MEMORY_H
#define DFC_MEMORY_H

#include "dfc.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

typedef struct {
  int inputLength;
  cl_mem input;
  cl_mem patterns;
  cl_mem dfcStructure;
  cl_mem result;
} DfcOpenClMemory;

typedef struct {
  cl_platform_id platform;
  cl_device_id device;
  cl_context context;
  cl_program program;
  cl_kernel kernel;
  cl_command_queue queue;
} DfcOpenClEnvironment;

extern DfcOpenClMemory DFC_OPENCL_BUFFERS;
extern DfcOpenClEnvironment DFC_OPENCL_ENVIRONMENT;

void unmapOpenClInputBuffers();

typedef struct {
  int inputLength;
  char *input;
  DFC_STRUCTURE *dfcStructure;
  DFC_PATTERNS *patterns;
} DfcHostMemory;

extern DfcHostMemory DFC_HOST_MEMORY;

void setupExecutionEnvironment();
void releaseExecutionEnvironment();

void allocateDfcStructure();
void allocateDfcPatterns(int numPatterns);
void allocateInput(int size);

void freeDfcStructure();
void freeDfcPatterns();
void freeDfcInput();

#endif
#ifndef DFC_SEARCH_CPU_H
#define DFC_SEARCH_CPU_H


#include "math.h"
#include "stdio.h"
#include "stdlib.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "search.h"

const int BLOCKING = 1;

typedef struct {
  int inputLength;
  cl_mem input;
  cl_mem patterns;
  cl_mem dfSmall;
  cl_mem ctSmall;
  cl_mem dfLarge;
  cl_mem dfLargeHash;
  cl_mem ctLarge;
  cl_mem result;
} DfcOpenClMemory;

typedef struct {
  cl_platform_id platform;
  cl_device_id device;
  cl_context context;
  cl_program program;
  cl_kernel kernel;
} DfcOpenClEnvironment;

cl_platform_id getPlatform() {
  cl_platform_id id;
  int amountOfPlatformsToReturn = 1;
  int status = clGetPlatformIDs(amountOfPlatformsToReturn, &id, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not get platform");
    exit(OPENCL_NO_PLATFORM_EXIT_CODE);
  }

  return id;
}

cl_device_id getDevice(cl_platform_id platform) {
  cl_device_id id;
  int amountOfDevicesToReturn = 1;
  int status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_DEFAULT,
                              amountOfDevicesToReturn, &id, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not get device");
    exit(OPENCL_NO_DEVICE_EXIT_CODE);
  }

  return id;
}

cl_context getContext(cl_device_id device) {
  cl_int status;
  int amountOfDevices = 1;
  cl_context context =
      clCreateContext(NULL, amountOfDevices, &device, NULL, NULL, &status);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not create context for reason: %i", status);
    exit(OPENCL_COULD_NOT_CREATE_CONTEXT);
  }

  return context;
}

cl_program loadAndCreateProgram(cl_context context) {
  // Currently expecting the PWD to be "build"
  FILE *fp = fopen("../src/search/kernel.cl", "r");
  if (!fp) {
    fprintf(stderr, "Failed to load kernel.\n");
    exit(COULD_NOT_LOAD_KERNEL);
  }

  fseek(fp, 0L, SEEK_END);
  size_t fileLength = ftell(fp);
  rewind(fp);

  char *source_str = (char *)malloc(fileLength);
  size_t source_size = fread(source_str, 1, fileLength, fp);
  fclose(fp);

  cl_int status;
  cl_program program = clCreateProgramWithSource(
      context, 1, (const char **)&source_str, &source_size, &status);

  free(source_str);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not create program for reason %i", status);
    exit(OPENCL_COULD_NOT_CREATE_PROGRAM);
  }

  return program;
}

void buildProgram(cl_program *program, cl_device_id device) {
  cl_int status =
      clBuildProgram(*program, 1, &device, "-cl-std=CL1.2", NULL, NULL);

  if (status != CL_SUCCESS) {
    size_t log_size;
    clGetProgramBuildInfo(*program, device, CL_PROGRAM_BUILD_LOG, 0, NULL,
                          &log_size);

    char *log = (char *)malloc(log_size);
    clGetProgramBuildInfo(*program, device, CL_PROGRAM_BUILD_LOG, log_size, log,
                          NULL);

    fprintf(stderr, "Could not compile program for reason %i\n", status);
    fprintf(stderr, "%s\n", log);

    free(log);
    exit(OPENCL_COULD_NOT_BUILD_PROGRAM);
  }
}

cl_kernel createKernel(cl_program *program) {
  cl_int status;
  cl_kernel kernel = clCreateKernel(*program, "search", &status);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not create kernel for reason %i", status);
    exit(OPENCL_COULD_NOT_CREATE_KERNEL);
  }

  return kernel;
}

DfcOpenClEnvironment setupEnvironment() {
  cl_platform_id platform = getPlatform();
  cl_device_id device = getDevice(platform);
  cl_context context = getContext(device);
  cl_program program = loadAndCreateProgram(context);
  buildProgram(&program, device);
  cl_kernel kernel = createKernel(&program);

  DfcOpenClEnvironment env = {
      .platform = platform,
      .device = device,
      .context = context,
      .program = program,
      .kernel = kernel,
  };

  return env;
}

void releaseEnvironment(DfcOpenClEnvironment *environment) {
  clReleaseKernel(environment->kernel);
  clReleaseProgram(environment->program);
  clReleaseContext(environment->context);
}

cl_command_queue createCommandQueue(DfcOpenClEnvironment *env) {
  return clCreateCommandQueue(env->context, env->device, 0, NULL);
}

DfcOpenClMemory createMemory(DfcOpenClEnvironment *environment,
                             DFC_STRUCTURE *dfc, int inputLength) {
  cl_context context = environment->context;
  cl_mem kernelInput =
      clCreateBuffer(context, CL_MEM_READ_ONLY, inputLength, NULL, NULL);
  cl_mem patterns =
      clCreateBuffer(context, CL_MEM_READ_ONLY,
                     sizeof(DFC_FIXED_PATTERN) * dfc->numPatterns, NULL, NULL);
  cl_mem dfSmall = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                  sizeof(dfc->directFilterSmall), NULL, NULL);
  cl_mem ctSmall = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                  sizeof(dfc->compactTableSmall), NULL, NULL);
  cl_mem dfLarge = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                  sizeof(dfc->directFilterLarge), NULL, NULL);
  cl_mem dfLargeHash =
      clCreateBuffer(context, CL_MEM_READ_ONLY,
                     sizeof(dfc->directFilterLargeHash), NULL, NULL);
  cl_mem ctLarge = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                  sizeof(dfc->compactTableLarge), NULL, NULL);
  cl_mem result =
      clCreateBuffer(context, CL_MEM_READ_WRITE, inputLength, NULL, NULL);

  DfcOpenClMemory memory = {.input = kernelInput,
                            .patterns = patterns,
                            .dfSmall = dfSmall,
                            .ctSmall = ctSmall,
                            .dfLarge = dfLarge,
                            .dfLargeHash = dfLargeHash,
                            .ctLarge = ctLarge,
                            .result = result};

  return memory;
}

void writeMemory(DfcOpenClMemory *memory, cl_command_queue queue,
                 DFC_STRUCTURE *dfc, uint8_t *input, int inputLength) {
  memory->inputLength = inputLength;
  clEnqueueWriteBuffer(queue, memory->input, BLOCKING, 0, inputLength, input, 0,
                       NULL, NULL);
  clEnqueueWriteBuffer(queue, memory->patterns, BLOCKING, 0,
                       sizeof(DFC_FIXED_PATTERN) * dfc->numPatterns,
                       dfc->dfcMatchList, 0, NULL, NULL);
  clEnqueueWriteBuffer(queue, memory->dfSmall, BLOCKING, 0,
                       sizeof(dfc->directFilterSmall), dfc->directFilterSmall,
                       0, NULL, NULL);
  clEnqueueWriteBuffer(queue, memory->ctSmall, BLOCKING, 0,
                       sizeof(dfc->compactTableSmall), dfc->compactTableSmall,
                       0, NULL, NULL);
  clEnqueueWriteBuffer(queue, memory->dfLarge, BLOCKING, 0,
                       sizeof(dfc->directFilterLarge), dfc->directFilterLarge,
                       0, NULL, NULL);
  clEnqueueWriteBuffer(queue, memory->dfLargeHash, BLOCKING, 0,
                       sizeof(dfc->directFilterLargeHash),
                       dfc->directFilterLargeHash, 0, NULL, NULL);
  clEnqueueWriteBuffer(queue, memory->ctLarge, BLOCKING, 0,
                       sizeof(dfc->compactTableLarge), dfc->compactTableLarge,
                       0, NULL, NULL);
}

void freeMemory(DfcOpenClMemory *mem) {
  clReleaseMemObject(mem->input);
  clReleaseMemObject(mem->patterns);
  clReleaseMemObject(mem->dfSmall);
  clReleaseMemObject(mem->ctSmall);
  clReleaseMemObject(mem->dfLarge);
  clReleaseMemObject(mem->dfLargeHash);
  clReleaseMemObject(mem->ctLarge);
  clReleaseMemObject(mem->result);
}

void setKernelArgs(cl_kernel kernel, DfcOpenClMemory *mem) {
  clSetKernelArg(kernel, 0, sizeof(int), &mem->inputLength);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &mem->input);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), &mem->patterns);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), &mem->dfSmall);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &mem->ctSmall);
  clSetKernelArg(kernel, 5, sizeof(cl_mem), &mem->dfLarge);
  clSetKernelArg(kernel, 6, sizeof(cl_mem), &mem->dfLargeHash);
  clSetKernelArg(kernel, 7, sizeof(cl_mem), &mem->ctLarge);
  clSetKernelArg(kernel, 8, sizeof(cl_mem), &mem->result);
}

void startKernelForQueue(cl_kernel kernel, cl_command_queue queue,
                         int inputLength) {
  const size_t localGroupSize = WORK_GROUP_SIZE;
  const size_t globalGroupSize =
      ceil((float)inputLength / localGroupSize) * localGroupSize;

  int status = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalGroupSize,
                                      &localGroupSize, 0, NULL, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "Could not start kernel: %d\n", status);
    exit(OPENCL_COULD_NOT_START_KERNEL);
  }
}

int readResult(DfcOpenClMemory *mem, cl_command_queue queue) {
  uint8_t *output = malloc(mem->inputLength);
  for (int i = 0; i < mem->inputLength; ++i) {
    output[i] = 0;
  }
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

int search(DFC_STRUCTURE *dfc, uint8_t *input, int inputLength) {
  DfcOpenClEnvironment env = setupEnvironment();
  cl_command_queue queue = createCommandQueue(&env);
  DfcOpenClMemory mem = createMemory(&env, dfc, inputLength);
  writeMemory(&mem, queue, dfc, input, inputLength);

  setKernelArgs(env.kernel, &mem);
  startKernelForQueue(env.kernel, queue, inputLength);

  int matches = readResult(&mem, queue);

  clFlush(queue);
  clFinish(queue);
  freeMemory(&mem);
  clReleaseCommandQueue(queue);
  releaseEnvironment(&env);

  return matches;
}

#endif

#include "memory.h"

DfcHostMemory DFC_HOST_MEMORY;

DfcOpenClMemory DFC_OPENCL_BUFFERS;
DfcOpenClEnvironment DFC_OPENCL_ENVIRONMENT;

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
  char arguments[200];
  sprintf(arguments,
          "-cl-std=CL1.2 -D CHECK_COUNT_PER_THREAD=%d -D DFC_OPENCL -I ../src",
          CHECK_COUNT_PER_THREAD);
  cl_int status = clBuildProgram(*program, 1, &device, arguments, NULL, NULL);

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

cl_command_queue createCommandQueue(cl_context context, cl_device_id device) {
  return clCreateCommandQueue(context, device, 0, NULL);
}

DfcOpenClEnvironment setupOpenClEnvironment() {
  cl_platform_id platform = getPlatform();
  cl_device_id device = getDevice(platform);
  cl_context context = getContext(device);
  cl_program program = loadAndCreateProgram(context);
  buildProgram(&program, device);
  cl_kernel kernel = createKernel(&program);
  cl_command_queue queue = createCommandQueue(context, device);

  DfcOpenClEnvironment env = {.platform = platform,
                              .device = device,
                              .context = context,
                              .program = program,
                              .kernel = kernel,
                              .queue = queue};

  return env;
}

void releaseOpenClEnvironment(DfcOpenClEnvironment *environment) {
  clReleaseKernel(environment->kernel);
  clReleaseProgram(environment->program);
  clReleaseContext(environment->context);
}

void setupExecutionEnvironment() {
  if (SEARCH_WITH_GPU) {
    DFC_OPENCL_ENVIRONMENT = setupOpenClEnvironment();
  }
}
void releaseExecutionEnvironment() {
  if (SEARCH_WITH_GPU) {
    releaseOpenClEnvironment(&DFC_OPENCL_ENVIRONMENT);
  }
}

void allocateDfcStructureWithMap() {
  cl_int errcode;

  DFC_OPENCL_BUFFERS.dfcStructure = clCreateBuffer(
      DFC_OPENCL_ENVIRONMENT.context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
      sizeof(DFC_STRUCTURE), NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create mapped DFC buffer");
    exit(OPENCL_COULD_NOT_CREATE_MAPPED_DFC_BUFFER);
  }

  DFC_HOST_MEMORY.dfcStructure = clEnqueueMapBuffer(
      DFC_OPENCL_ENVIRONMENT.queue, DFC_OPENCL_BUFFERS.dfcStructure,
      CL_BLOCKING, CL_MAP_READ | CL_MAP_WRITE, 0, sizeof(DFC_STRUCTURE), 0,
      NULL, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map DFC buffer to host memory");
    exit(OPENCL_COULD_NOT_MAP_DFC_TO_HOST);
  }

  memset(DFC_HOST_MEMORY.dfcStructure, 0, sizeof(DFC_STRUCTURE));
}

void allocateDfcPatternsWithMap(int numPatterns) {
  const size_t size = sizeof(DFC_FIXED_PATTERN) * numPatterns;
  cl_int errcode;

  DFC_OPENCL_BUFFERS.patterns = clCreateBuffer(
      DFC_OPENCL_ENVIRONMENT.context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
      size, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create mapped pattern buffer");
    exit(OPENCL_COULD_NOT_CREATE_MAPPED_PATTERN_BUFFER);
  }

  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  patterns->dfcMatchList = clEnqueueMapBuffer(
      DFC_OPENCL_ENVIRONMENT.queue, DFC_OPENCL_BUFFERS.patterns, CL_BLOCKING,
      CL_MAP_READ | CL_MAP_WRITE, 0, size, 0, NULL, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map pattern buffer to host memory");
    exit(OPENCL_COULD_NOT_MAP_PATTERN_TO_HOST);
  }

  DFC_HOST_MEMORY.patterns = patterns;
}

void allocateInputWithMap(int size) {
  DFC_HOST_MEMORY.inputLength = size;
  DFC_OPENCL_BUFFERS.inputLength = size;

  cl_int errcode;

  DFC_OPENCL_BUFFERS.input = clCreateBuffer(
      DFC_OPENCL_ENVIRONMENT.context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
      size, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create mapped input buffer");
    exit(OPENCL_COULD_NOT_CREATE_MAPPED_INPUT_BUFFER);
  }

  DFC_HOST_MEMORY.input = clEnqueueMapBuffer(
      DFC_OPENCL_ENVIRONMENT.queue, DFC_OPENCL_BUFFERS.input, CL_BLOCKING,
      CL_MAP_READ | CL_MAP_WRITE, 0, size, 0, NULL, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map input buffer to host memory");
    exit(OPENCL_COULD_NOT_MAP_INPUT_TO_HOST);
  }
}

void unmapOpenClInputBuffers() {
  cl_int errcode = clEnqueueUnmapMemObject(
      DFC_OPENCL_ENVIRONMENT.queue, DFC_OPENCL_BUFFERS.dfcStructure,
      DFC_HOST_MEMORY.dfcStructure, 0, NULL, NULL);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not unmap DFC");
    exit(OPENCL_COULD_NOT_UNMAP_DFC);
  }

  errcode = clEnqueueUnmapMemObject(
      DFC_OPENCL_ENVIRONMENT.queue, DFC_OPENCL_BUFFERS.patterns,
      DFC_HOST_MEMORY.patterns->dfcMatchList, 0, NULL, NULL);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not unmap patterns: %d", errcode);
    exit(OPENCL_COULD_NOT_UNMAP_PATTERNS);
  }

  errcode = clEnqueueUnmapMemObject(DFC_OPENCL_ENVIRONMENT.queue,
                                    DFC_OPENCL_BUFFERS.input,
                                    DFC_HOST_MEMORY.input, 0, NULL, NULL);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not unmap input");
    exit(OPENCL_COULD_NOT_UNMAP_INPUT);
  }
}

void allocateDfcStructureOnHost() {
  DFC_HOST_MEMORY.dfcStructure = calloc(1, sizeof(DFC_STRUCTURE));
}

void allocateDfcPatternsOnHost(int numPatterns) {
  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  patterns->dfcMatchList = calloc(1, sizeof(DFC_FIXED_PATTERN) * numPatterns);

  DFC_HOST_MEMORY.patterns = patterns;
}

void allocateInputOnHost(int size) {
  DFC_HOST_MEMORY.inputLength = size;
  DFC_HOST_MEMORY.input = calloc(1, size);
}

void freeDfcStructureOnHost() { free(DFC_HOST_MEMORY.dfcStructure); }

void freeDfcPatternsOnHost() {
  free(DFC_HOST_MEMORY.patterns->dfcMatchList);
  free(DFC_HOST_MEMORY.patterns);
}

void freeDfcInputOnHost() {
  DFC_HOST_MEMORY.inputLength = 0;
  free(DFC_HOST_MEMORY.input);
}

bool shouldUseMappedMemory() { return MAP_MEMORY && SEARCH_WITH_GPU; }

void allocateDfcStructure() {
  if (shouldUseMappedMemory()) {
    allocateDfcStructureWithMap();
  } else {
    allocateDfcStructureOnHost();
  }
}

void allocateDfcPatterns(int numPatterns) {
  if (shouldUseMappedMemory()) {
    allocateDfcPatternsWithMap(numPatterns);
  } else {
    allocateDfcPatternsOnHost(numPatterns);
  }
}

void allocateInput(int size) {
  if (shouldUseMappedMemory()) {
    allocateInputWithMap(size);
  } else {
    allocateInputOnHost(size);
  }
}

void freeDfcStructure() {
  if (!shouldUseMappedMemory()) {
    freeDfcStructureOnHost();
  }
}

void freeDfcPatterns() {
  if (!shouldUseMappedMemory()) {
    freeDfcPatternsOnHost();
  }
}

void freeDfcInput() {
  if (!shouldUseMappedMemory()) {
    freeDfcInputOnHost();
  }
}
#include "memory.h"

DfcHostMemory DFC_HOST_MEMORY;

#if SEARCH_WITH_GPU

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

DfcOpenClEnvironment setupOpenClEnvironment() {
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

void releaseOpenClEnvironment(DfcOpenClEnvironment *environment) {
  clReleaseKernel(environment->kernel);
  clReleaseProgram(environment->program);
  clReleaseContext(environment->context);
}

void setupExecutionEnvironment() {
  DFC_OPENCL_ENVIRONMENT = setupOpenClEnvironment();
}
void releaseExecutionEnvironment() {
  releaseOpenClEnvironment(&DFC_OPENCL_ENVIRONMENT);
}

#else

void setupExecutionEnvironment() {}
void releaseExecutionEnvironment() {}

#endif

#if MAP_MEMORY
DFC_STRUCTURE *allocateDfcStructure() {}

DFC_PATTERNS *allocateDfcPatterns(int numPatterns);

void freeDfcStructure();
void freeDfcPatterns();

#else

void allocateDfcStructure() {
  DFC_HOST_MEMORY.dfcStructure = calloc(1, sizeof(DFC_STRUCTURE));
}

void allocateDfcPatterns(int numPatterns) {
  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  patterns->dfcMatchList = calloc(1, sizeof(DFC_FIXED_PATTERN) * numPatterns);

  DFC_HOST_MEMORY.patterns = patterns;
}

void allocateInput(int size) {
  DFC_HOST_MEMORY.inputLength = size;
  DFC_HOST_MEMORY.input = calloc(1, size);
}

void freeDfcStructure() { free(DFC_HOST_MEMORY.dfcStructure); }

void freeDfcPatterns() {
  free(DFC_HOST_MEMORY.patterns->dfcMatchList);
  free(DFC_HOST_MEMORY.patterns);
}

void freeDfcInput() {
  DFC_HOST_MEMORY.inputLength = 0;
  free(DFC_HOST_MEMORY.input);
}

#endif
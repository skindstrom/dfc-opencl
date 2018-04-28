#include "memory.h"

#include "timer.h"

DfcHostMemory DFC_HOST_MEMORY;
DfcMemoryRequirements DFC_MEMORY_REQUIREMENTS;

DfcOpenClBuffers DFC_OPENCL_BUFFERS;
DfcOpenClEnvironment DFC_OPENCL_ENVIRONMENT;

int sizeInBytesOfResultVector(int inputLength) {
  if (HETEROGENEOUS_DESIGN) {
    return inputLength - 1;
  }

  return (inputLength - 1) * sizeof(VerifyResult);
}

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
  char arguments[400];
  sprintf(arguments,
          "-cl-std=CL1.2 "
          "-D THREAD_GRANULARITY=%d "
          "-D LOCAL_MEMORY_LOAD_PER_ITEM=%d "
          "-D DFC_OPENCL "
          "-I ../src",
          THREAD_GRANULARITY, DF_SIZE_REAL / WORK_GROUP_SIZE);
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

void setKernelName(char *name) {
  if (HETEROGENEOUS_DESIGN && USE_TEXTURE_MEMORY) {
    strcpy(name, "filter_with_image");
  } else if (HETEROGENEOUS_DESIGN && USE_LOCAL_MEMORY) {
    strcpy(name, "filter_with_local");
  } else if (HETEROGENEOUS_DESIGN) {
    strcpy(name, "filter");
  } else if (VECTORIZE_KERNEL) {
    strcpy(name, "search_vec");
  } else if (USE_TEXTURE_MEMORY) {
    strcpy(name, "search_with_image");
  } else if (USE_LOCAL_MEMORY) {
    strcpy(name, "search_with_local");
  } else {
    strcpy(name, "search");
  }
}

cl_kernel createKernel(cl_program *program) {
  cl_int status;

  char kernelName[50];
  setKernelName(kernelName);
  cl_kernel kernel = clCreateKernel(*program, kernelName, &status);

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

bool shouldUseOpenCl() { return SEARCH_WITH_GPU || HETEROGENEOUS_DESIGN; }

void setupExecutionEnvironment() {
  startTimer(TIMER_ENVIRONMENT_SETUP);
  if (shouldUseOpenCl()) {
    DFC_OPENCL_ENVIRONMENT = setupOpenClEnvironment();
  }
  stopTimer(TIMER_ENVIRONMENT_SETUP);
}
void releaseExecutionEnvironment() {
  startTimer(TIMER_ENVIRONMENT_TEARDOWN);
  if (shouldUseOpenCl()) {
    releaseOpenClEnvironment(&DFC_OPENCL_ENVIRONMENT);
  }
  stopTimer(TIMER_ENVIRONMENT_TEARDOWN);
}

void mapBuffer(cl_command_queue queue, void **host, cl_mem buffer,
               size_t size) {
  cl_int errcode;
  startTimer(TIMER_WRITE_TO_DEVICE);
  *host =
      clEnqueueMapBuffer(queue, buffer, CL_BLOCKING, CL_MAP_READ | CL_MAP_WRITE,
                         0, size, 0, NULL, NULL, &errcode);
  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map DFC buffer to host memory: %d\n", errcode);
    exit(OPENCL_COULD_NOT_MAP_DFC_TO_HOST);
  }
}

void createBufferAndMap(cl_context context, cl_command_queue queue, void **host,
                        cl_mem *buffer, size_t size) {
  cl_int errcode;

  size = size == 0 ? 1 : size;

  startTimer(TIMER_WRITE_TO_DEVICE);

  *buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                           size, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create mapped DFC buffer: %d\n", errcode);
    exit(OPENCL_COULD_NOT_CREATE_MAPPED_DFC_BUFFER);
  }

  mapBuffer(queue, host, *buffer, size);

  memset(*host, 0, size);
}

void createTextureBufferAndMap(cl_context context, cl_command_queue queue,
                               void **host, cl_mem *buffer, int size) {
  cl_image_format imageFormat = {
      .image_channel_order = CL_RGBA,  // use all 4 bytes of channel
      /**
       * when reading image data, 128 bits (4 * 4 bytes) are always read
       * no way around it
       * Therefore, make sure to use as much data as possible
       */
      .image_channel_data_type = CL_UNSIGNED_INT32};
  cl_image_desc imageDescription = {
      .image_type = CL_MEM_OBJECT_IMAGE1D,
      .image_width =
          size / TEXTURE_CHANNEL_BYTE_SIZE,  // divide by size of channel
      .image_height = 1,
      .image_depth = 1,
      .image_array_size = 0,   // not used since we're not creating an array
      .image_row_pitch = 0,    // must be 0 if host_ptr is NULL
      .image_slice_pitch = 0,  // must be 0 if host_ptr is NULL
      .num_mip_levels = 0,     // must always be 0
      .num_samples = 0,        // must always be 0
      .buffer = NULL           // must be NULL since we're not using a buffer
  };

  cl_int errcode;

  startTimer(TIMER_WRITE_TO_DEVICE);

  *buffer = clCreateImage(context, CL_MEM_READ_ONLY, &imageFormat,
                          &imageDescription, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create mapped DFC buffer");
    exit(OPENCL_COULD_NOT_CREATE_MAPPED_DFC_BUFFER);
  }

  size_t offset[3] = {0, 0, 0};  // origin in OpenCL
  size_t imageSize[3] = {size / TEXTURE_CHANNEL_BYTE_SIZE, 1,
                         1};  // region in OpenCL
  size_t pitch = 0;           // if 0, OpenCL calculates a fitting pitch
  *host = clEnqueueMapImage(queue, *buffer, CL_BLOCKING,
                            CL_MAP_READ | CL_MAP_WRITE, offset, imageSize,
                            &pitch, &pitch, 0, NULL, NULL, &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map DFC texture buffer to host memory");
    exit(OPENCL_COULD_NOT_MAP_DFC_TO_HOST);
  }

  memset(*host, 0, size);
}

void allocateCompactTablesOnHost(DFC_STRUCTURE *dfc,
                                 DfcMemoryRequirements requirements) {
  dfc->ctSmallEntries =
      calloc(1, sizeof(CompactTableSmallEntry) * COMPACT_TABLE_SIZE_SMALL);
  if (!dfc->ctSmallEntries) {
    fprintf(stderr, "Could not allocate small CT\n");
    exit(1);
  }
  dfc->ctSmallPids = calloc(1, sizeof(PID_TYPE) * requirements.ctSmallPidCount);
  if (!dfc->ctSmallPids) {
    fprintf(stderr, "Could not allocate small CT pids\n");
    exit(1);
  }

  dfc->ctLargeBuckets =
      calloc(1, sizeof(CompactTableLargeBucket) * COMPACT_TABLE_SIZE_LARGE);
  if (!dfc->ctLargeBuckets) {
    fprintf(stderr, "Could not allocate large CT\n");
    exit(1);
  }

  dfc->ctLargeEntries = calloc(
      1, sizeof(CompactTableLargeEntry) * requirements.ctLargeEntryCount);
  if (!dfc->ctLargeEntries) {
    fprintf(stderr, "Could not allocate large CT entries\n");
    exit(1);
  }

  dfc->ctLargePids = calloc(1, sizeof(PID_TYPE) * requirements.ctLargePidCount);
  if (!dfc->ctLargeEntries) {
    fprintf(stderr, "Could not allocate large CT pids\n");
    exit(1);
  }
}

void allocateDfcStructureWithMap(DfcMemoryRequirements requirements) {
  cl_context context = DFC_OPENCL_ENVIRONMENT.context;
  cl_command_queue queue = DFC_OPENCL_ENVIRONMENT.queue;

  DFC_STRUCTURE *dfc = malloc(sizeof(DFC_STRUCTURE));

  if (USE_TEXTURE_MEMORY) {
    createTextureBufferAndMap(context, queue, (void *)&dfc->directFilterSmall,
                              &DFC_OPENCL_BUFFERS.dfSmall, DF_SIZE_REAL);
    createTextureBufferAndMap(context, queue, (void *)&dfc->directFilterLarge,
                              &DFC_OPENCL_BUFFERS.dfLarge, DF_SIZE_REAL);
  } else {
    createBufferAndMap(context, queue, (void *)&dfc->directFilterSmall,
                       &DFC_OPENCL_BUFFERS.dfSmall, DF_SIZE_REAL);
    createBufferAndMap(context, queue, (void *)&dfc->directFilterLarge,
                       &DFC_OPENCL_BUFFERS.dfLarge, DF_SIZE_REAL);
  }
  createBufferAndMap(context, queue, (void *)&dfc->directFilterLargeHash,
                     &DFC_OPENCL_BUFFERS.dfLargeHash, DF_SIZE_REAL);

  if (HETEROGENEOUS_DESIGN) {
    allocateCompactTablesOnHost(dfc, requirements);
  } else {
    createBufferAndMap(
        context, queue, (void *)&dfc->ctSmallEntries,
        &DFC_OPENCL_BUFFERS.ctSmallEntries,
        COMPACT_TABLE_SIZE_SMALL * sizeof(CompactTableSmallEntry));
    createBufferAndMap(context, queue, (void *)&dfc->ctSmallPids,
                       &DFC_OPENCL_BUFFERS.ctSmallPids,
                       requirements.ctSmallPidCount * sizeof(PID_TYPE));

    createBufferAndMap(
        context, queue, (void *)&dfc->ctLargeBuckets,
        &DFC_OPENCL_BUFFERS.ctLargeBuckets,
        COMPACT_TABLE_SIZE_LARGE * sizeof(CompactTableLargeBucket));
    createBufferAndMap(
        context, queue, (void *)&dfc->ctLargeEntries,
        &DFC_OPENCL_BUFFERS.ctLargeEntries,
        requirements.ctLargeEntryCount * sizeof(CompactTableLargeEntry));
    createBufferAndMap(context, queue, (void *)&dfc->ctLargePids,
                       &DFC_OPENCL_BUFFERS.ctLargePids,
                       requirements.ctLargePidCount * sizeof(PID_TYPE));
  }

  DFC_HOST_MEMORY.dfcStructure = dfc;
}

void allocateDfcPatternsWithMap(int numPatterns) {
  cl_context context = DFC_OPENCL_ENVIRONMENT.context;
  cl_command_queue queue = DFC_OPENCL_ENVIRONMENT.queue;

  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  createBufferAndMap(context, queue, (void *)&patterns->dfcMatchList,
                     &DFC_OPENCL_BUFFERS.patterns,
                     sizeof(DFC_FIXED_PATTERN) * numPatterns);

  DFC_HOST_MEMORY.dfcStructure->patterns = patterns;
}

void allocateInputWithMap(int size) {
  cl_context context = DFC_OPENCL_ENVIRONMENT.context;
  cl_command_queue queue = DFC_OPENCL_ENVIRONMENT.queue;

  createBufferAndMap(context, queue, (void *)&DFC_HOST_MEMORY.input,
                     &DFC_OPENCL_BUFFERS.input, size);
}

void unmapOpenClBuffer(cl_command_queue queue, void *host, cl_mem buffer) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_int errcode = clEnqueueUnmapMemObject(queue, buffer, host, 0, NULL, NULL);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not unmap DFC");
    exit(OPENCL_COULD_NOT_UNMAP_DFC);
  }
}

void unmapOpenClInputBuffers() {
  cl_command_queue queue = DFC_OPENCL_ENVIRONMENT.queue;
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;
  DfcOpenClBuffers *buffers = &DFC_OPENCL_BUFFERS;

  unmapOpenClBuffer(queue, dfc->directFilterSmall, buffers->dfSmall);
  unmapOpenClBuffer(queue, dfc->directFilterLarge, buffers->dfLarge);
  unmapOpenClBuffer(queue, dfc->directFilterLargeHash, buffers->dfLargeHash);

  if (!HETEROGENEOUS_DESIGN) {
    unmapOpenClBuffer(queue, dfc->ctSmallEntries, buffers->ctSmallEntries);
    unmapOpenClBuffer(queue, dfc->ctSmallPids, buffers->ctSmallPids);

    unmapOpenClBuffer(queue, dfc->ctLargeBuckets, buffers->ctLargeBuckets);
    unmapOpenClBuffer(queue, dfc->ctLargeEntries, buffers->ctLargeEntries);
    unmapOpenClBuffer(queue, dfc->ctLargePids, buffers->ctLargePids);

    unmapOpenClBuffer(queue,
                      DFC_HOST_MEMORY.dfcStructure->patterns->dfcMatchList,
                      DFC_OPENCL_BUFFERS.patterns);
  }
}

void allocateDfcStructureOnHost(DfcMemoryRequirements requirements) {
  DFC_STRUCTURE *dfc = malloc(sizeof(DFC_STRUCTURE));

  dfc->directFilterSmall = calloc(1, DF_SIZE_REAL);
  dfc->directFilterLarge = calloc(1, DF_SIZE_REAL);
  dfc->directFilterLargeHash = calloc(1, DF_SIZE_REAL);

  allocateCompactTablesOnHost(dfc, requirements);

  DFC_HOST_MEMORY.dfcStructure = dfc;
}

void allocateDfcPatternsOnHost(int numPatterns) {
  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  patterns->dfcMatchList = calloc(1, sizeof(DFC_FIXED_PATTERN) * numPatterns);

  DFC_HOST_MEMORY.dfcStructure->patterns = patterns;
}

void allocateInputOnHost(int size) { DFC_HOST_MEMORY.input = calloc(1, size); }

void freeDfcStructureOnHost() {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;

  free(dfc->directFilterSmall);
  free(dfc->directFilterLarge);
  free(dfc->directFilterLargeHash);

  free(dfc->ctSmallEntries);
  free(dfc->ctSmallPids);

  free(dfc->ctLargeBuckets);
  free(dfc->ctLargeEntries);
  free(dfc->ctLargePids);

  free(dfc);

  DFC_HOST_MEMORY.dfcStructure = NULL;
}

void freeDfcStructureWithMap() {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;

  if (HETEROGENEOUS_DESIGN) {
    free(dfc->ctSmallEntries);
    free(dfc->ctSmallPids);

    free(dfc->ctLargeBuckets);
    free(dfc->ctLargeEntries);
    free(dfc->ctLargePids);
  }

  free(dfc);

  DFC_HOST_MEMORY.dfcStructure = NULL;
}

void freeDfcPatternsOnHost() {
  free(DFC_HOST_MEMORY.dfcStructure->patterns->dfcMatchList);
  free(DFC_HOST_MEMORY.dfcStructure->patterns);
}

void freeDfcInputOnHost() { free(DFC_HOST_MEMORY.input); }

bool shouldUseMappedMemory() { return MAP_MEMORY && shouldUseOpenCl(); }

bool shouldMapPatternMemory() {
  return shouldUseMappedMemory() && !HETEROGENEOUS_DESIGN;
}

void allocateDfcPatterns(int numPatterns) {
  if (shouldMapPatternMemory()) {
    allocateDfcPatternsWithMap(numPatterns);
  } else {
    allocateDfcPatternsOnHost(numPatterns);
  }
}

void allocateDfcStructure(DfcMemoryRequirements requirements) {
  DFC_MEMORY_REQUIREMENTS = requirements;

  if (shouldUseMappedMemory()) {
    allocateDfcStructureWithMap(requirements);
  } else {
    allocateDfcStructureOnHost(requirements);
  }

  allocateDfcPatterns(requirements.patternCount);
}

char *allocateInput(int size) {
  if (shouldUseMappedMemory()) {
    allocateInputWithMap(size);
  } else {
    allocateInputOnHost(size);
  }

  return DFC_HOST_MEMORY.input;
}

void freeDfcPatterns() {
  if (!shouldMapPatternMemory()) {
    freeDfcPatternsOnHost();
  }
}

void freeDfcStructure() {
  freeDfcPatterns();

  if (shouldUseMappedMemory()) {
    freeDfcStructureWithMap();
  } else {
    freeDfcStructureOnHost();
  }
}

void freeDfcInput() {
  if (!shouldUseMappedMemory()) {
    freeDfcInputOnHost();
  }
}

cl_mem createReadOnlyBuffer(cl_context context, size_t size) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  // size may not be 0
  size = size == 0 ? 1 : size;

  cl_int errcode;
  cl_mem buffer =
      clCreateBuffer(context, CL_MEM_READ_ONLY, size, NULL, &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create read only buffer %d\n", errcode);
    exit(1);
  }

  return buffer;
}

cl_mem createReadWriteBuffer(cl_context context, int size) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_int errcode;
  cl_mem buffer =
      clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create read write buffer");
    exit(1);
  }

  return buffer;
}

cl_mem createReadOnlyTextureBuffer(cl_context context, int size) {
  cl_image_format imageFormat = {
      .image_channel_order = CL_RGBA,  // use all 4 bytes of channel
      /**
       * when reading image data, 128 bits (4 * 4 bytes) are always read
       * no way around it
       * Therefore, make sure to use as much data as possible
       */
      .image_channel_data_type = CL_UNSIGNED_INT32};
  cl_image_desc imageDescription = {
      .image_type = CL_MEM_OBJECT_IMAGE1D,
      .image_width =
          size / TEXTURE_CHANNEL_BYTE_SIZE,  // divide by size of channel
      .image_height = 1,
      .image_depth = 1,
      .image_array_size = 0,   // not used since we're not creating an array
      .image_row_pitch = 0,    // must be 0 if host_ptr is NULL
      .image_slice_pitch = 0,  // must be 0 if host_ptr is NULL
      .num_mip_levels = 0,     // must always be 0
      .num_samples = 0,        // must always be 0
      .buffer = NULL           // must be NULL since we're not using a buffer
  };

  startTimer(TIMER_WRITE_TO_DEVICE);

  void *host = NULL;
  cl_int errcode;
  cl_mem buffer = clCreateImage(context, CL_MEM_READ_ONLY, &imageFormat,
                                &imageDescription, host, &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create texture buffer");
    exit(1);
  }

  return buffer;
}

DfcOpenClBuffers createOpenClBuffers(DfcOpenClEnvironment *environment,
                                     DFC_PATTERNS *dfcPatterns,
                                     DfcMemoryRequirements requirements) {
  cl_context context = environment->context;

  cl_mem dfSmall;
  cl_mem dfLarge;
  if (USE_TEXTURE_MEMORY) {
    dfSmall = createReadOnlyTextureBuffer(context, DF_SIZE_REAL);
    dfLarge = createReadOnlyTextureBuffer(context, DF_SIZE_REAL);
  } else {
    dfSmall = createReadOnlyBuffer(context, DF_SIZE_REAL);
    dfLarge = createReadOnlyBuffer(context, DF_SIZE_REAL);
  }
  cl_mem dfLargeHash = createReadOnlyBuffer(context, DF_SIZE_REAL);

  cl_mem ctSmallEntries = NULL;
  cl_mem ctSmallPids = NULL;

  cl_mem ctLargeBuckets = NULL;
  cl_mem ctLargeEntries = NULL;
  cl_mem ctLargePids = NULL;

  cl_mem patterns = NULL;
  if (!HETEROGENEOUS_DESIGN) {
    ctSmallEntries = createReadOnlyBuffer(
        context, sizeof(CompactTableSmallEntry) * COMPACT_TABLE_SIZE_SMALL);
    ctSmallPids = createReadOnlyBuffer(
        context, requirements.ctSmallPidCount * sizeof(PID_TYPE));

    ctLargeBuckets = createReadOnlyBuffer(
        context, sizeof(CompactTableLargeBucket) * COMPACT_TABLE_SIZE_LARGE);
    ctLargeEntries =
        createReadOnlyBuffer(context, sizeof(CompactTableLargeEntry) *
                                          requirements.ctLargeEntryCount);
    ctLargePids = createReadOnlyBuffer(
        context, sizeof(PID_TYPE) * requirements.ctLargePidCount);

    patterns = createReadOnlyBuffer(
        context, sizeof(DFC_FIXED_PATTERN) * dfcPatterns->numPatterns);
  }

  cl_mem result = createReadWriteBuffer(
      context, sizeInBytesOfResultVector(INPUT_READ_CHUNK_BYTES));

  cl_mem input = createReadOnlyBuffer(DFC_OPENCL_ENVIRONMENT.context,
                                      INPUT_READ_CHUNK_BYTES);

  DfcOpenClBuffers memory = {.patterns = patterns,

                             .dfSmall = dfSmall,

                             .ctSmallEntries = ctSmallEntries,
                             .ctSmallPids = ctSmallPids,

                             .dfLarge = dfLarge,
                             .dfLargeHash = dfLargeHash,

                             .ctLargeBuckets = ctLargeBuckets,
                             .ctLargeEntries = ctLargeEntries,
                             .ctLargePids = ctLargePids,

                             .result = result,
                             .input = input};

  return memory;
}

void writeOpenClBuffer(cl_command_queue queue, void *host, cl_mem buffer,
                       size_t size) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_int errcode = clEnqueueWriteBuffer(queue, buffer, BLOCKING_DEVICE_ACCESS,
                                        0, size, host, 0, NULL, NULL);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not write to buffer: %d\n", errcode);
    exit(1);
  }
}

void writeOpenClTextureBuffer(cl_command_queue queue, void *host, cl_mem buffer,
                              int size) {
  size_t offset[3] = {0, 0, 0};  // origin in OpenCL
  size_t imageSize[3] = {size / TEXTURE_CHANNEL_BYTE_SIZE, 1,
                         1};  // region in OpenCL
  size_t pitch = 0;           // if 0, OpenCL calculates a fitting pitch

  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_int errcode =
      clEnqueueWriteImage(queue, buffer, BLOCKING_DEVICE_ACCESS, offset,
                          imageSize, pitch, pitch, host, 0, NULL, NULL);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not write to texture buffer: %d\n", errcode);
    exit(1);
  }
}

void writeOpenClBuffers(DfcOpenClBuffers *deviceMemory, cl_command_queue queue,
                        DfcHostMemory *hostMemory,
                        DfcMemoryRequirements requirements) {
  if (USE_TEXTURE_MEMORY) {
    writeOpenClTextureBuffer(queue, hostMemory->dfcStructure->directFilterSmall,
                             deviceMemory->dfSmall, DF_SIZE_REAL);
    writeOpenClTextureBuffer(queue, hostMemory->dfcStructure->directFilterLarge,
                             deviceMemory->dfLarge, DF_SIZE_REAL);
  } else {
    writeOpenClBuffer(queue, hostMemory->dfcStructure->directFilterSmall,
                      deviceMemory->dfSmall, DF_SIZE_REAL);
    writeOpenClBuffer(queue, hostMemory->dfcStructure->directFilterLarge,
                      deviceMemory->dfLarge, DF_SIZE_REAL);
  }

  writeOpenClBuffer(queue, hostMemory->dfcStructure->directFilterLargeHash,
                    deviceMemory->dfLargeHash, DF_SIZE_REAL);

  if (!HETEROGENEOUS_DESIGN) {
    writeOpenClBuffer(
        queue, hostMemory->dfcStructure->ctSmallEntries,
        deviceMemory->ctSmallEntries,
        sizeof(CompactTableSmallEntry) * COMPACT_TABLE_SIZE_SMALL);
    writeOpenClBuffer(queue, hostMemory->dfcStructure->ctSmallPids,
                      deviceMemory->ctSmallPids,
                      requirements.ctSmallPidCount * sizeof(PID_TYPE));

    writeOpenClBuffer(
        queue, hostMemory->dfcStructure->ctLargeBuckets,
        deviceMemory->ctLargeBuckets,
        sizeof(CompactTableLargeBucket) * COMPACT_TABLE_SIZE_LARGE);
    writeOpenClBuffer(
        queue, hostMemory->dfcStructure->ctLargeEntries,
        deviceMemory->ctLargeEntries,
        sizeof(CompactTableLargeEntry) * requirements.ctLargeEntryCount);
    writeOpenClBuffer(queue, hostMemory->dfcStructure->ctLargePids,
                      deviceMemory->ctLargePids,
                      sizeof(PID_TYPE) * requirements.ctLargePidCount);

    writeOpenClBuffer(queue, hostMemory->dfcStructure->patterns->dfcMatchList,
                      deviceMemory->patterns,
                      hostMemory->dfcStructure->patterns->numPatterns *
                          sizeof(DFC_FIXED_PATTERN));
  }
}

cl_mem createMappedBuffer(cl_context context, int inputLength) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_mem buffer =
      clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                     inputLength, NULL, NULL);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  return buffer;
}

void prepareOpenClBuffersForSearch() {
  if (MAP_MEMORY) {
    unmapOpenClInputBuffers();
    DFC_OPENCL_BUFFERS.result =
        createMappedBuffer(DFC_OPENCL_ENVIRONMENT.context,
                           sizeInBytesOfResultVector(INPUT_READ_CHUNK_BYTES));
  } else {
    DFC_OPENCL_BUFFERS = createOpenClBuffers(
        &DFC_OPENCL_ENVIRONMENT, DFC_HOST_MEMORY.dfcStructure->patterns,
        DFC_MEMORY_REQUIREMENTS);
    writeOpenClBuffers(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue,
                       &DFC_HOST_MEMORY, DFC_MEMORY_REQUIREMENTS);
  }
}

void freeOpenClBuffers() {
  clReleaseMemObject(DFC_OPENCL_BUFFERS.input);

  clReleaseMemObject(DFC_OPENCL_BUFFERS.dfSmall);
  clReleaseMemObject(DFC_OPENCL_BUFFERS.dfLarge);
  clReleaseMemObject(DFC_OPENCL_BUFFERS.dfLargeHash);

  if (!HETEROGENEOUS_DESIGN) {
    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctSmallEntries);
    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctSmallPids);

    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctLargeBuckets);
    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctLargeEntries);
    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctLargePids);

    clReleaseMemObject(DFC_OPENCL_BUFFERS.patterns);
  }

  clReleaseMemObject(DFC_OPENCL_BUFFERS.result);
}

char *getOwnershipOfInputBuffer() {
  void **host = (void **)&DFC_HOST_MEMORY.input;
  cl_command_queue queue = DFC_OPENCL_ENVIRONMENT.queue;
  cl_mem buffer = DFC_OPENCL_BUFFERS.input;
  size_t size = INPUT_READ_CHUNK_BYTES;

  if (MAP_MEMORY) {
    mapBuffer(queue, host, buffer, size);
  } else {
    if (*host == NULL) {
      *host = malloc(size);
    }
  }

  return *host;
}
void writeInputBufferToDevice(char *host, int count) {
  cl_command_queue queue = DFC_OPENCL_ENVIRONMENT.queue;
  cl_mem buffer = DFC_OPENCL_BUFFERS.input;
  if (MAP_MEMORY) {
    unmapOpenClBuffer(queue, host, buffer);
  } else {
    writeOpenClBuffer(queue, host, buffer, count);
  }
}
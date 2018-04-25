#include "memory.h"

#include "timer.h"

DfcHostMemory DFC_HOST_MEMORY;

DfcOpenClBuffers DFC_OPENCL_BUFFERS;
DfcOpenClEnvironment DFC_OPENCL_ENVIRONMENT;

int sizeInBytesOfResultVector(int inputLength) {
  if (HETEROGENEOUS_DESIGN) {
    return inputLength;
  }

  return inputLength * sizeof(VerifyResult);
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

void createBufferAndMap(cl_context context, void **host, cl_mem *buffer,
                        int size) {
  cl_int errcode;

  startTimer(TIMER_WRITE_TO_DEVICE);

  *buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                           size, NULL, &errcode);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create mapped DFC buffer");
    exit(OPENCL_COULD_NOT_CREATE_MAPPED_DFC_BUFFER);
  }

  *host = clEnqueueMapBuffer(DFC_OPENCL_ENVIRONMENT.queue, *buffer, CL_BLOCKING,
                             CL_MAP_READ | CL_MAP_WRITE, 0, size, 0, NULL, NULL,
                             &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map DFC buffer to host memory");
    exit(OPENCL_COULD_NOT_MAP_DFC_TO_HOST);
  }

  memset(*host, 0, size);
}

void createTextureBufferAndMap(cl_context context, void **host, cl_mem *buffer,
                               int size) {
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
  *host = clEnqueueMapImage(DFC_OPENCL_ENVIRONMENT.queue, *buffer, CL_BLOCKING,
                            CL_MAP_READ | CL_MAP_WRITE, offset, imageSize,
                            &pitch, &pitch, 0, NULL, NULL, &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not map DFC texture buffer to host memory");
    exit(OPENCL_COULD_NOT_MAP_DFC_TO_HOST);
  }

  memset(*host, 0, size);
}

void allocateCompactTablesOnHost(DFC_STRUCTURE *dfc) {
  dfc->compactTableSmall =
      calloc(1, sizeof(CompactTableSmallEntry) * COMPACT_TABLE_SIZE_SMALL);
  dfc->compactTableLarge =
      calloc(1, sizeof(CompactTableLarge) * COMPACT_TABLE_SIZE_LARGE);
}

void allocateDfcStructureWithMap() {
  cl_context context = DFC_OPENCL_ENVIRONMENT.context;

  DFC_STRUCTURE *dfc = malloc(sizeof(DFC_STRUCTURE));

  if (USE_TEXTURE_MEMORY) {
    createTextureBufferAndMap(context, (void *)&dfc->directFilterSmall,
                              &DFC_OPENCL_BUFFERS.dfSmall, DF_SIZE_REAL);
    createTextureBufferAndMap(context, (void *)&dfc->directFilterLarge,
                              &DFC_OPENCL_BUFFERS.dfLarge, DF_SIZE_REAL);
  } else {
    createBufferAndMap(context, (void *)&dfc->directFilterSmall,
                       &DFC_OPENCL_BUFFERS.dfSmall, DF_SIZE_REAL);
    createBufferAndMap(context, (void *)&dfc->directFilterLarge,
                       &DFC_OPENCL_BUFFERS.dfLarge, DF_SIZE_REAL);
  }
  createBufferAndMap(context, (void *)&dfc->directFilterLargeHash,
                     &DFC_OPENCL_BUFFERS.dfLargeHash, DF_SIZE_REAL);

  if (HETEROGENEOUS_DESIGN) {
    allocateCompactTablesOnHost(dfc);
  } else {
    createBufferAndMap(
        context, (void *)&dfc->compactTableSmall, &DFC_OPENCL_BUFFERS.ctSmall,
        COMPACT_TABLE_SIZE_SMALL * sizeof(CompactTableSmallEntry));
    createBufferAndMap(context, (void *)&dfc->compactTableLarge,
                       &DFC_OPENCL_BUFFERS.ctLarge,
                       COMPACT_TABLE_SIZE_LARGE * sizeof(CompactTableLarge));
  }

  DFC_HOST_MEMORY.dfcStructure = dfc;
}

void allocateDfcPatternsWithMap(int numPatterns) {
  cl_context context = DFC_OPENCL_ENVIRONMENT.context;

  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  createBufferAndMap(context, (void *)&patterns->dfcMatchList,
                     &DFC_OPENCL_BUFFERS.patterns,
                     sizeof(DFC_FIXED_PATTERN) * numPatterns);

  DFC_HOST_MEMORY.dfcStructure->patterns = patterns;
}

void allocateInputWithMap(int size) {
  DFC_HOST_MEMORY.inputLength = size;
  DFC_OPENCL_BUFFERS.inputLength = size;

  createBufferAndMap(DFC_OPENCL_ENVIRONMENT.context,
                     (void *)&DFC_HOST_MEMORY.input, &DFC_OPENCL_BUFFERS.input,
                     size);
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
    unmapOpenClBuffer(queue, dfc->compactTableSmall, buffers->ctSmall);
    unmapOpenClBuffer(queue, dfc->compactTableLarge, buffers->ctLarge);
    unmapOpenClBuffer(queue,
                      DFC_HOST_MEMORY.dfcStructure->patterns->dfcMatchList,
                      DFC_OPENCL_BUFFERS.patterns);
  }

  unmapOpenClBuffer(queue, DFC_HOST_MEMORY.input, DFC_OPENCL_BUFFERS.input);
}

void allocateDfcStructureOnHost() {
  DFC_STRUCTURE *dfc = malloc(sizeof(DFC_STRUCTURE));

  dfc->directFilterSmall = calloc(1, DF_SIZE_REAL);
  dfc->directFilterLarge = calloc(1, DF_SIZE_REAL);
  dfc->directFilterLargeHash = calloc(1, DF_SIZE_REAL);

  allocateCompactTablesOnHost(dfc);

  DFC_HOST_MEMORY.dfcStructure = dfc;
}

void allocateDfcPatternsOnHost(int numPatterns) {
  DFC_PATTERNS *patterns = malloc(sizeof(DFC_PATTERNS));

  patterns->numPatterns = numPatterns;
  patterns->dfcMatchList = calloc(1, sizeof(DFC_FIXED_PATTERN) * numPatterns);

  DFC_HOST_MEMORY.dfcStructure->patterns = patterns;
}

void allocateInputOnHost(int size) {
  DFC_HOST_MEMORY.inputLength = size;
  DFC_HOST_MEMORY.input = calloc(1, size);
}

void freeDfcStructureOnHost() {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;

  free(dfc->directFilterSmall);
  free(dfc->directFilterLarge);
  free(dfc->directFilterLargeHash);

  free(dfc->compactTableSmall);
  free(dfc->compactTableLarge);

  free(dfc);

  DFC_HOST_MEMORY.dfcStructure = NULL;
}

void freeDfcStructureWithMap() {
  DFC_STRUCTURE *dfc = DFC_HOST_MEMORY.dfcStructure;

  if (HETEROGENEOUS_DESIGN) {
    free(dfc->compactTableSmall);
    free(dfc->compactTableLarge);
  }

  free(dfc);

  DFC_HOST_MEMORY.dfcStructure = NULL;
}

void freeDfcPatternsOnHost() {
  free(DFC_HOST_MEMORY.dfcStructure->patterns->dfcMatchList);
  free(DFC_HOST_MEMORY.dfcStructure->patterns);
}

void freeDfcInputOnHost() {
  DFC_HOST_MEMORY.inputLength = 0;
  free(DFC_HOST_MEMORY.input);
}

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

void allocateDfcStructure(int numPatterns) {
  if (shouldUseMappedMemory()) {
    allocateDfcStructureWithMap();
  } else {
    allocateDfcStructureOnHost();
  }

  allocateDfcPatterns(numPatterns);
}

void allocateInput(int size) {
  if (shouldUseMappedMemory()) {
    allocateInputWithMap(size);
  } else {
    allocateInputOnHost(size);
  }
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

cl_mem createReadOnlyBuffer(cl_context context, int size) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_int errcode;
  cl_mem buffer =
      clCreateBuffer(context, CL_MEM_READ_ONLY, size, NULL, &errcode);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not create read only buffer");
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
                                     int inputLength) {
  cl_context context = environment->context;
  cl_mem kernelInput = createReadOnlyBuffer(context, inputLength);

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

  cl_mem ctSmall = NULL;
  cl_mem ctLarge = NULL;
  cl_mem patterns = NULL;
  if (!HETEROGENEOUS_DESIGN) {
    ctSmall = createReadOnlyBuffer(
        context, sizeof(CompactTableSmallEntry) * COMPACT_TABLE_SIZE_SMALL);
    ctLarge = createReadOnlyBuffer(
        context, sizeof(CompactTableLarge) * COMPACT_TABLE_SIZE_LARGE);
    patterns = createReadOnlyBuffer(
        context, sizeof(DFC_FIXED_PATTERN) * dfcPatterns->numPatterns);
  }

  cl_mem result =
      createReadWriteBuffer(context, sizeInBytesOfResultVector(inputLength));

  DfcOpenClBuffers memory = {.inputLength = inputLength,
                             .input = kernelInput,
                             .patterns = patterns,

                             .dfSmall = dfSmall,
                             .ctSmall = ctSmall,

                             .dfLarge = dfLarge,
                             .dfLargeHash = dfLargeHash,
                             .ctLarge = ctLarge,

                             .result = result};

  return memory;
}

void writeOpenClBuffer(cl_command_queue queue, void *host, cl_mem buffer,
                       int size) {
  startTimer(TIMER_WRITE_TO_DEVICE);

  cl_int errcode = clEnqueueWriteBuffer(queue, buffer, CL_BLOCKING, 0, size,
                                        host, 0, NULL, NULL);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not write to buffer");
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
      clEnqueueWriteImage(queue, buffer, CL_BLOCKING, offset, imageSize, pitch,
                          pitch, host, 0, NULL, NULL);

  stopTimer(TIMER_WRITE_TO_DEVICE);

  if (errcode != CL_SUCCESS) {
    fprintf(stderr, "Could not write to texture buffer: %d\n", errcode);
    exit(1);
  }
}

void writeOpenClBuffers(DfcOpenClBuffers *deviceMemory, cl_command_queue queue,
                        DfcHostMemory *hostMemory) {
  writeOpenClBuffer(queue, hostMemory->input, deviceMemory->input,
                    deviceMemory->inputLength);

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
        queue, hostMemory->dfcStructure->compactTableSmall,
        deviceMemory->ctSmall,
        sizeof(CompactTableSmallEntry) * COMPACT_TABLE_SIZE_SMALL);
    writeOpenClBuffer(queue, hostMemory->dfcStructure->compactTableLarge,
                      deviceMemory->ctLarge,
                      sizeof(CompactTableLarge) * COMPACT_TABLE_SIZE_LARGE);
    writeOpenClBuffer(queue, hostMemory->dfcStructure->patterns->dfcMatchList,
                      deviceMemory->patterns,
                      hostMemory->dfcStructure->patterns->numPatterns *
                          sizeof(DFC_FIXED_PATTERN));
  }
}

cl_mem createMappedResultBuffer(cl_context context, int inputLength) {
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
    DFC_OPENCL_BUFFERS.result = createMappedResultBuffer(
        DFC_OPENCL_ENVIRONMENT.context,
        sizeInBytesOfResultVector(DFC_HOST_MEMORY.inputLength));
  } else {
    DFC_OPENCL_BUFFERS = createOpenClBuffers(
        &DFC_OPENCL_ENVIRONMENT, DFC_HOST_MEMORY.dfcStructure->patterns,
        DFC_HOST_MEMORY.inputLength);
    writeOpenClBuffers(&DFC_OPENCL_BUFFERS, DFC_OPENCL_ENVIRONMENT.queue,
                       &DFC_HOST_MEMORY);
  }
}

void freeOpenClBuffers() {
  DFC_OPENCL_BUFFERS.inputLength = 0;
  clReleaseMemObject(DFC_OPENCL_BUFFERS.input);

  clReleaseMemObject(DFC_OPENCL_BUFFERS.dfSmall);
  clReleaseMemObject(DFC_OPENCL_BUFFERS.dfLarge);
  clReleaseMemObject(DFC_OPENCL_BUFFERS.dfLargeHash);

  if (!HETEROGENEOUS_DESIGN) {
    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctSmall);
    clReleaseMemObject(DFC_OPENCL_BUFFERS.ctLarge);
    clReleaseMemObject(DFC_OPENCL_BUFFERS.patterns);
  }

  clReleaseMemObject(DFC_OPENCL_BUFFERS.result);
}
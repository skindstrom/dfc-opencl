cmake_minimum_required (VERSION 3.1)
project(OpenCL-DFC)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra -Werror")
set(CMAKE_CXX_STANDARD 11)

set(EXT_PROJECTS_DIR external)

set(OpenCL_VERSION 120)
find_package(OpenCL)

# The ODROID-XU4 only supports OpenCL 1.2, so enable the deprecated API
add_definitions(-DCL_USE_DEPRECATED_OPENCL_1_2_APIS)

#########################################
# Feature flags
# 0 = OFF
# 1 = ON

# Feature flags
# These two are mutually exclusive
set(DFC_SEARCH_WITH_GPU 0)
set(DFC_HETEROGENEOUS_DESIGN 1)

set(DFC_MAP_MEMORY 1)
set(DFC_USE_TEXTURE_MEMORY 0)

# Continous values
set(DFC_WORK_GROUP_SIZE 32)
# the amount positions in the input that gets checked per thread
# higher value = more work per thread = fewer threads
set(DFC_CHECK_COUNT_PER_THREAD 10)

#########################################

if (${DFC_SEARCH_WITH_GPU} AND ${DFC_HETEROGENEOUS_DESIGN})
  message( FATAL_ERROR "DFC: Both DFC_SEARCH_WITH_GPU and DFC_HETEROGENEOUS_DESIGN may not be set")
endif()

set(DFC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utility.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/constants.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/search.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shared-functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/memory.h

)
set(DFC_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.c
      ${CMAKE_CURRENT_SOURCE_DIR}/src/memory.c
      ${CMAKE_CURRENT_SOURCE_DIR}/src/utility.c
      ${CMAKE_CURRENT_SOURCE_DIR}/src/search/search.c
      ${CMAKE_CURRENT_SOURCE_DIR}/src/search/search-gpu.c
      ${CMAKE_CURRENT_SOURCE_DIR}/src/search/search-cpu.c
      )

if(${DFC_SEARCH_WITH_GPU})
  message("DFC: Using GPU version of DFC algorithm")
elseif(${DFC_HETEROGENEOUS_DESIGN})
  message("DFC: Using heterogeneous version of DFC algorithm")
else()
  message("DFC: Using CPU version of DFC algorithm")
endif()

if(${DFC_MAP_MEMORY})
  message("DFC: Mapping memory to reduce memory transfers")
endif()

if(${DFC_USE_TEXTURE_MEMORY})
  message("DFC: Using texture memory for some data structures")
endif()

message("DFC: Using work groups of size ${DFC_WORK_GROUP_SIZE}")

if(${DFC_SEARCH_WITH_GPU} OR ${DFC_HETEROGENEOUS_DESIGN})
message("DFC: Each GPU thread will check ${DFC_CHECK_COUNT_PER_THREAD} locations of the input")
endif()

set(DFC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(dfc SHARED ${DFC_HEADERS} ${DFC_SOURCES})
target_include_directories(dfc PUBLIC ${DFC_INCLUDE_DIR} ${OpenCL_INCLUDE_DIRS})
target_link_libraries(dfc ${OpenCL_LIBRARIES} -lm)
target_compile_definitions(dfc PRIVATE
    SEARCH_WITH_GPU=${DFC_SEARCH_WITH_GPU}
    HETEROGENEOUS_DESIGN=${DFC_HETEROGENEOUS_DESIGN}
    WORK_GROUP_SIZE=${DFC_WORK_GROUP_SIZE}
    MAP_MEMORY=${DFC_MAP_MEMORY}
    CHECK_COUNT_PER_THREAD=${DFC_CHECK_COUNT_PER_THREAD}
    USE_TEXTURE_MEMORY=${DFC_USE_TEXTURE_MEMORY}
    )

add_subdirectory(${EXT_PROJECTS_DIR}/catch)
add_subdirectory(tests)
add_subdirectory(example)

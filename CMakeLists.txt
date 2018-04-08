cmake_minimum_required (VERSION 3.1)
project(OpenCL-DFC)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra -Werror")
set(CMAKE_CXX_STANDARD 11)

set(EXT_PROJECTS_DIR external)

set(OpenCL_VERSION 120)
find_package(OpenCL)

if(NOT ${OpenCL_FOUND})
  set (LOCAL_OpenCL "${CMAKE_SOURCE_DIR}/../Mali_OpenCL_SDK")
  if (EXISTS ${LOCAL_OpenCL})
    message("DFC: Found local version of OpenCL")
    set(OpenCL_INCLUDE_DIRS "${LOCAL_OpenCL}/include")
    # Needs a system-wide opencl library installed
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -L/usr/lib/arm-linux-gnueabihf/")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/lib/arm-linux-gnueabihf/")
    set(OpenCL_LIBRARIES "OpenCL")
  else()
    message( FATAL_ERROR "OpenCL not found" )
  endif()
endif()

# The ODROID-XU4 only supports OpenCL 1.2, so enable the deprecated API
add_definitions(-DCL_USE_DEPRECATED_OPENCL_1_2_APIS)

#########################################
# Feature flags
# 0 = OFF
# 1 = ON

set(DFC_SEARCH_WITH_GPU 1)
set(DFC_WORK_GROUP_SIZE 32)
# the amount positions in the input that gets checked per thread
set(DFC_CHECK_COUNT_PER_THREAD 10)

#########################################

set(DFC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utility.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/constants.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/search.h
)
set(DFC_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.c
      ${CMAKE_CURRENT_SOURCE_DIR}/src/utility.c
      )

if(${DFC_SEARCH_WITH_GPU})
  message("DFC: Using GPU version of DFC algorithm")
  set(DFC_SOURCES ${DFC_SOURCES}
      ${CMAKE_CURRENT_SOURCE_DIR}/src/search/search-gpu.c)
else()
  message("DFC: Using CPU version of DFC algorithm")
  set(DFC_SOURCES ${DFC_SOURCES}
      ${CMAKE_CURRENT_SOURCE_DIR}/src/search/search-cpu.c)
endif()


set(DFC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(dfc SHARED ${DFC_HEADERS} ${DFC_SOURCES})
target_include_directories(dfc PUBLIC ${DFC_INCLUDE_DIR} ${OpenCL_INCLUDE_DIRS})
target_link_libraries(dfc ${OpenCL_LIBRARIES} -lm)
target_compile_definitions(dfc PRIVATE
    SEARCH_WITH_GPU=${DFC_SEARCH_WITH_GPU}
    WORK_GROUP_SIZE=${DFC_WORK_GROUP_SIZE}
    MAP_MEMORY=${DFC_MAP_MEMORY}
    CHECK_COUNT_PER_THREAD=${DFC_CHECK_COUNT_PER_THREAD}
    )

add_subdirectory(${EXT_PROJECTS_DIR}/catch)
add_subdirectory(tests)
add_subdirectory(example)

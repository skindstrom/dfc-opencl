cmake_minimum_required (VERSION 3.1)
project(OpenCL-DFC)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra -Werror")
set(CMAKE_CXX_STANDARD 14)

set(EXT_PROJECTS_DIR external)

set(OpenCL_VERSION 120)
find_package(OpenCL)

if(NOT ${OpenCL_FOUND})
  set (LOCAL_OpenCL "${CMAKE_SOURCE_DIR}/../Mali_OpenCL_SDK")
  if (EXISTS ${LOCAL_OpenCL})
    message( "Found local version of OpenCL")
    set(OpenCL_INCLUDE_DIRS "${LOCAL_OpenCL}/include")
    # Needs a system-wide opencl library installed
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -L/usr/lib/arm-linux-gnueabihf/")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/lib/arm-linux-gnueabihf/")
    set(OpenCL_LIBRARIES "OpenCL")
  else()
    message( FATAL_ERROR "OpenCL not found" )
  endif()
endif()

add_definitions(-DCL_USE_DEPRECATED_OPENCL_1_2_APIS)

set(DFC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utility.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/constants.h
)
set(DFC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.c)

set(DFC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(dfc SHARED ${DFC_HEADERS} ${DFC_SOURCES})
target_include_directories(dfc PUBLIC ${DFC_INCLUDE_DIR} ${OpenCL_INCLUDE_DIRS})
target_link_libraries(dfc ${OpenCL_LIBRARIES} -lm)

add_subdirectory(${EXT_PROJECTS_DIR}/catch)
add_subdirectory(tests)
add_subdirectory(example)

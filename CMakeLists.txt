cmake_minimum_required (VERSION 3.1)
project(OpenCL-DFC)

SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -march=native" )

set(EXT_PROJECTS_DIR external)

set(CMAKE_CXX_STANDARD 14)

set(DFC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.h ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc_framework.h)
set(DFC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DFC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/dfc.c)

add_library(dfc SHARED ${DFC_HEADERS} ${DFC_SOURCES})
target_include_directories(dfc PUBLIC ${DFC_INCLUDE_DIR})

add_subdirectory(tests)
add_subdirectory(example)
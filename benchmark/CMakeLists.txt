project(DFC-Benchmark CXX)
set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -Werror" )

set(BENCHMARK_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/parser.c)

add_executable(benchmark-tests tests-main.cpp tests.cpp ${BENCHMARK_SRCS})
add_dependencies(benchmark-tests catch)
target_include_directories(benchmark-tests PUBLIC ${CATCH_INCLUDE_DIR} ${DFC_INCLUDE_DIR})
target_link_libraries(benchmark-tests dfc)
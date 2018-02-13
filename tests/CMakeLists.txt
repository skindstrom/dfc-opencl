project(DFC-Tests CXX)
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -Werror" )

add_executable(tests tests-main.cpp tests.cpp)
target_include_directories(tests PUBLIC ${CATCH_INCLUDE_DIR} ${DFC_INCLUDE_DIR})
target_link_libraries(tests dfc)
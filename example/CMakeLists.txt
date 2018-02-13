project(DFC-Example C)

add_executable(example example.c)
target_include_directories(example PUBLIC ${DFC_INCLUDE_DIR})
target_link_libraries(example dfc)
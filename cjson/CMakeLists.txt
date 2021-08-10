cmake_minimum_required (VERSION 2.8.11)
add_library(cjson cjson.c)

target_include_directories(cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

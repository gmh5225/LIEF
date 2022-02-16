message("Cmake version: ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}")

cmake_minimum_required(VERSION 3.12)

project(mytest)

# set include directories
include_directories(${CMAKE_SOURCE_DIR}../include)

# add subdirectory
add_subdirectory(../ build)
add_subdirectory(src)






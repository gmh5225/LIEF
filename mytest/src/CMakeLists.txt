cmake_minimum_required(VERSION 3.0.2)

project( mytest )

set( SOURCE 
main.cpp
)


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od") 
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /ZI") 
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")

add_executable( ${PROJECT_NAME} ${SOURCE} )

target_link_libraries(${PROJECT_NAME} LIB_LIEF)
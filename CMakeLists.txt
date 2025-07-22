cmake_minimum_required(VERSION 3.20)

# set the project name
project(lmdbpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)

# include folders
FILE(GLOB_RECURSE MY_HEADERS "./*.h*")

include_directories(./)
add_executable(lmdbpp midl.c mdb.c lmdbpp-test.cpp ${MY_HEADERS} )

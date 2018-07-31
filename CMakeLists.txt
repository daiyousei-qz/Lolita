cmake_minimum_required(VERSION 3.10)
project(Lolita CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories("Reference/edslib/edslib/src")

add_subdirectory("lolita")
add_subdirectory("lolita-test")
cmake_minimum_required(VERSION 3.15)
project(rn-ex03
  VERSION 0.1.0
  LANGUAGES C
)

set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer"
    CACHE STRING "Enable ASAN"
    FORCE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 

add_subdirectory(src)

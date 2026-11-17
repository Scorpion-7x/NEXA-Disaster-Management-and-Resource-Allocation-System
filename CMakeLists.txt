cmake_minimum_required(VERSION 3.16)
project(NEXA CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  add_compile_options(-Wall -Wextra)
endif()

include_directories(include)

file(GLOB_RECURSE SRC src/*.cpp)
add_executable(nexa ${SRC})

enable_testing()
file(GLOB TESTS tests/test_*.cpp)
foreach(t ${TESTS})
  get_filename_component(name ${t} NAME_WE)
  add_executable(${name} ${t})
  target_include_directories(${name} PRIVATE tests)
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()

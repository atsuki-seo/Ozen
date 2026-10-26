# SPDX-License-Identifier: MIT OR Apache-2.0
cmake_minimum_required(VERSION 3.28)
project(ozen LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
# CMake 3.28 spells C23 as -std=c2x for clang 17; the plan pins -std=c23 for clang (D13).
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C23_STANDARD_COMPILE_OPTION "-std=c23")
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OZN_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(OZN_FUZZ "Build the libFuzzer targets" OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Wswitch-enum -Werror)
if(OZN_FUZZ)
  add_compile_options(-fsanitize=fuzzer-no-link)
endif()
if(OZN_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()

file(GLOB OZN_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_library(ozen_lib STATIC ${OZN_SOURCES})
set_target_properties(ozen_lib PROPERTIES OUTPUT_NAME ozen)
target_include_directories(ozen_lib PUBLIC src)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cli ${CMAKE_CURRENT_BINARY_DIR}/cli)

enable_testing()
if(EMSCRIPTEN)
  return()
endif()

add_test(NAME symbols_prefixed COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/check-symbols.sh $<TARGET_FILE:ozen_lib>)

file(GLOB OZN_UNIT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.c)
add_executable(ozen_unit ${OZN_UNIT_SOURCES})
target_link_libraries(ozen_unit PRIVATE ozen_lib)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/tests.def OZN_UNIT_LINES REGEX "^OZN_TEST\\(")
foreach(l IN LISTS OZN_UNIT_LINES)
  string(REGEX REPLACE "^OZN_TEST\\(([a-z0-9_]+)\\)$" "\\1" t "${l}")
  add_test(NAME ${t} COMMAND ozen_unit ${t})
endforeach()

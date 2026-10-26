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
set(OZN_STDLIB ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib)
file(GLOB_RECURSE OZN_STD_FILES CONFIGURE_DEPENDS ${OZN_STDLIB}/*.ozen)
set(OZN_STD_C ${CMAKE_CURRENT_BINARY_DIR}/gen/std_embedded.c)
add_custom_command(OUTPUT ${OZN_STD_C}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/gen
  COMMAND ${CMAKE_COMMAND} -DSTDLIB=${OZN_STDLIB} -DOUT=${OZN_STD_C} -P ${CMAKE_CURRENT_SOURCE_DIR}/gen/embed-std.cmake
  DEPENDS ${OZN_STD_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/gen/embed-std.cmake)
add_library(ozen_lib STATIC ${OZN_SOURCES} ${OZN_STD_C})
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
  if(t STREQUAL "span_nesting")
    file(GLOB_RECURSE OZN_GOLDEN_INPUTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../tests/golden/*.ozen)
    list(SORT OZN_GOLDEN_INPUTS)
    add_test(NAME ${t} COMMAND ozen_unit ${t} ${OZN_GOLDEN_INPUTS})
  else()
    add_test(NAME ${t} COMMAND ozen_unit ${t})
  endif()
  if(t STREQUAL "arena_poison_detects" AND OZN_SANITIZE)
    set_tests_properties(${t} PROPERTIES WILL_FAIL TRUE)
  endif()
endforeach()

add_executable(ozen_oom_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/oom/oom_test.c)
target_link_libraries(ozen_oom_test PRIVATE ozen_lib)

if(OZN_FUZZ)
  add_executable(fuzz_parse ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_parse.c)
  target_compile_options(fuzz_parse PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_parse PRIVATE -fsanitize=fuzzer)
  target_link_libraries(fuzz_parse PRIVATE ozen_lib)
endif()

# SPDX-License-Identifier: MIT OR Apache-2.0
add_executable(ozen ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)
target_link_libraries(ozen PRIVATE ozen_lib)
set_target_properties(ozen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

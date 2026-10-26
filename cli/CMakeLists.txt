# SPDX-License-Identifier: MIT OR Apache-2.0
add_executable(ozen ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c ${CMAKE_CURRENT_SOURCE_DIR}/src/sarif.c)
target_link_libraries(ozen PRIVATE ozen_lib)
set_target_properties(ozen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION OZN_VERSION LIMIT_COUNT 1)
target_compile_definitions(ozen PRIVATE OZEN_VERSION="${OZN_VERSION}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION)
if(EMSCRIPTEN)
  target_link_options(ozen PRIVATE -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sNODERAWFS=1 -sEXIT_RUNTIME=1)
  add_executable(ozen_bridge ${CMAKE_CURRENT_SOURCE_DIR}/../bindings/wasm/bridge.c)
  target_include_directories(ozen_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/include)
  target_link_libraries(ozen_bridge PRIVATE ozen_lib)
  set_target_properties(ozen_bridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  target_link_options(ozen_bridge PRIVATE -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sENVIRONMENT=web,worker,node
    -sMODULARIZE=1 -sEXPORT_NAME=OzenModule -sEXPORTED_FUNCTIONS=_ozw_build,_ozw_free,_malloc,_free
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32)
endif()

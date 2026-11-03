# SPDX-License-Identifier: MIT OR Apache-2.0
# Usage: cmake -DIN=<agents-template.md> -DOUT=<file.h> -P embed-template.cmake
file(READ ${IN} hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
file(WRITE ${OUT}.tmp "/* Generated from reference/agents-template.md by cli/src/embed-template.cmake. */\nstatic const unsigned char agents_template[] = {${bytes}0x00};\n")
file(COPY_FILE ${OUT}.tmp ${OUT} ONLY_IF_DIFFERENT)

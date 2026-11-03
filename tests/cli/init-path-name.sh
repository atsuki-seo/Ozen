#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
"$ozen" init my-book > out.txt 2>&1
want "$?" 2 "init status"
[ -e my-book ] && fail "created my-book"
"$ozen" init my-book --name myBook > out.txt 2>&1
want "$?" 0 "init status with --name"
grep -qx 'package myBook {' my-book/package.ozen || fail "package name"
exit 0

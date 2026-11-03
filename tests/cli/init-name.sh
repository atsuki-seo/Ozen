#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
"$ozen" init book --name kitchenBook > out.txt 2>&1
want "$?" 0 "init status"
want "$(head -n 1 book/package.ozen)" 'package kitchenBook {' "package line"
exit 0

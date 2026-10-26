#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

: "${OZEN_SIM_UDID:?}" "${OZEN_SIM_LAUNCHER:?}" "${OZEN_SIM_CLI:?}"
exec xcrun simctl spawn "$OZEN_SIM_UDID" "$OZEN_SIM_LAUNCHER" "$PWD" "$OZEN_SIM_CLI" "$@"

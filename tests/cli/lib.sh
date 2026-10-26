# SPDX-License-Identifier: MIT OR Apache-2.0
ozen=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

want() {
  [ "$1" = "$2" ] || fail "$3: got '$1', want '$2'"
}

file() {
  mkdir -p "$(dirname "$1")"
  cat > "$1"
}

package() {
  file package.ozen <<'OZEN'
package demo {
  version: "0.1.0"
}
OZEN
}

recipe() {
  file "$1" <<OZEN
recipe $2() {
  ingredients {
    rice: 300 g
  }
  let w = wash(rice)
}
OZEN
}

#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
file zsoup.ozen <<'OZEN'
recipe zsoup {
  ingredients {
    water: 400 ml
  }
  let simmered = simmer(water, 5 min)
}
OZEN
file dinner.ozen <<'OZEN'
import zsoup.{zsoup}

menu dinner(people: 2) {
  let soupBowl = make zsoup
  serve main {
    plate perPerson on SoupBowl { s }
  }
}
OZEN
out=$("$ozen" render --kind schedule --lang en dinner)
want "$?" 0 "module target status"
grep -q 'dinner' <<< "$(head -n 1 <<< "$out")" || fail "module target renders $(head -n 1 <<< "$out")"
out=$("$ozen" render --kind schedule --lang en)
want "$?" 0 "default entry status"
grep -q 'dinner' <<< "$(head -n 1 <<< "$out")" || fail "default entry renders $(head -n 1 <<< "$out")"
out=$("$ozen" render --serve-at 18:00 dinner 2>&1)
want "$?" 1 "error result status"
grep -q 'SYN003' <<< "$out" || fail "error result output: $out"
file lunch.ozen <<'OZEN'
import zsoup.{zsoup}

menu lunch(people: 1) {
  let soupBowl = make zsoup
  serve main {
    plate perPerson on SoupBowl { s }
  }
}
OZEN
out=$("$ozen" render --kind schedule 2>&1)
want "$?" 1 "ambiguous status"
grep -q 'NAM009' <<< "$out" || fail "ambiguous output: $out"

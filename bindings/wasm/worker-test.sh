#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
wasm=${BRIDGE_WASM:-$root/build/core-wasm}
native=${BRIDGE_NATIVE:-$root/build/core-gcc-release/ozen}
inputs_list=${BRIDGE_INPUTS:-$root/bindings/inputs.txt}
bridge_js=${BRIDGE_JS:-$root/bindings/wasm/ozen.mjs}
work=$(mktemp -d)
server=""
chrome=""
cleanup() {
  [ -n "$chrome" ] && kill -TERM -- "-$chrome" 2>/dev/null
  [ -n "$server" ] && kill "$server" 2>/dev/null
  wait 2>/dev/null
  for _ in $(seq 50); do
    rm -rf "$work" 2>/dev/null && break
    sleep 0.1
  done
}
trap cleanup EXIT
mkdir -p "$work/site" "$work/profile" "$work/native"
cp "$wasm/ozen_bridge.js" "$wasm/ozen_bridge.wasm" "$root/bindings/wasm/worker.js" "$root/bindings/wasm/node-run.mjs" \
  "$root/bindings/wasm/worker-test.html" "$work/site/"
cp "$bridge_js" "$work/site/ozen.mjs"
mapfile -t inputs < <(grep -v '^#' "$inputs_list")
python3 -B - "$root" "$work/site/inputs.json" "${inputs[@]}" <<'PY'
import json, os, sys
root, out = sys.argv[1], sys.argv[2]
items = [{"path": os.path.basename(f), "text": open(os.path.join(root, f), encoding="utf-8").read()} for f in sys.argv[3:]]
json.dump(items, open(out, "w", encoding="utf-8"))
PY
for i in "${!inputs[@]}"; do
  f=$root/${inputs[$i]}
  (cd "$(dirname "$f")" && "$native" check --format json --emit ir,schedule,steps,definitions "$(basename "$f")") \
    > "$work/native/$i.json" 2>/dev/null
done
node "$work/site/node-run.mjs" "$wasm/ozen_bridge.js" "$work/site/inputs.json" "$work/node.json" 2>/dev/null
port=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')
# Headless Chrome's --dump-dom does not wait for Web Workers, so the page POSTs its result to this server.
python3 -B - "$work/site" "$port" "$work/worker.json" >/dev/null 2>&1 <<'PY' &
import functools, http.server, sys
site, port, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".mjs": "text/javascript",
                      ".wasm": "application/wasm"}
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        with open(out, "wb") as f:
            f.write(body)
        self.send_response(204)
        self.end_headers()
http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=site)).serve_forever()
PY
server=$!
for _ in $(seq 50); do
  curl -s -o /dev/null "http://127.0.0.1:$port/worker-test.html" && break
  sleep 0.1
done
setsid google-chrome --headless=new --disable-gpu --no-first-run --no-default-browser-check \
  --user-data-dir="$work/profile" "http://127.0.0.1:$port/worker-test.html" >/dev/null 2>&1 &
chrome=$!
for _ in $(seq 600); do
  [ -s "$work/worker.json" ] && break
  sleep 0.1
done
python3 -B - "$work" "${#inputs[@]}" <<'PY'
import json, os, sys
work, n = sys.argv[1], int(sys.argv[2])
def load(name):
    try:
        return json.load(open(os.path.join(work, name), encoding="utf-8"))
    except (OSError, ValueError):
        return None
results = {"node": load("node.json"), "worker": load("worker.json")}
mismatches = 0
for i in range(n):
    want = open(os.path.join(work, "native", f"{i}.json"), encoding="utf-8").read()
    bad = [name for name, got in results.items() if not isinstance(got, list) or i >= len(got) or got[i] != want]
    if bad:
        mismatches += 1
        print(f"mismatch: {i} ({', '.join(bad)})", file=sys.stderr)
print(f"worker: {n} inputs, {mismatches} mismatches")
sys.exit(0 if mismatches == 0 else 1)
PY

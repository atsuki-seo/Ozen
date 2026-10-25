#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
worker="$root/proto/wasm/worker.js"
while [ $# -gt 0 ]; do
  case "$1" in
    --worker) worker="$2"; shift 2 ;;
    *) echo "usage: $0 [--worker FILE]" >&2; exit 2 ;;
  esac
done
web="$root/build/wasm-web"
native="$root/build/gcc-release/ozenproto"
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
mkdir -p "$work/site" "$work/profile"
cp "$web/ozenproto_bridge.js" "$web/ozenproto_bridge.wasm" "$root/proto/wasm/worker-test.html" "$work/site/"
cp "$worker" "$work/site/worker.js"
node --input-type=module -e "import { inputs } from '$root/proto/wasm/inputs.mjs'; console.log(JSON.stringify(inputs));" > "$work/site/inputs.json"
port=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')
# Headless Chrome's --dump-dom does not wait for Web Workers, so the page POSTs its result to this server.
python3 -B - "$work/site" "$port" "$work/result.json" >/dev/null 2>&1 <<'PY' &
import functools, http.server, sys
site, port, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
class Handler(http.server.SimpleHTTPRequestHandler):
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
for _ in $(seq 300); do
  [ -s "$work/result.json" ] && break
  sleep 0.1
done
python3 -B - "$work/result.json" "$work/site/inputs.json" "$native" <<'PY'
import json, os, subprocess, sys, tempfile
result, inputs_path, native = sys.argv[1:4]
inputs = json.load(open(inputs_path, encoding="utf-8"))
try:
    got = json.load(open(result, encoding="utf-8"))
except (OSError, ValueError):
    got = None
mismatches = 0
with tempfile.TemporaryDirectory() as tmp:
    for i, text in enumerate(inputs):
        path = os.path.join(tmp, f"in{i}.ozen")
        open(path, "w", encoding="utf-8").write(text)
        want = subprocess.run([native, path], capture_output=True, text=True).stdout
        if not isinstance(got, list) or i >= len(got) or got[i] != want:
            mismatches += 1
print(f"worker: {len(inputs)} inputs, {mismatches} mismatches")
sys.exit(0 if mismatches == 0 else 1)
PY

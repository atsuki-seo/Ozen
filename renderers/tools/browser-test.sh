#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
json_dir=${BROWSER_JSON:-$root/build/results/json/samples}
samples=${BROWSER_SAMPLES:-$root/samples}
chrome_bin=${BROWSER_CHROME:-google-chrome}
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
if [ -n "${BROWSER_BUNDLE:-}" ]; then
  cp "$BROWSER_BUNDLE" "$work/site/ozen-renderer.js"
else
  node "$here/bundle.mjs" "$work/site/ozen-renderer.js" >/dev/null
fi
cp "$here/browser-test.html" "$here/browser-cases.mjs" "$work/site/"
n=$(python3 -B - "$json_dir" "$samples" "$root/phrases" "$work/site/inputs.json" <<'PY'
import json, os, sys
json_dir, samples, std_dir, out = sys.argv[1:5]
def phrases(d, name):
    read = lambda lang: open(os.path.join(d, f"{lang}.ftl"), encoding="utf-8").read() if os.path.isfile(os.path.join(d, f"{lang}.ftl")) else None
    return {"name": name, "files": {"ja": read("ja"), "en": read("en")}}
files = sorted(f for f in os.listdir(json_dir) if f.endswith(".json")) if os.path.isdir(json_dir) else []
items = []
for f in files:
    sample = f.split("--")[0]
    items.append({"name": f, "result": json.load(open(os.path.join(json_dir, f), encoding="utf-8")),
                  "phrases": {"std": phrases(std_dir, "std"), "project": phrases(os.path.join(samples, sample, "phrases"), sample)}})
json.dump(items, open(out, "w", encoding="utf-8"))
print(len(items))
PY
)
node "$here/browser-run.mjs" "$work/site/inputs.json" "$work/node.json"
port=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')
# Headless Chrome's --dump-dom returns before module scripts finish their fetches, so the page POSTs its result.
python3 -B - "$work/site" "$port" "$work/browser.json" >/dev/null 2>&1 <<'PY' &
import functools, http.server, sys
site, port, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".mjs": "text/javascript", ".js": "text/javascript"}
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
  curl -s -o /dev/null "http://127.0.0.1:$port/browser-test.html" && break
  sleep 0.1
done
setsid "$chrome_bin" --headless=new --disable-gpu --no-first-run --no-default-browser-check \
  --user-data-dir="$work/profile" "http://127.0.0.1:$port/browser-test.html" >/dev/null 2>&1 &
chrome=$!
for _ in $(seq 600); do
  [ -s "$work/browser.json" ] && break
  sleep 0.1
done
python3 -B - "$work" "$n" <<'PY'
import json, os, sys
work, n = sys.argv[1], int(sys.argv[2])
def load(name):
    try:
        return json.load(open(os.path.join(work, name), encoding="utf-8"))
    except (OSError, ValueError):
        return None
node, browser = load("node.json"), load("browser.json")
if isinstance(browser, dict):
    print(f"browser error: {browser.get('error')}", file=sys.stderr)
mismatches = 0
for i in range(n):
    want = node[i] if isinstance(node, list) and i < len(node) else None
    got = browser[i] if isinstance(browser, list) and i < len(browser) else None
    if want is None or got != want:
        mismatches += 1
        bad = sorted(k for k in (want or {}) if not isinstance(got, dict) or got.get(k) != want[k])
        print(f"mismatch: {i} ({', '.join(bad[:5])})", file=sys.stderr)
print(f"browser: {n} inputs, {mismatches} mismatches")
sys.exit(0 if n > 0 and mismatches == 0 else 1)
PY

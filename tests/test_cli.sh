#!/usr/bin/env bash
# tests/test_cli.sh - Exercises the built CLI binary end to end, in both
# human and --json output modes, against a real (test) account and server.
#
# Deliberately no default credentials, same reason as the web Playwright
# suite: a hardcoded fallback account that silently stops existing makes
# every test "pass" by failing to log in. Set these before running:
#
#   TEST_EMAIL=you@example.com TEST_PASSWORD='...' ./tests/test_cli.sh
#
# TEST_BASE_URL defaults to dev, not prod - this creates and deletes real
# posts/comments, and dev is where that belongs.
#
# Runs the binary with HOME pointed at a scratch directory for the whole
# run, so it never touches your real ~/.simple-social-cli or
# ~/.config/simple-social-cli, and cleans that scratch dir up on exit
# whether the run passed or failed.

set -u
cd "$(dirname "$0")/.."

BASE_URL="${TEST_BASE_URL:-https://dev.davidfruin.com}"
BIN="./simple-social-cli"

if [ -z "${TEST_EMAIL:-}" ] || [ -z "${TEST_PASSWORD:-}" ]; then
  echo "Set TEST_EMAIL and TEST_PASSWORD (a real account on $BASE_URL) before running." >&2
  exit 1
fi

if [ ! -x "$BIN" ]; then
  echo "Building..." >&2
  make >&2 || { echo "build failed"; exit 1; }
fi

SCRATCH_HOME="$(mktemp -d)"
mkdir -p "$SCRATCH_HOME/.config/simple-social-cli"
echo "base_url = $BASE_URL/api.php" > "$SCRATCH_HOME/.config/simple-social-cli/config.ini"
export HOME="$SCRATCH_HOME"

CLEANUP_POST_IDS=()
cleanup() {
  for pid in "${CLEANUP_POST_IDS[@]:-}"; do
    [ -n "$pid" ] && "$BIN" --json delete "$pid" > /dev/null 2>&1
  done
  rm -rf "$SCRATCH_HOME"
}
trap cleanup EXIT

PASS=0
FAIL=0
fail() { echo "FAIL: $1"; FAIL=$((FAIL + 1)); }
ok()   { PASS=$((PASS + 1)); }

# Runs a command and asserts its exit code, printing stdout only (the app
# separates streams deliberately - "Error: ..." on stderr, clean JSON on
# stdout, verified by hand before this script existed - so a caller that
# wants to validate JSON needs stdout alone, not stdout+stderr merged).
# $1 = label, $2 = expected exit code, rest = command.
run() {
  local label="$1" want_rc="$2"; shift 2
  local out err rc
  err="$(mktemp)"
  out="$("$@" 2>"$err")"; rc=$?
  if [ "$rc" != "$want_rc" ]; then
    fail "$label: exit $rc, wanted $want_rc (stdout: $out / stderr: $(cat "$err"))"
    rm -f "$err"
    return 1
  fi
  rm -f "$err"
  ok
  printf '%s' "$out"
}

json_field() {
  # $1 = json string, $2 = python expression on the parsed object
  python3 -c "import sys, json; d = json.loads(sys.argv[1]); print($2)" "$1" 2>/dev/null
}

assert_valid_json() {
  local label="$1" json="$2"
  if echo "$json" | python3 -m json.tool > /dev/null 2>&1; then ok; else
    fail "$label: not valid JSON: $json"
  fi
}

echo "== auth =="
run "login" 0 "$BIN" login "$TEST_EMAIL" "$TEST_PASSWORD" > /dev/null

out=$(run "whoami --json" 0 "$BIN" --json whoami)
assert_valid_json "whoami --json shape" "$out"
[ "$(json_field "$out" 'd["email"]')" = "$TEST_EMAIL" ] && ok || fail "whoami email mismatch: $out"

run "whoami (human)" 0 "$BIN" whoami > /dev/null

echo "== reads, --json validity =="
for cmd in "feed --limit 3" "posts --limit 3" "users" "profile" \
           "followers" "following" "notifications --limit 3" "notify-count"; do
  out=$(run "$cmd --json" 0 "$BIN" --json $cmd)
  assert_valid_json "$cmd --json" "$out"
done

echo "== reads, human mode doesn't crash =="
for cmd in "feed --limit 2" "users" "profile" "notify-count"; do
  run "$cmd (human)" 0 "$BIN" $cmd > /dev/null
done

echo "== post lifecycle =="
MARKER="cli-test-$(date +%s)-$$"
out=$(run "create" 0 "$BIN" --json create "$MARKER")
assert_valid_json "create shape" "$out"
POST_ID="$(json_field "$out" 'd["postId"]')"
if [ -z "$POST_ID" ]; then
  fail "create: no postId in response: $out"
else
  ok
  CLEANUP_POST_IDS+=("$POST_ID")

  out=$(run "post <id> --json" 0 "$BIN" --json post "$POST_ID")
  [ "$(json_field "$out" 'd["post"]["text"]')" = "$MARKER" ] && ok || fail "post text mismatch: $out"

  # The web UI hides the like button on your own posts; confirm the server
  # enforces the same rule rather than just the UI, and that the error
  # comes back as a clean, parseable envelope on both streams.
  out=$(run "like own post (must fail)" 1 "$BIN" --json like "$POST_ID")
  assert_valid_json "like-own-post error shape" "$out"
  [ "$(json_field "$out" 'd["ok"]')" = "False" ] && ok || fail "like-own-post: expected ok:false, got $out"

  # Unlike on a post you never liked is deliberately idempotent server-side
  # (postFound/wasLiked guard in api.php) - exit 0, not an error.
  run "unlike (idempotent, not liked)" 0 "$BIN" --json unlike "$POST_ID" > /dev/null

  out=$(run "comment" 0 "$BIN" --json comment "$POST_ID" "cli-test comment")
  COMMENT_ID="$(json_field "$out" 'd["commentId"]')"
  if [ -z "$COMMENT_ID" ]; then
    fail "comment: no commentId in response: $out"
  else
    ok
    out=$(run "comments --json" 0 "$BIN" --json comments "$POST_ID")
    assert_valid_json "comments shape" "$out"
    [ "$(json_field "$out" 'd["totalCount"]')" = "1" ] && ok || fail "comments totalCount: $out"
    run "delete-comment" 0 "$BIN" --json delete-comment "$COMMENT_ID" > /dev/null
  fi

  run "delete post" 0 "$BIN" --json delete "$POST_ID" > /dev/null
  CLEANUP_POST_IDS=()   # already deleted, don't double-delete in the trap

  out=$(run "post <id> after delete (must 404)" 1 "$BIN" --json post "$POST_ID")
  [ "$(json_field "$out" 'd["ok"]')" = "False" ] && ok || fail "post-after-delete shape: $out"
fi

echo "== error handling =="
run "unknown command" 1 "$BIN" this-is-not-a-command > /dev/null
run "missing required arg" 1 "$BIN" comment > /dev/null
run "--help" 0 "$BIN" --help > /dev/null

echo "== stream separation (--json goes to stdout, errors to stderr) =="
STDOUT_ONLY="$("$BIN" --json post "1.0" 2>/dev/null)"
assert_valid_json "stdout-only is clean JSON" "$STDOUT_ONLY"

echo "== session persistence across separate process runs =="
"$BIN" whoami > /dev/null 2>&1
run "second process, still logged in" 0 "$BIN" whoami > /dev/null

echo "== logout =="
run "logout" 0 "$BIN" logout > /dev/null
out=$(run "whoami after logout (must fail)" 1 "$BIN" --json whoami)
assert_valid_json "logged-out error shape" "$out"

echo
echo "=================================="
echo "  $PASS passed, $FAIL failed"
echo "=================================="
[ "$FAIL" -eq 0 ]

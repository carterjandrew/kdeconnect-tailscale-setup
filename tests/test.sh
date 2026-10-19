#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -uo pipefail

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
readonly PROJECT_DIR
readonly PROGRAM="$PROJECT_DIR/kdeconnect-tailscale"
TEST_COUNT=0

fail() {
    printf 'not ok %d - %s\n' "$TEST_COUNT" "$1"
    [[ -n ${2:-} ]] && printf '%s\n' "$2" >&2
    exit 1
}

pass() {
    printf 'ok %d - %s\n' "$TEST_COUNT" "$1"
}

assert_contains() {
    local haystack=$1 needle=$2 message=$3
    [[ $haystack == *"$needle"* ]] || fail "$message" "Expected to find: $needle
Actual output: $haystack"
}

assert_not_contains() {
    local haystack=$1 needle=$2 message=$3
    [[ $haystack != *"$needle"* ]] || fail "$message" "Did not expect to find: $needle
Actual output: $haystack"
}

run_test() {
    TEST_COUNT=$((TEST_COUNT + 1))
    "$@"
}

setup() {
    TEST_DIR=$(mktemp -d)
    MOCK_BIN="$TEST_DIR/bin"
    BUSCTL_LOG="$TEST_DIR/busctl.log"
    mkdir -p "$MOCK_BIN"
    : >"$BUSCTL_LOG"

    cat >"$MOCK_BIN/tailscale" <<'EOF'
#!/usr/bin/env bash
if [[ ${1:-} == status && ${2:-} == --json ]]; then
    printf '%s\n' "$MOCK_TAILSCALE_STATUS"
else
    printf 'unexpected tailscale arguments\n' >&2
    exit 2
fi
EOF
    cat >"$MOCK_BIN/busctl" <<'EOF'
#!/usr/bin/env bash
printf '%s' "$1" >>"$BUSCTL_LOG"
shift
printf ' %s' "$@" >>"$BUSCTL_LOG"
printf '\n' >>"$BUSCTL_LOG"
if [[ " $* " == *" get-property "* ]]; then
    printf '%s\n' "$MOCK_CUSTOM_DEVICES"
elif [[ -n ${MOCK_BUSCTL_FAILURE:-} ]]; then
    printf '%s\n' "$MOCK_BUSCTL_FAILURE" >&2
    exit 1
fi
EOF
    chmod +x "$MOCK_BIN/tailscale" "$MOCK_BIN/busctl"

    export TEST_DIR MOCK_BIN BUSCTL_LOG
    export PATH="$MOCK_BIN:$PATH"
    export MOCK_TAILSCALE_STATUS='{
      "BackendState":"Running",
      "Peer":{
        "nodekey:offline":{"HostName":"Zeta","DNSName":"zeta.ts.net.","OS":"android","Online":false,"TailscaleIPs":["100.80.0.3"]},
        "nodekey:desktop":{"HostName":"Desktop","OS":"linux","Online":true,"TailscaleIPs":["100.80.0.2"]},
        "nodekey:phone":{"HostName":"Phone","DNSName":"phone.ts.net.","OS":"Android","Online":true,"TailscaleIPs":["fd7a:115c:a1e0::1","100.70.1.2"]}
      }
    }'
    export MOCK_CUSTOM_DEVICES='{"type":"as","data":["192.168.1.5"]}'
}

teardown() {
    rm -rf -- "$TEST_DIR"
}

test_peers_filters_and_sorts() {
    local output
    setup
    output=$($PROGRAM peers) || fail "peers filters and sorts" "$output"
    assert_contains "$output" "1. Phone  100.70.1.2  online" "peers filters and sorts"
    assert_contains "$output" "2. Zeta  100.80.0.3  offline" "peers filters and sorts"
    assert_not_contains "$output" "Desktop" "peers filters and sorts"
    teardown
    pass "peers filters and sorts"
}

test_add_preserves_existing_address() {
    local output log
    setup
    output=$($PROGRAM add phone) || fail "add preserves existing address" "$output"
    log=$(<"$BUSCTL_LOG")
    assert_contains "$log" "set-property $DBUS_SERVICE_PLACEHOLDER" "add preserves existing address"
    assert_contains "$log" "customDevices as 2 192.168.1.5 100.70.1.2" "add preserves existing address"
    assert_contains "$log" "forceOnNetworkChange" "add preserves existing address"
    teardown
    pass "add preserves existing address"
}

test_add_existing_only_refreshes() {
    local log
    setup
    export MOCK_CUSTOM_DEVICES='{"type":"as","data":["100.70.1.2"]}'
    $PROGRAM add nodekey:phone >/dev/null || fail "add existing only refreshes"
    log=$(<"$BUSCTL_LOG")
    assert_not_contains "$log" "set-property" "add existing only refreshes"
    assert_contains "$log" "forceOnNetworkChange" "add existing only refreshes"
    teardown
    pass "add existing only refreshes"
}

test_repair_replaces_and_deduplicates() {
    local log
    setup
    export MOCK_CUSTOM_DEVICES='{"type":"as","data":["100.70.1.1","192.168.1.5","100.70.1.2"]}'
    $PROGRAM repair 100.70.1.1 phone >/dev/null || fail "repair replaces and deduplicates"
    log=$(<"$BUSCTL_LOG")
    assert_contains "$log" "customDevices as 2 100.70.1.2 192.168.1.5" "repair replaces and deduplicates"
    teardown
    pass "repair replaces and deduplicates"
}

test_repair_rejects_unknown_address() {
    local output status
    setup
    set +e
    output=$($PROGRAM repair 100.90.9.9 phone 2>&1)
    status=$?
    set -e
    ((status != 0)) || fail "repair rejects unknown address"
    assert_contains "$output" "is not a configured KDE Connect custom address" "repair rejects unknown address"
    teardown
    pass "repair rejects unknown address"
}

test_tailscale_backend_must_run() {
    local output status
    setup
    export MOCK_TAILSCALE_STATUS='{"BackendState":"Stopped","Peer":{}}'
    set +e
    output=$($PROGRAM peers 2>&1)
    status=$?
    set -e
    ((status != 0)) || fail "Tailscale backend must run"
    assert_contains "$output" "Tailscale is not running" "Tailscale backend must run"
    teardown
    pass "Tailscale backend must run"
}

if ! command -v jq >/dev/null 2>&1; then
    printf '1..0 # SKIP jq is required to run tests\n'
    exit 0
fi

# Used as a stable substring without repeating shell quoting in the assertion.
readonly DBUS_SERVICE_PLACEHOLDER="org.kde.kdeconnect"

printf '1..6\n'
run_test test_peers_filters_and_sorts
run_test test_add_preserves_existing_address
run_test test_add_existing_only_refreshes
run_test test_repair_replaces_and_deduplicates
run_test test_repair_rejects_unknown_address
run_test test_tailscale_backend_must_run

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

set -uo pipefail

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
readonly PROGRAM="$PROJECT_DIR/kdeconnect-tailscale"
readonly ORIGINAL_PATH=$PATH
TEST_COUNT=0

fail() {
    printf 'not ok %d - %s\n' "$TEST_COUNT" "$1" >&2
    exit 1
}

assert_contains() {
    [[ $1 == *"$2"* ]] || fail "expected '$2' in: $1"
}

assert_not_contains() {
    [[ $1 != *"$2"* ]] || fail "unexpected '$2' in: $1"
}

setup() {
    TEST_DIR=$(mktemp -d)
    MOCK_BIN="$TEST_DIR/bin"
    BUSCTL_LOG="$TEST_DIR/busctl.log"
    TAILSCALE_LOG="$TEST_DIR/tailscale.log"
    mkdir -p "$MOCK_BIN"
    : >"$BUSCTL_LOG"
    : >"$TAILSCALE_LOG"
    cat >"$MOCK_BIN/tailscale" <<'MOCK'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$TAILSCALE_LOG"
if [[ -n ${MOCK_TAILSCALE_FAILURE:-} ]]; then
    printf '%s\n' "$MOCK_TAILSCALE_FAILURE" >&2
    exit 1
fi
printf '%s\n' "$MOCK_TAILSCALE_STATUS"
MOCK
    cat >"$MOCK_BIN/busctl" <<'MOCK'
#!/usr/bin/env bash
printf '%s ' "$@" >>"$BUSCTL_LOG"
printf '\n' >>"$BUSCTL_LOG"
if [[ " $* " == *" ${MOCK_BUSCTL_FAILURE:-never} "* ]]; then
    printf 'mock busctl failure\n' >&2
    exit 1
fi
if [[ " $* " == *" get-property "* ]]; then
    printf '%s\n' "$MOCK_CUSTOM_DEVICES"
fi
MOCK
    chmod +x "$MOCK_BIN/tailscale" "$MOCK_BIN/busctl"
    export TEST_DIR MOCK_BIN BUSCTL_LOG TAILSCALE_LOG
    export PATH="$MOCK_BIN:$ORIGINAL_PATH"
    export MOCK_TAILSCALE_FAILURE="" MOCK_BUSCTL_FAILURE=""
    export MOCK_TAILSCALE_STATUS='{"BackendState":"Running","Peer":{
      "nodekey:offline":{"HostName":"Zeta","OS":"android","Online":false,"TailscaleIPs":["100.80.0.3"]},
      "nodekey:desktop":{"HostName":"Desktop","OS":"linux","Online":true,"TailscaleIPs":["100.80.0.2"]},
      "nodekey:phone":{"HostName":"Phone","OS":"Android","Online":true,"TailscaleIPs":["fd7a::1","100.70.1.2"]},
      "nodekey:v6":{"HostName":"IPv6-only","OS":"other","TailscaleIPs":["fd7a::2"]}
    }}'
    export MOCK_CUSTOM_DEVICES='{"type":"as","data":["192.168.1.5"]}'
}

teardown() {
    PATH=$ORIGINAL_PATH
    rm -rf -- "$TEST_DIR"
}

assert_no_updates() {
    local log
    log=$(<"$BUSCTL_LOG")
    assert_not_contains "$log" set-property
    assert_not_contains "$log" forceOnNetworkChange
}

expect_failure() {
    local expected_status=$1 message=$2 output status
    shift 2
    if output=$("$@" 2>&1); then
        fail "expected failure from $*"
    else
        status=$?
    fi
    [[ $status == "$expected_status" ]] || fail "expected status $expected_status, got $status: $output"
    assert_contains "$output" "$message"
}

# Use the real menu flow while supplying answers without a controlling terminal.
run_interactive() (
    source "$PROGRAM"
    prompt() {
        ((${#MOCK_SELECTIONS[@]} > 0)) || return 1
        printf -v "$1" '%s' "${MOCK_SELECTIONS[0]}"
        MOCK_SELECTIONS=("${MOCK_SELECTIONS[@]:1}")
    }
    main "$@"
)

test_discovery() {
    local output
    output=$($PROGRAM peers) || fail "list peers"
    [[ $output == $'Desktop  linux  100.80.0.2  online\nPhone  Android  100.70.1.2  online\nZeta  android  100.80.0.3  offline' ]] || fail "unexpected peer list: $output"
    MOCK_TAILSCALE_STATUS=$(jq '.Peer["nodekey:phone"].HostName = ""' <<<"$MOCK_TAILSCALE_STATUS")
    output=$($PROGRAM peers) || fail "unnamed peer"
    assert_contains "$output" 'nodekey:phone  Android  100.70.1.2'
    assert_no_updates
}

test_direct_add() {
    local address log
    for address in 100.70.1.2 10.0.0.8 fd7a::1 phone.example phone 1; do
        : >"$BUSCTL_LOG"
        $PROGRAM add "$address" >/dev/null || fail "add literal $address"
        log=$(<"$BUSCTL_LOG")
        assert_contains "$log" "customDevices as 2 192.168.1.5 $address"
        assert_contains "$log" forceOnNetworkChange
    done
    [[ ! -s $TAILSCALE_LOG ]] || fail "direct add queried Tailscale"
    MOCK_CUSTOM_DEVICES='{"type":"as","data":[]}'
    $PROGRAM add 10.0.0.8 >/dev/null || fail "add to empty addresses"
    assert_contains "$(<"$BUSCTL_LOG")" 'customDevices as 1 10.0.0.8'
}

test_no_tailscale() {
    local isolated_path="$TEST_DIR/isolated"
    mkdir -p "$isolated_path"
    ln -s "$(command -v bash)" "$isolated_path/bash"
    ln -s "$(command -v jq)" "$isolated_path/jq"
    ln -s "$MOCK_BIN/busctl" "$isolated_path/busctl"
    PATH=$isolated_path "$PROGRAM" add 10.0.0.8 >/dev/null || fail "add without tailscale installed"
    PATH=$isolated_path "$PROGRAM" repair 192.168.1.5 10.0.0.8 >/dev/null || fail "repair without tailscale installed"
    MOCK_TAILSCALE_STATUS='{"BackendState":"Stopped"}'
    $PROGRAM add 10.0.0.8 >/dev/null || fail "add with tailscale stopped"
    $PROGRAM repair 192.168.1.5 10.0.0.8 >/dev/null || fail "repair with tailscale stopped"
    [[ ! -s $TAILSCALE_LOG ]] || fail "direct commands queried Tailscale"
}

test_unchanged() {
    local log
    $PROGRAM add 192.168.1.5 >/dev/null || fail "already configured add"
    $PROGRAM repair 192.168.1.5 192.168.1.5 >/dev/null || fail "unchanged repair"
    log=$(<"$BUSCTL_LOG")
    assert_not_contains "$log" set-property
    assert_contains "$log" forceOnNetworkChange
}

test_repair() {
    MOCK_CUSTOM_DEVICES='{"type":"as","data":["100.70.1.1","192.168.1.5","100.70.1.2","100.70.1.1"]}'
    $PROGRAM repair 100.70.1.1 100.70.1.2 >/dev/null || fail "repair and deduplicate"
    assert_contains "$(<"$BUSCTL_LOG")" 'customDevices as 2 100.70.1.2 192.168.1.5'
    : >"$BUSCTL_LOG"
    expect_failure 1 'is not a configured' "$PROGRAM" repair 1 10.0.0.8
    assert_no_updates
}

test_menus() {
    MOCK_SELECTIONS=(2)
    run_interactive >/dev/null || fail "default interactive add"
    assert_contains "$(<"$BUSCTL_LOG")" 'customDevices as 2 192.168.1.5 100.70.1.2'
    : >"$BUSCTL_LOG"
    MOCK_SELECTIONS=(1)
    run_interactive add >/dev/null || fail "non-Android selection"
    assert_contains "$(<"$BUSCTL_LOG")" 'customDevices as 2 192.168.1.5 100.80.0.2'
    : >"$BUSCTL_LOG"
    MOCK_TAILSCALE_STATUS=$(jq '.Peer["nodekey:offline"].HostName = "Phone"' <<<"$MOCK_TAILSCALE_STATUS")
    MOCK_SELECTIONS=(1 3)
    run_interactive repair >/dev/null || fail "repair with duplicate peer names"
    assert_contains "$(<"$BUSCTL_LOG")" 'customDevices as 1 100.80.0.3'
}

test_invalid_input() {
    local input
    expect_failure 2 'invalid command' "$PROGRAM" repair 192.168.1.5
    expect_failure 2 'invalid command' "$PROGRAM" add --peer phone
    expect_failure 2 'invalid command' "$PROGRAM" repair --address x --peer phone
    expect_failure 2 'invalid command' "$PROGRAM" peers extra
    expect_failure 2 'invalid command' "$PROGRAM" unknown
    for input in '' --peer '-x' 'a b' $'a\nb' $'a\tb' $'a\x7fb'; do
        expect_failure 2 'addresses must' "$PROGRAM" add "$input"
        expect_failure 2 'addresses must' "$PROGRAM" repair "$input" target
    done
    for input in '' 0 01 4 -1 phone 100.70.1.2 999999999999999999999999999999999; do
        MOCK_SELECTIONS=("$input")
        expect_failure 1 'invalid menu number' run_interactive add
        expect_failure 1 'invalid menu number' run_interactive repair
    done
    MOCK_SELECTIONS=(1 bad)
    expect_failure 1 'invalid menu number' run_interactive repair
    assert_no_updates
}

test_empty_discovery() {
    local status output
    for status in '{"BackendState":"Running"}' '{"BackendState":"Running","Peer":null}' '{"BackendState":"Running","Peer":{}}' \
        '{"BackendState":"Running","Peer":{"v6":{"TailscaleIPs":["fd7a::1"]}}}'; do
        MOCK_TAILSCALE_STATUS=$status
        output=$($PROGRAM peers) || fail "empty peers should list successfully"
        assert_contains "$output" 'Use add ADDRESS'
        MOCK_SELECTIONS=()
        expect_failure 1 'Use add ADDRESS' run_interactive add
    done
    MOCK_CUSTOM_DEVICES='{"type":"as","data":[]}'
    expect_failure 1 'no options available' run_interactive repair
    assert_no_updates
}

test_read_failures() {
    local status
    for status in invalid '{"BackendState":"Stopped"}' '{"BackendState":"Running","Peer":7}'; do
        MOCK_TAILSCALE_STATUS=$status
        expect_failure 1 'could not read Tailscale peers' "$PROGRAM" peers
    done
    MOCK_TAILSCALE_FAILURE='tailscaled unavailable'
    expect_failure 1 'tailscale status failed' "$PROGRAM" peers
    MOCK_SELECTIONS=(1)
    expect_failure 1 'tailscale status failed' run_interactive add
    MOCK_BUSCTL_FAILURE=get-property
    expect_failure 1 'could not read KDE Connect' "$PROGRAM" add target
    MOCK_BUSCTL_FAILURE=''
    for status in invalid '{"type":"as","data":null}' '{"type":"as","data":[2]}' \
        '{"type":"s","data":["x"]}' '{"type":"as","data":[""]}' '{"type":"as","data":["a\nb"]}'; do
        MOCK_CUSTOM_DEVICES=$status
        expect_failure 1 'unsupported KDE Connect' "$PROGRAM" add target
    done
    assert_no_updates
}

test_write_failures() {
    MOCK_BUSCTL_FAILURE=set-property
    expect_failure 1 'could not update' "$PROGRAM" add target
    assert_not_contains "$(<"$BUSCTL_LOG")" forceOnNetworkChange
    : >"$BUSCTL_LOG"
    MOCK_BUSCTL_FAILURE=call
    expect_failure 1 'addresses are configured, but' "$PROGRAM" add target
    assert_contains "$(<"$BUSCTL_LOG")" set-property
    assert_contains "$(<"$BUSCTL_LOG")" forceOnNetworkChange
}

if ! command -v jq >/dev/null 2>&1; then
    printf '1..0 # SKIP jq is required to run tests\n'
    exit 0
fi

tests=(test_discovery test_direct_add test_no_tailscale test_unchanged test_repair
       test_menus test_invalid_input test_empty_discovery test_read_failures test_write_failures)
printf '1..%d\n' "${#tests[@]}"
for test in "${tests[@]}"; do
    TEST_COUNT=$((TEST_COUNT + 1))
    setup
    "$test" || fail "$test"
    teardown
    printf 'ok %d - %s\n' "$TEST_COUNT" "$test"
done

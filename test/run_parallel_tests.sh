#!/usr/bin/env bash

# Repeatedly runs a test in parallel until its first failure, prints a short
# backtrace. This helps reproduce intermittent CI failures.

NUM_PARALLEL=100

usage() {
    printf 'Usage: %s [-j JOBS] TEST_EXECUTABLE\n' "${0##*/}"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -j|--jobs)
            if [ "$#" -lt 2 ]; then
                printf '%s: %s requires a value\n' "${0##*/}" "$1" >&2
                usage >&2
                exit 2
            fi
            NUM_PARALLEL=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            printf '%s: unknown option: %s\n' "${0##*/}" "$1" >&2
            usage >&2
            exit 2
            ;;
        *)
            break
            ;;
    esac
done

if [ "$#" -ne 1 ]; then
    usage >&2
    exit 2
fi

if [[ ! $NUM_PARALLEL =~ ^[1-9][0-9]*$ ]]; then
    printf '%s: jobs must be a positive integer: %s\n' "${0##*/}" "$NUM_PARALLEL" >&2
    exit 2
fi

TEST_BIN=$1
case "$TEST_BIN" in
    */*) ;;
    *) TEST_BIN="./$TEST_BIN" ;;
esac

if [ ! -f "$TEST_BIN" ] || [ ! -x "$TEST_BIN" ]; then
    printf '%s: not an executable file: %s\n' "${0##*/}" "$1" >&2
    exit 1
fi

GDB=$(command -v gdb 2>/dev/null || true)
TEST_DIR=${TEST_BIN%/*}
TEST_NAME=${TEST_BIN##*/}
DEBUG_BIN=$TEST_BIN
if [ -x "$TEST_DIR/.libs/$TEST_NAME" ]; then
    DEBUG_BIN="$TEST_DIR/.libs/$TEST_NAME"
fi

if [ ! -r /proc/sys/kernel/core_pattern ] ||
   [ ! -r /proc/sys/kernel/core_uses_pid ] ||
   [ "$(< /proc/sys/kernel/core_pattern)" != "core" ] ||
   [ "$(< /proc/sys/kernel/core_uses_pid)" != "1" ]; then
    printf '%s: automatic core handling requires core_pattern=core and core_uses_pid=1\n' \
        "${0##*/}" >&2
    exit 1
fi

if ! ulimit -c unlimited; then
    printf '%s: cannot enable core dumps\n' "${0##*/}" >&2
    exit 1
fi

if ! STATE_DIR=$(mktemp -d); then
    printf '%s: cannot create temporary directory\n' "${0##*/}" >&2
    exit 1
fi

CORE_DIR=$PWD
FAILURE_LOG="$STATE_DIR/first_failure.log"
FAILURE_PID="$STATE_DIR/first_failure.pid"
PARENT_PID=$BASHPID
failure_notified=0
FIRST_FAILURE_PID=
declare -A PREEXISTING_CORES=()

for core_file in "$CORE_DIR"/core.*; do
    if [ -e "$core_file" ] || [ -L "$core_file" ]; then
        PREEXISTING_CORES["$core_file"]=1
    fi
done

stop_workers() {
    local pid
    local job_pids=()

    mapfile -t job_pids < <(jobs -p)
    if [ "${#job_pids[@]}" -ne 0 ]; then
        for pid in "${job_pids[@]}"; do
            kill -KILL -- "-$pid" 2>/dev/null || true
        done
        wait "${job_pids[@]}" 2>/dev/null || true
    fi
}

remove_other_cores() {
    local pid_file pid core_file

    for pid_file in "$STATE_DIR"/instance_*.pids; do
        [ -f "$pid_file" ] || continue
        while IFS= read -r pid; do
            [[ $pid =~ ^[1-9][0-9]*$ ]] || continue
            [ "$pid" = "$FIRST_FAILURE_PID" ] && continue

            core_file="$CORE_DIR/core.$pid"
            if [ -f "$core_file" ] &&
               [[ ! ${PREEXISTING_CORES[$core_file]+present} ]]; then
                rm -f -- "$core_file" ||
                    printf '%s: cannot remove core file: %s\n' \
                        "${0##*/}" "$core_file" >&2
            fi
        done < "$pid_file"
    done
}

cleanup() {
    trap '' HUP INT TERM USR1
    stop_workers
    remove_other_cores
    rm -rf -- "$STATE_DIR"
}

handle_signal() {
    local status=$1

    if [ "$BASHPID" -eq "$PARENT_PID" ]; then
        cleanup
    fi
    exit "$status"
}

run_test() {
    local instance=$1
    local log_file="$STATE_DIR/instance_${instance}.log"
    local pid_file="$STATE_DIR/instance_${instance}.pids"
    local test_pid=

    set +m
    while :; do
        VLC_TEST_TIMEOUT=-1 "$TEST_BIN" > "$log_file" 2>&1 &
        test_pid=$!
        if ! printf '%s\n' "$test_pid" > "$pid_file"; then
            kill -KILL "$test_pid" 2>/dev/null || true
            wait "$test_pid" 2>/dev/null || true
            printf '%s: cannot record test process ID\n' "${0##*/}" > "$log_file"
            break
        fi

        if wait "$test_pid" 2>> "$log_file"; then
            rm -f -- "$log_file" "$pid_file"
        else
            break
        fi
    done

    if ln -s -- "${log_file##*/}" "$FAILURE_LOG" 2>/dev/null; then
        if ! printf '%s\n' "$test_pid" > "$FAILURE_PID"; then
            printf '%s: cannot record first failure process ID\n' "${0##*/}" >> "$log_file"
        fi
        kill -USR1 "$PARENT_PID" 2>/dev/null || true
    fi
    return 1
}

trap 'handle_signal 129' HUP
trap 'handle_signal 130' INT
trap 'handle_signal 143' TERM
trap 'failure_notified=1' USR1

printf 'Starting %s parallel test instances. Press Ctrl+C to stop.\n' "$NUM_PARALLEL"

# Put each worker in its own process group so hung test children can be stopped.
set -m
for ((i = 1; i <= NUM_PARALLEL; i++)); do
    [ "$failure_notified" -eq 0 ] || break
    run_test "$i" &
done
set +m

while [ "$failure_notified" -eq 0 ]; do
    wait -n 2>/dev/null || true
    [ "$failure_notified" -ne 0 ] || [ -n "$(jobs -p)" ] || break
done
stop_workers

if [ ! -L "$FAILURE_LOG" ] ||
   ! IFS= read -r FIRST_FAILURE_PID < "$FAILURE_PID" ||
   [[ ! $FIRST_FAILURE_PID =~ ^[1-9][0-9]*$ ]]; then
    printf '%s: a worker stopped without recording a failure\n' "${0##*/}" >&2
    cleanup
    exit 1
fi

printf '\nFirst failure:\n'
cat -- "$FAILURE_LOG"

FIRST_CORE="$CORE_DIR/core.$FIRST_FAILURE_PID"
if [ -f "$FIRST_CORE" ]; then
    if [ -n "$GDB" ]; then
        "$GDB" -q --batch "$DEBUG_BIN" -c "$FIRST_CORE" \
            -ex 'set pagination off' \
            -ex 'bt' || true
    fi
    cleanup
    printf '\nFull manual inspection:\ngdb %q -c %q -ex '\''thread apply all bt full'\''\n' \
        "$DEBUG_BIN" "$FIRST_CORE"
else
    printf '\n%s: no core dump found for PID %s\n' \
        "${0##*/}" "$FIRST_FAILURE_PID" >&2
    cleanup
fi

exit 1

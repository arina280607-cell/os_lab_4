#!/usr/bin/env bash
# Run from the project root: bash tests/check_launch_failure.sh
set -u

if [[ ! -x ./build/parent || ! -x ./build/child1 ]]; then
    printf 'First run make in the project root.\n' >&2
    exit 1
fi
if ! command -v timeout >/dev/null 2>&1; then
    printf 'Required command not found: timeout\n' >&2
    exit 1
fi

test_dir=$(mktemp -d /tmp/os_lab4_launch_XXXXXX) || exit 1
cleanup()
{
    case "$test_dir" in
        /tmp/os_lab4_launch_*) rm -rf -- "$test_dir" ;;
    esac
}
trap cleanup EXIT

mkdir "$test_dir/build" || exit 1
cp ./build/parent ./build/child1 "$test_dir/build/" || exit 1
printf 'hello   world\n' > "$test_dir/input"

printf 'Testing a missing child2 executable; allow about 30 seconds.\n'
if (
    cd "$test_dir" || exit 1
    timeout 40s ./build/parent < input > actual 2> log
); then
    status=0
else
    status=$?
fi

cat "$test_dir/log"
printf 'Parent/test exit status: %d\n' "$status"

if [[ "$status" -ne 1 ]]; then
    printf 'FAIL: expected exit status 1 (124 means test timeout).\n' >&2
    exit 1
fi
if ! grep -q 'exited with code 127' "$test_dir/log"; then
    printf 'FAIL: missing evidence of exec failure (code 127).\n' >&2
    exit 1
fi

shared_path=$(awk '/^\[parent\].*shared file:/ { print $NF; exit }' "$test_dir/log")
if [[ "$shared_path" != /tmp/os_lab4_* ]]; then
    printf 'FAIL: temporary shared file path was not found.\n' >&2
    exit 1
fi
if [[ -e "$shared_path" ]]; then
    printf 'FAIL: shared file remains: %s\n' "$shared_path" >&2
    exit 1
fi
if [[ -s "$test_dir/actual" ]]; then
    printf 'FAIL: unexpected result output after processing failure.\n' >&2
    exit 1
fi

printf 'PASS: missing child2 detected; failure status and file cleanup verified.\n'

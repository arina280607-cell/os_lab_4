#!/usr/bin/env bash
# Run from the project root: bash tests/check_basic.sh
set -u

if [[ ! -x ./build/parent ]]; then
    printf 'First run make in the project root.\n' >&2
    exit 1
fi
if ! command -v timeout >/dev/null 2>&1; then
    printf 'Required command not found: timeout\n' >&2
    exit 1
fi

work_dir=$(mktemp -d /tmp/os_lab4_checks_XXXXXX) || exit 1
trap 'rm -rf -- "$work_dir"' EXIT
passed=0
failed=0

check_removed_file()
{
    local log=$1
    local path
    path=$(awk '/^\[parent\].*shared file:/ { print $NF; exit }' "$log")
    if [[ "$path" != /tmp/os_lab4_* ]]; then
        printf '  Could not find the temporary file path in diagnostics.\n' >&2
        return 1
    fi
    if [[ -e "$path" ]]; then
        printf '  Temporary file still exists: %s\n' "$path" >&2
        return 1
    fi
    return 0
}

run_case()
{
    local name=$1
    local status
    if timeout 15s ./build/parent < "$work_dir/input" \
        > "$work_dir/actual" 2> "$work_dir/log"; then
        status=0
    else
        status=$?
    fi

    if [[ "$status" -eq 0 ]] && \
        cmp -s "$work_dir/expected" "$work_dir/actual" && \
        check_removed_file "$work_dir/log"; then
        printf 'PASS: %s\n' "$name"
        passed=$((passed + 1))
    else
        printf 'FAIL: %s (exit=%s)\n' "$name" "$status" >&2
        diff -u "$work_dir/expected" "$work_dir/actual" >&2 || true
        cat "$work_dir/log" >&2
        failed=$((failed + 1))
    fi
}

printf 'hello   мир\n\none    two\n' > "$work_dir/input"
printf 'HELLO МИР\n\nONE TWO\n' > "$work_dir/expected"
run_case 'multiple lines, Cyrillic and empty line'

: > "$work_dir/input"
: > "$work_dir/expected"
run_case 'empty input (immediate EOF)'

printf 'hello   мир' > "$work_dir/input"
printf 'HELLO МИР' > "$work_dir/expected"
run_case 'last line without newline'

printf '    \n  a   b  \n' > "$work_dir/input"
printf ' \n A B \n' > "$work_dir/expected"
run_case 'spaces only, leading and trailing spaces'

printf 'a\t\tb   c\n' > "$work_dir/input"
printf 'A\t\tB C\n' > "$work_dir/expected"
run_case 'tabs are preserved'

awk 'BEGIN { for (i=0;i<20000;i++) printf "a"; printf "   b\n" }' > "$work_dir/input"
awk 'BEGIN { for (i=0;i<20000;i++) printf "A"; printf " B\n" }' > "$work_dir/expected"
run_case 'long line (more than 4096 bytes)'

awk 'BEGIN { for (i=0;i<10000;i++) printf "я"; printf "   ёж\n" }' > "$work_dir/input"
awk 'BEGIN { for (i=0;i<10000;i++) printf "Я"; printf " ЁЖ\n" }' > "$work_dir/expected"
run_case 'long Cyrillic line'

awk 'BEGIN { for (i=0;i<255;i++) printf "a"; printf "       мир\n" }' > "$work_dir/input"
awk 'BEGIN { for (i=0;i<255;i++) printf "A"; printf " МИР\n" }' > "$work_dir/expected"
run_case 'spaces at a chunk boundary on the current Linux platform'

awk 'BEGIN { for (i=0;i<255;i++) printf "a"; printf "я   ё\n" }' > "$work_dir/input"
awk 'BEGIN { for (i=0;i<255;i++) printf "A"; printf "Я Ё\n" }' > "$work_dir/expected"
run_case 'multibyte character near a chunk boundary'

printf '\377\n' > "$work_dir/input"
if timeout 15s ./build/parent < "$work_dir/input" \
    > "$work_dir/actual" 2> "$work_dir/log"; then
    status=0
else
    status=$?
fi
if [[ "$status" -eq 1 ]] && check_removed_file "$work_dir/log"; then
    printf 'PASS: invalid UTF-8 is rejected and temporary file removed\n'
    passed=$((passed + 1))
else
    printf 'FAIL: invalid UTF-8 (exit=%s)\n' "$status" >&2
    cat "$work_dir/log" >&2
    failed=$((failed + 1))
fi

printf '\nPassed: %d; failed: %d\n' "$passed" "$failed"
if [[ "$failed" -ne 0 ]]; then
    exit 1
fi

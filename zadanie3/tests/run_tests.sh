#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TMP_DIR="$(mktemp -d)"
PASS_COUNT=0
FAIL_COUNT=0

pass() {
    printf 'PASS %s\n' "$1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

fail() {
    printf 'FAIL %s\n' "$1"
    if [ $# -ge 2 ]; then
        printf '  %s\n' "$2"
    fi
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

compile_binary() {
    if (
        cd "$PROJECT_ROOT" || exit 1
        make compile > "$TMP_DIR/compile.log" 2>&1
    ); then
        pass "build compile"
    else
        fail "build compile" "kompilacia zlyhala, pozri $TMP_DIR/compile.log"
        printf '\nVysledok: %d PASS, %d FAIL\n' "$PASS_COUNT" "$FAIL_COUNT"
        exit 1
    fi
}

run_case() {
    local input_file="$1"
    local output_file="$2"

    (
        cd "$PROJECT_ROOT" || exit 1
        ./zadanie3.exe < "$input_file" > "$output_file" 2>&1
    )
}

expect_output() {
    local test_name="$1"
    local input_text="$2"
    local expected_text="$3"
    local input_file="$TMP_DIR/input.txt"
    local output_file="$TMP_DIR/output.txt"
    local expected_file="$TMP_DIR/expected.txt"
    local diff_file="$TMP_DIR/diff.txt"

    printf '%s' "$input_text" > "$input_file"
    printf '%s' "$expected_text" > "$expected_file"
    run_case "$input_file" "$output_file"

    if cmp -s "$expected_file" "$output_file"; then
        pass "$test_name"
    else
        diff -u "$expected_file" "$output_file" > "$diff_file" || true
        fail "$test_name" "vystup nesedi, diff je v $diff_file"
    fi
}

test_empty_and_unknown_commands() {
    expect_output \
        "empty input and unknown command" \
        $'\nfoo\nquit\n' \
        $'chyba\n'
}

test_missing_and_extra_arguments() {
    expect_output \
        "missing and extra arguments" \
        $'touch\nmkdir\nrm\nvypis\nspusti\ncd\nchmod 5\nchown admin\nls a b\nquit now\nquit\n' \
        $'chyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\n'
}

test_basic_example_flow() {
    expect_output \
        "basic example from assignment" \
        $'ls\ntouch subor1\nls\nchmod 4 subor1\nls\nchown admin subor1\nls\nmkdir adr1\nls\nspusti subor1\nvypis subor1\nzapis subor1 ahoj\nchmod 5 adr1\nls adr1\nquit\n' \
        $'ziaden subor\nsubor1 balaz rwx\nsubor1 balaz r--\nsubor1 admin r--\nadr1 balaz rwx\nsubor1 admin r--\nchyba\nok\nchyba\nadr1 balaz r-x\n'
}

test_nested_paths_and_navigation() {
    expect_output \
        "nested paths with cd dot dot and slash" \
        $'mkdir a\nmkdir a/b\ntouch a/b/file\ncd a/b\nls\ncd ..\nls b\ncd /\nls a/b/file\nquit\n' \
        $'file balaz rwx\nb balaz rwx\nfile balaz rwx\n'
}

test_ls_prints_directory_info_only() {
    expect_output \
        "ls on directory prints only directory info" \
        $'mkdir adr1\ntouch adr1/subor1\nls adr1\nquit\n' \
        $'adr1 balaz rwx\n'
}

test_permissions_for_directory_and_file() {
    expect_output \
        "permission checks for directory and file" \
        $'mkdir adr1\nchmod 0 adr1\nls adr1\ncd adr1\ntouch adr1/subor\nchmod 0 /\nls\nmkdir x\nquit\n' \
        $'adr1 balaz ---\nchyba\nchyba\nchyba\nchyba\n'
}

test_read_write_execute_failures() {
    expect_output \
        "read write execute failures" \
        $'touch subor\nchmod 0 subor\nvypis subor\nspusti subor\nzapis subor data\nquit\n' \
        $'chyba\nchyba\nchyba\n'
}

test_type_errors() {
    expect_output \
        "wrong type operations" \
        $'touch file\nmkdir dir\ncd file\nvypis dir\nspusti dir\nquit\n' \
        $'chyba\nchyba\nchyba\n'
}

test_duplicate_and_invalid_names() {
    expect_output \
        "duplicate and invalid names" \
        $'touch a\ntouch a\nmkdir a\ntouch .\nmkdir ..\ntouch /\nquit\n' \
        $'chyba\nchyba\nchyba\nchyba\nchyba\n'
}

test_rm_recursive_and_root_protection() {
    expect_output \
        "rm recursive and root protection" \
        $'mkdir dir\ntouch dir/file\nrm dir\nls\nrm /\nquit\n' \
        $'ziaden subor\nchyba\n'
}

test_zapis_with_spaces_and_empty_content() {
    expect_output \
        "zapis with spaces and empty content" \
        $'touch text\nzapis text ahoj svet 123\nvypis text\nzapis text\nvypis text\nquit\n' \
        $'ahoj svet 123\nok\n'
}

test_swapped_arguments_for_chmod_and_chown() {
    expect_output \
        "swapped arguments chmod chown" \
        $'touch a\nchmod a 5\nchown a user1\nls\nquit\n' \
        $'a user1 r-x\n'
}

test_absolute_and_relative_paths() {
    expect_output \
        "absolute and relative paths" \
        $'mkdir dir\ntouch /dir/a\ncd /dir\nls\ncd /\nls /dir/a\nquit\n' \
        $'a balaz rwx\na balaz rwx\n'
}

test_nonexistent_targets() {
    expect_output \
        "nonexistent targets" \
        $'ls nic\nrm nic\nvypis nic\nspusti nic\ncd nic\nchmod 4 nic\nchown admin nic\nzapis nic text\nquit\n' \
        $'chyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\n'
}

test_invalid_permissions() {
    expect_output \
        "invalid chmod values" \
        $'touch a\nchmod 8 a\nchmod -1 a\nchmod xx a\nchmod 44 a\nquit\n' \
        $'chyba\nchyba\nchyba\nchyba\n'
}

test_path_parent_navigation_in_command() {
    expect_output \
        "path parent navigation in commands" \
        $'mkdir a\nmkdir a/b\ncd a/b\ntouch ../x\ncd ..\nls\nquit\n' \
        $'x balaz rwx\nb balaz rwx\n'
}

main() {
    compile_binary
    test_empty_and_unknown_commands
    test_missing_and_extra_arguments
    test_basic_example_flow
    test_nested_paths_and_navigation
    test_ls_prints_directory_info_only
    test_permissions_for_directory_and_file
    test_read_write_execute_failures
    test_type_errors
    test_duplicate_and_invalid_names
    test_rm_recursive_and_root_protection
    test_zapis_with_spaces_and_empty_content
    test_swapped_arguments_for_chmod_and_chown
    test_absolute_and_relative_paths
    test_nonexistent_targets
    test_invalid_permissions
    test_path_parent_navigation_in_command

    printf '\nVysledok: %d PASS, %d FAIL\n' "$PASS_COUNT" "$FAIL_COUNT"

    if [ "$FAIL_COUNT" -ne 0 ]; then
        exit 1
    fi
}

main

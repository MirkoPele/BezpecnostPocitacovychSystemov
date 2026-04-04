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

expect_output_unordered_lines() {
    local test_name="$1"
    local input_text="$2"
    local expected_text="$3"
    local input_file="$TMP_DIR/input.txt"
    local output_file="$TMP_DIR/output.txt"
    local expected_file="$TMP_DIR/expected.txt"
    local sorted_output_file="$TMP_DIR/output_sorted.txt"
    local sorted_expected_file="$TMP_DIR/expected_sorted.txt"
    local diff_file="$TMP_DIR/diff.txt"

    printf '%s' "$input_text" > "$input_file"
    printf '%s' "$expected_text" > "$expected_file"
    run_case "$input_file" "$output_file"

    sort "$output_file" > "$sorted_output_file"
    sort "$expected_file" > "$sorted_expected_file"

    if cmp -s "$sorted_expected_file" "$sorted_output_file"; then
        pass "$test_name"
    else
        diff -u "$sorted_expected_file" "$sorted_output_file" > "$diff_file" || true
        fail "$test_name" "vystup nesedi bez ohladu na poradie riadkov, diff je v $diff_file"
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
        $'touch\nmkdir\nrm\nvypis\nspusti\ncd\nzapis\nchmod 5\nchown admin\nls a b\nquit now\nquit\n' \
        $'chyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\nchyba\n'
}

test_basic_example_flow() {
    expect_output \
        "basic example from assignment" \
        $'ls\ntouch subor1\nls subor1\nchmod 4 subor1\nls subor1\nchown admin subor1\nls subor1\nmkdir adr1\nls adr1\nspusti subor1\nvypis subor1\nzapis subor1\nchmod 5 adr1\nls adr1\nquit\n' \
        $'ziaden subor\nsubor1 balaz rwx\nsubor1 balaz r--\nsubor1 admin r--\nadr1 balaz rwx\nchyba\nok\nchyba\nadr1 balaz r-x\n'
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
        $'touch subor\nchmod 0 subor\nvypis subor\nspusti subor\nzapis subor\nquit\n' \
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
        "zapis accepts only one argument and keeps file empty" \
        $'touch text\nzapis text ahoj svet 123\nvypis text\nzapis text\nvypis text\nquit\n' \
        $'chyba\nok\nok\n'
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
        $'ls nic\nrm nic\nvypis nic\nspusti nic\ncd nic\nchmod 4 nic\nchown admin nic\nzapis nic\nquit\n' \
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
        "parent navigation works only in cd" \
        $'mkdir a\nmkdir a/b\ncd a/b\ntouch ../x\ncd ..\nls\nquit\n' \
        $'chyba\nb balaz rwx\n'
}

test_listing_order_is_not_important() {
    expect_output_unordered_lines \
        "listing order is not important" \
        $'touch subor1\nmkdir adr1\nls\nquit\n' \
        $'adr1 balaz rwx\nsubor1 balaz rwx\n'
}

test_dot_is_not_supported() {
    expect_output \
        "dot path is not supported" \
        $'mkdir adr1\nls .\nchmod 3 .\ncd .\nquit\n' \
        $'chyba\nchyba\nchyba\n'
}

test_cd_parent_and_root_behavior() {
    expect_output \
        "cd parent and root behavior" \
        $'mkdir adr1\ncd adr1\ntouch subor\ncd ..\nls adr1/subor\ncd ..\ntouch rootfile\nls rootfile\nquit\n' \
        $'subor balaz rwx\nrootfile balaz rwx\n'
}

test_same_name_allowed_in_different_directories() {
    expect_output \
        "same name allowed in different directories" \
        $'mkdir adr1\ntouch file\ncd adr1\ntouch file\nls file\ncd ..\nls file\nquit\n' \
        $'file balaz rwx\nfile balaz rwx\n'
}

test_ls_on_file_and_directory_without_read_rights() {
    expect_output \
        "ls on file and directory prints metadata even without read right" \
        $'touch file\nmkdir adr\nchmod 0 file\nchmod 0 adr\nls file\nls adr\nquit\n' \
        $'file balaz ---\nadr balaz ---\n'
}

test_cd_parent_from_root_is_silent() {
    expect_output \
        "cd parent from root is silent" \
        $'cd ..\ntouch a\nls a\nquit\n' \
        $'a balaz rwx\n'
}

test_directory_can_be_target_of_zapis() {
    expect_output \
        "directory can be target of zapis" \
        $'mkdir adr1\nzapis adr1\nls adr1\nchmod 0 adr1\nzapis adr1\nquit\n' \
        $'adr1 balaz rwx\nchyba\n'
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
    test_listing_order_is_not_important
    test_dot_is_not_supported
    test_cd_parent_and_root_behavior
    test_same_name_allowed_in_different_directories
    test_ls_on_file_and_directory_without_read_rights
    test_cd_parent_from_root_is_silent
    test_directory_can_be_target_of_zapis

    printf '\nVysledok: %d PASS, %d FAIL\n' "$PASS_COUNT" "$FAIL_COUNT"

    if [ "$FAIL_COUNT" -ne 0 ]; then
        exit 1
    fi
}

main

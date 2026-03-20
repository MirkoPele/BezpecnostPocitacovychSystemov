#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FIXTURES_DIR="$SCRIPT_DIR/fixtures"
TMP_DIR="$(mktemp -d)"
HASH_KEY="moj_tajny_kluc"
PASS_COUNT=0
FAIL_COUNT=0

ORIGINAL_HESLA_PRESENT=0
ORIGINAL_INFO_PRESENT=0
ORIGINAL_HESLA_BACKUP="$TMP_DIR/original_hesla.csv"
ORIGINAL_INFO_BACKUP="$TMP_DIR/original_info.txt"

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

backup_originals() {
    if [ -f "$PROJECT_ROOT/hesla.csv" ]; then
        cp "$PROJECT_ROOT/hesla.csv" "$ORIGINAL_HESLA_BACKUP"
        ORIGINAL_HESLA_PRESENT=1
    fi

    if [ -f "$PROJECT_ROOT/info.txt" ]; then
        cp "$PROJECT_ROOT/info.txt" "$ORIGINAL_INFO_BACKUP"
        ORIGINAL_INFO_PRESENT=1
    fi
}

restore_originals() {
    if [ "$ORIGINAL_HESLA_PRESENT" -eq 1 ]; then
        cp "$ORIGINAL_HESLA_BACKUP" "$PROJECT_ROOT/hesla.csv"
    else
        rm -f "$PROJECT_ROOT/hesla.csv"
    fi

    if [ "$ORIGINAL_INFO_PRESENT" -eq 1 ]; then
        cp "$ORIGINAL_INFO_BACKUP" "$PROJECT_ROOT/info.txt"
    else
        rm -f "$PROJECT_ROOT/info.txt"
    fi

    rm -f "$PROJECT_ROOT/temp.csv"
}

cleanup() {
    restore_originals
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

expect_status_token() {
    local test_name="$1"
    local output_file="$2"
    local expected="$3"
    local actual

    actual="$(tr -cs '[:alnum:]_' '\n' < "$output_file" | tail -n 1)"

    if [ "$actual" = "$expected" ]; then
        pass "$test_name"
    else
        fail "$test_name" "ocakavany posledny token '$expected', realne '$actual'"
    fi
}

expect_file_equals() {
    local test_name="$1"
    local actual_file="$2"
    local expected_file="$3"
    local diff_file="$TMP_DIR/file_diff.txt"

    if cmp -s "$expected_file" "$actual_file"; then
        pass "$test_name"
    else
        diff -u "$expected_file" "$actual_file" > "$diff_file" || true
        fail "$test_name" "subor sa nezhoduje s ocakavanim, diff je v $diff_file"
    fi
}

expect_file_equals_ignoring_final_newline() {
    local test_name="$1"
    local actual_file="$2"
    local expected_file="$3"
    local normalized_actual="$TMP_DIR/normalized_actual.txt"
    local normalized_expected="$TMP_DIR/normalized_expected.txt"
    local diff_file="$TMP_DIR/file_diff.txt"

    perl -0pe 's/\n?\z/\n/' "$actual_file" > "$normalized_actual"
    perl -0pe 's/\n?\z/\n/' "$expected_file" > "$normalized_expected"

    if cmp -s "$normalized_expected" "$normalized_actual"; then
        pass "$test_name"
    else
        diff -u "$normalized_expected" "$normalized_actual" > "$diff_file" || true
        fail "$test_name" "subor sa nezhoduje ani po ignorovani finalneho newline, diff je v $diff_file"
    fi
}

expect_file_absent() {
    local test_name="$1"
    local file_path="$2"

    if [ ! -e "$file_path" ]; then
        pass "$test_name"
    else
        fail "$test_name" "subor stale existuje: $file_path"
    fi
}

expect_runtime_files() {
    local test_name="$1"
    local expected_hesla="$2"
    local expected_info="$3"

    expect_file_equals \
        "$test_name hesla.csv" \
        "$PROJECT_ROOT/hesla.csv" \
        "$expected_hesla"
    expect_file_equals \
        "$test_name info.txt" \
        "$PROJECT_ROOT/info.txt" \
        "$expected_info"
}

prepare_runtime_files() {
    local hesla_fixture="$1"
    local info_fixture="$2"

    cp "$hesla_fixture" "$PROJECT_ROOT/hesla.csv"
    cp "$info_fixture" "$PROJECT_ROOT/info.txt"
    rm -f "$PROJECT_ROOT/temp.csv"
}

prepare_original_runtime_files() {
    cp "$ORIGINAL_HESLA_BACKUP" "$PROJECT_ROOT/hesla.csv"
    cp "$ORIGINAL_INFO_BACKUP" "$PROJECT_ROOT/info.txt"
    rm -f "$PROJECT_ROOT/temp.csv"
}

run_program() {
    local output_file="$1"
    local user_name="$2"
    local plain_password="$3"
    local one_time_key="$4"

    (
        cd "$PROJECT_ROOT" || exit 1
        printf '%s\n%s\n%s\n' "$user_name" "$plain_password" "$one_time_key" | ./zadanie2.exe > "$output_file" 2>&1
    )
}

run_program_with_input_file() {
    local output_file="$1"
    local input_file="$2"

    (
        cd "$PROJECT_ROOT" || exit 1
        ./zadanie2.exe < "$input_file" > "$output_file" 2>&1
    )
}

hash_plaintext() {
    perl -e '
        my ($plain, $key) = @ARGV;
        my $out = "";
        my $key_len = length($key);
        for (my $i = 0; $i < length($plain); $i++) {
            $out .= sprintf("%02x", ord(substr($plain, $i, 1)) ^ ord(substr($key, $i % $key_len, 1)));
        }
        print $out;
    ' "$1" "$HASH_KEY"
}

build_expected_after_key_removal() {
    local source_file="$1"
    local user_name="$2"
    local one_time_key="$3"
    local output_file="$4"

    awk -F ':' -v OFS=':' -v target_user="$user_name" -v target_key="$one_time_key" '
        {
            if ($1 == target_user) {
                key_count = split($3, keys, ",")
                new_keys = ""
                removed = 0

                for (i = 1; i <= key_count; i++) {
                    if (!removed && keys[i] == target_key) {
                        removed = 1
                        continue
                    }

                    if (new_keys != "") {
                        new_keys = new_keys "," keys[i]
                    } else {
                        new_keys = keys[i]
                    }
                }

                print $1, $2, new_keys
            } else {
                print $0
            }
        }
    ' "$source_file" > "$output_file"
}

first_key_for_user() {
    local source_file="$1"
    local user_name="$2"

    awk -F ':' -v target_user="$user_name" '
        $1 == target_user {
            split($3, keys, ",")
            print keys[1]
            exit
        }
    ' "$source_file"
}

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

test_success_login() {
    local output_file="$TMP_DIR/success.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "heslo123" "2245"

    expect_status_token "success middle key output" "$output_file" "ok"
    expect_runtime_files \
        "success middle key updates only matching user" \
        "$FIXTURES_DIR/expected_admin_2245_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
    expect_file_absent "success leaves no temp.csv" "$PROJECT_ROOT/temp.csv"
}

test_success_first_key() {
    local output_file="$TMP_DIR/success_first.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "heslo123" "3350"

    expect_status_token "success first key output" "$output_file" "ok"
    expect_runtime_files \
        "success first key preserves order after removal" \
        "$FIXTURES_DIR/expected_admin_3350_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_success_last_key() {
    local output_file="$TMP_DIR/success_last.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "heslo123" "2882"

    expect_status_token "success last key output" "$output_file" "ok"
    expect_runtime_files \
        "success last key removes trailing key cleanly" \
        "$FIXTURES_DIR/expected_admin_2882_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_success_last_user() {
    local output_file="$TMP_DIR/success_last_user.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "anna" "kvetinka" "5334"

    expect_status_token "success last user output" "$output_file" "ok"
    expect_runtime_files \
        "success last user keeps previous rows intact" \
        "$FIXTURES_DIR/expected_anna_5334_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_success_distinct_user_middle() {
    local output_file="$TMP_DIR/success_distinct_middle.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_distinct.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "mirec" "tajomstvo" "2004"

    expect_status_token "success middle row distinct key output" "$output_file" "ok"
    expect_runtime_files \
        "success middle row distinct key removes only chosen key" \
        "$FIXTURES_DIR/expected_mirec_2004_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_wrong_password() {
    local output_file="$TMP_DIR/wrong_password.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "zleheslo" "2245"

    expect_status_token "wrong password output" "$output_file" "chyba"
    expect_runtime_files \
        "wrong password keeps runtime files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    expect_file_absent "wrong password leaves no temp.csv" "$PROJECT_ROOT/temp.csv"
}

test_wrong_user() {
    local output_file="$TMP_DIR/wrong_user.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "neexistuje" "heslo123" "2245"

    expect_status_token "wrong user output" "$output_file" "chyba"
    expect_runtime_files \
        "wrong user keeps runtime files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_wrong_key() {
    local output_file="$TMP_DIR/wrong_key.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "heslo123" "9999"

    expect_status_token "wrong key output" "$output_file" "chyba"
    expect_runtime_files \
        "wrong key keeps runtime files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_key_from_other_user_fails() {
    local output_file="$TMP_DIR/wrong_other_user_key.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_distinct.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "heslo123" "2001"

    expect_status_token "key from other user output" "$output_file" "chyba"
    expect_runtime_files \
        "key from other user does not modify files" \
        "$FIXTURES_DIR/hesla_distinct.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_password_of_other_user_fails() {
    local output_file="$TMP_DIR/wrong_other_password.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "tajomstvo" "2245"

    expect_status_token "password from other user output" "$output_file" "chyba"
    expect_runtime_files \
        "password from other user keeps files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_username_case_sensitive() {
    local output_file="$TMP_DIR/wrong_case_user.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "Admin" "heslo123" "2245"

    expect_status_token "username case sensitivity output" "$output_file" "chyba"
    expect_runtime_files \
        "username case sensitivity keeps files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_password_case_sensitive() {
    local output_file="$TMP_DIR/wrong_case_password.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "HESLO123" "2245"

    expect_status_token "password case sensitivity output" "$output_file" "chyba"
    expect_runtime_files \
        "password case sensitivity keeps files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_reused_key() {
    local first_output="$TMP_DIR/reuse_first.out"
    local second_output="$TMP_DIR/reuse_second.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$first_output" "admin" "heslo123" "2245"
    run_program "$second_output" "admin" "heslo123" "2245"

    expect_status_token "reused key output" "$second_output" "chyba"
    expect_runtime_files \
        "reused key does not change file again" \
        "$FIXTURES_DIR/expected_admin_2245_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_last_remaining_key() {
    local output_file="$TMP_DIR/last_key.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_admin_last_key.csv" \
        "$FIXTURES_DIR/info_admin_last_key.txt"
    run_program "$output_file" "admin" "heslo123" "2245"

    expect_status_token "last remaining key output" "$output_file" "ok"
    expect_runtime_files \
        "last remaining key can be removed to empty list" \
        "$FIXTURES_DIR/expected_admin_no_keys.csv" \
        "$FIXTURES_DIR/info_admin_last_key.txt"
}

test_empty_key_list_fails() {
    local output_file="$TMP_DIR/empty_key_list.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/expected_admin_no_keys.csv" \
        "$FIXTURES_DIR/info_admin_last_key.txt"
    run_program "$output_file" "admin" "heslo123" "2245"

    expect_status_token "empty key list output" "$output_file" "chyba"
    expect_runtime_files \
        "empty key list stays unchanged" \
        "$FIXTURES_DIR/expected_admin_no_keys.csv" \
        "$FIXTURES_DIR/info_admin_last_key.txt"
}

test_double_successive_logins_remove_two_keys() {
    local first_output="$TMP_DIR/double_success_first.out"
    local second_output="$TMP_DIR/double_success_second.out"
    local intermediate_expected="$TMP_DIR/intermediate_expected.csv"
    local final_expected="$TMP_DIR/final_expected.csv"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"

    build_expected_after_key_removal \
        "$FIXTURES_DIR/hesla_base.csv" \
        "admin" \
        "3350" \
        "$intermediate_expected"
    build_expected_after_key_removal \
        "$intermediate_expected" \
        "admin" \
        "2245" \
        "$final_expected"

    run_program "$first_output" "admin" "heslo123" "3350"
    run_program "$second_output" "admin" "heslo123" "2245"

    expect_status_token "double success second output" "$second_output" "ok"
    expect_runtime_files \
        "double success removes two keys in sequence" \
        "$final_expected" \
        "$FIXTURES_DIR/info_base.txt"
}

test_missing_hesla_file() {
    local output_file="$TMP_DIR/missing_hesla.out"

    rm -f "$PROJECT_ROOT/hesla.csv" "$PROJECT_ROOT/temp.csv"
    cp "$FIXTURES_DIR/info_base.txt" "$PROJECT_ROOT/info.txt"
    run_program "$output_file" "admin" "heslo123" "2245"

    expect_status_token "missing hesla.csv output" "$output_file" "chyba"
    expect_file_absent "missing hesla.csv leaves no temp.csv" "$PROJECT_ROOT/temp.csv"
    expect_file_equals \
        "missing hesla.csv keeps info.txt unchanged" \
        "$PROJECT_ROOT/info.txt" \
        "$FIXTURES_DIR/info_base.txt"
}

test_missing_final_newline_in_hesla() {
    local output_file="$TMP_DIR/no_newline_hesla.out"
    local no_newline_hesla="$TMP_DIR/hesla_without_final_newline.csv"

    perl -0pe 's/\n\z//' "$FIXTURES_DIR/hesla_base.csv" > "$no_newline_hesla"

    prepare_runtime_files \
        "$no_newline_hesla" \
        "$FIXTURES_DIR/info_base.txt"
    run_program "$output_file" "admin" "heslo123" "2245"

    expect_status_token "missing final newline in hesla output" "$output_file" "ok"
    expect_file_equals_ignoring_final_newline \
        "missing final newline in hesla is handled correctly hesla.csv" \
        "$PROJECT_ROOT/hesla.csv" \
        "$FIXTURES_DIR/expected_admin_2245_removed.csv"
    expect_file_equals \
        "missing final newline in hesla is handled correctly info.txt" \
        "$PROJECT_ROOT/info.txt" \
        "$FIXTURES_DIR/info_base.txt"
}

test_whitespace_padded_input() {
    local output_file="$TMP_DIR/whitespace_input.out"
    local input_file="$TMP_DIR/whitespace_input.txt"

    printf '   admin   \n \theslo123\t \n    2245   \n' > "$input_file"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program_with_input_file "$output_file" "$input_file"

    expect_status_token "whitespace padded input output" "$output_file" "ok"
    expect_runtime_files \
        "whitespace padded input authenticates correctly" \
        "$FIXTURES_DIR/expected_admin_2245_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_long_username_fails_cleanly() {
    local output_file="$TMP_DIR/long_username.out"
    local input_file="$TMP_DIR/long_username_input.txt"

    awk 'BEGIN { for (i = 0; i < 140; i++) printf "a"; printf "\nheslo123\n2245\n" }' > "$input_file"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program_with_input_file "$output_file" "$input_file"

    expect_status_token "long username output" "$output_file" "chyba"
    expect_runtime_files \
        "long username keeps files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_long_password_fails_cleanly() {
    local output_file="$TMP_DIR/long_password.out"
    local input_file="$TMP_DIR/long_password_input.txt"

    awk 'BEGIN { printf "admin\n"; for (i = 0; i < 140; i++) printf "b"; printf "\n2245\n" }' > "$input_file"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    run_program_with_input_file "$output_file" "$input_file"

    expect_status_token "long password output" "$output_file" "chyba"
    expect_runtime_files \
        "long password keeps files unchanged" \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
}

test_stale_temp_file_is_cleaned() {
    local output_file="$TMP_DIR/stale_temp.out"

    prepare_runtime_files \
        "$FIXTURES_DIR/hesla_base.csv" \
        "$FIXTURES_DIR/info_base.txt"
    printf 'stary obsah\n' > "$PROJECT_ROOT/temp.csv"
    run_program "$output_file" "ferko" "auto123" "5334"

    expect_status_token "stale temp file output" "$output_file" "ok"
    expect_runtime_files \
        "stale temp file does not break successful login" \
        "$FIXTURES_DIR/expected_ferko_5334_removed.csv" \
        "$FIXTURES_DIR/info_base.txt"
    expect_file_absent "stale temp file is removed after success" "$PROJECT_ROOT/temp.csv"
}

test_fixture_has_five_users() {
    local line_count

    line_count="$(wc -l < "$FIXTURES_DIR/hesla_base.csv")"

    if [ "$line_count" -ge 5 ]; then
        pass "fixture has at least five users"
    else
        fail "fixture has at least five users" "pocet riadkov je $line_count"
    fi
}

test_fixture_key_counts() {
    local result

    result="$(
        awk -F ':' '
            NF != 3 { print "zly_format"; exit 1 }
            {
                key_count = split($3, arr, ",")
                if (key_count < 10) {
                    print $1 ":" key_count
                    exit 1
                }
            }
        ' "$FIXTURES_DIR/hesla_base.csv" 2>/dev/null
    )"

    if [ $? -eq 0 ]; then
        pass "fixture has at least ten keys per user"
    else
        fail "fixture has at least ten keys per user" "problemovy riadok: ${result:-neznamy}"
    fi
}

test_project_files_exist() {
    if [ "$ORIGINAL_HESLA_PRESENT" -eq 1 ] && [ "$ORIGINAL_INFO_PRESENT" -eq 1 ]; then
        pass "project files exist"
    else
        fail "project files exist" "chyba hesla.csv alebo info.txt"
    fi
}

test_project_info_integrity() {
    local result

    result="$(
        awk -F ':' '
            NF != 2 { print "zly_format_riadku:" NR; exit 1 }
            $1 == "" || $2 == "" { print "prazdne_pole:" NR; exit 1 }
            seen[$1]++ { print "duplikovane_meno:" $1; exit 1 }
            END {
                if (NR < 5) {
                    print "malo_pouzivatelov:" NR
                    exit 1
                }
            }
        ' "$ORIGINAL_INFO_BACKUP" 2>/dev/null
    )"

    if [ $? -eq 0 ]; then
        pass "project info.txt format and user count"
    else
        fail "project info.txt format and user count" "problem: ${result:-neznamy}"
    fi
}

test_project_hesla_integrity() {
    local result

    result="$(
        awk -F ':' '
            NF != 3 { print "zly_format_riadku:" NR; exit 1 }
            $1 == "" || $2 == "" || $3 == "" { print "prazdne_pole:" NR; exit 1 }
            seen[$1]++ { print "duplikovane_meno:" $1; exit 1 }
            {
                key_count = split($3, keys, ",")
                if (key_count < 10) {
                    print "malo_klucov:" $1 ":" key_count
                    exit 1
                }
                delete key_seen
                for (i = 1; i <= key_count; i++) {
                    if (keys[i] == "") {
                        print "prazdny_kluc:" $1
                        exit 1
                    }
                    if (keys[i] !~ /^[0-9]{4,}$/) {
                        print "zly_format_kluca:" $1 ":" keys[i]
                        exit 1
                    }
                    if (key_seen[keys[i]]++) {
                        print "duplikovany_kluc_v_ramci_pouzivatela:" $1 ":" keys[i]
                        exit 1
                    }
                }
            }
            END {
                if (NR < 5) {
                    print "malo_pouzivatelov:" NR
                    exit 1
                }
            }
        ' "$ORIGINAL_HESLA_BACKUP" 2>/dev/null
    )"

    if [ $? -eq 0 ]; then
        pass "project hesla.csv format and key integrity"
    else
        fail "project hesla.csv format and key integrity" "problem: ${result:-neznamy}"
    fi
}

test_project_user_sets_match() {
    local hesla_users="$TMP_DIR/project_hesla_users.txt"
    local info_users="$TMP_DIR/project_info_users.txt"

    awk -F ':' '{ print $1 }' "$ORIGINAL_HESLA_BACKUP" | sort > "$hesla_users"
    awk -F ':' '{ print $1 }' "$ORIGINAL_INFO_BACKUP" | sort > "$info_users"

    if cmp -s "$hesla_users" "$info_users"; then
        pass "project user sets match between hesla.csv and info.txt"
    else
        fail "project user sets match between hesla.csv and info.txt" "mena pouzivatelov sa nezhoduju"
    fi
}

test_project_hashes_match_info() {
    local user_name
    local plain_password
    local actual_hash
    local expected_hash

    while IFS=':' read -r user_name plain_password || [ -n "${user_name:-}" ]; do
        [ -n "$user_name" ] || continue

        actual_hash="$(
            awk -F ':' -v target_user="$user_name" '
                $1 == target_user {
                    print $2
                    exit
                }
            ' "$ORIGINAL_HESLA_BACKUP"
        )"
        expected_hash="$(hash_plaintext "$plain_password")"

        if [ "$actual_hash" = "$expected_hash" ]; then
            pass "project hash matches info for $user_name"
        else
            fail "project hash matches info for $user_name" "ocakavany hash $expected_hash, realny $actual_hash"
        fi
    done < "$ORIGINAL_INFO_BACKUP"
}

test_project_runtime_all_users() {
    local user_name
    local plain_password
    local one_time_key
    local output_file
    local expected_file

    while IFS=':' read -r user_name plain_password || [ -n "${user_name:-}" ]; do
        [ -n "$user_name" ] || continue

        prepare_original_runtime_files
        one_time_key="$(first_key_for_user "$ORIGINAL_HESLA_BACKUP" "$user_name")"
        output_file="$TMP_DIR/project_runtime_${user_name}.out"
        expected_file="$TMP_DIR/project_expected_${user_name}.csv"

        build_expected_after_key_removal \
            "$ORIGINAL_HESLA_BACKUP" \
            "$user_name" \
            "$one_time_key" \
            "$expected_file"

        run_program "$output_file" "$user_name" "$plain_password" "$one_time_key"

        expect_status_token "project runtime output for $user_name" "$output_file" "ok"
        expect_runtime_files \
            "project runtime file update for $user_name" \
            "$expected_file" \
            "$ORIGINAL_INFO_BACKUP"
    done < "$ORIGINAL_INFO_BACKUP"
}

main() {
    backup_originals
    compile_binary
    test_project_files_exist
    test_project_info_integrity
    test_project_hesla_integrity
    test_project_user_sets_match
    test_project_hashes_match_info
    test_project_runtime_all_users
    test_fixture_has_five_users
    test_fixture_key_counts
    test_success_login
    test_success_first_key
    test_success_last_key
    test_success_last_user
    test_success_distinct_user_middle
    test_wrong_password
    test_wrong_user
    test_wrong_key
    test_key_from_other_user_fails
    test_password_of_other_user_fails
    test_username_case_sensitive
    test_password_case_sensitive
    test_reused_key
    test_last_remaining_key
    test_empty_key_list_fails
    test_double_successive_logins_remove_two_keys
    test_missing_hesla_file
    test_missing_final_newline_in_hesla
    test_whitespace_padded_input
    test_long_username_fails_cleanly
    test_long_password_fails_cleanly
    test_stale_temp_file_is_cleaned

    printf '\nVysledok: %d PASS, %d FAIL\n' "$PASS_COUNT" "$FAIL_COUNT"

    if [ "$FAIL_COUNT" -ne 0 ]; then
        exit 1
    fi
}

main "$@"

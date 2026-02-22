#!/bin/bash

# Kompilácia
make compile > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "\033[0;31mChyba kompilácie!\033[0m"
    exit 1
fi

echo -e "\033[0;34m--- KRITICKÉ TESTY ŠIFRY A SÚBOROV ---\033[0m"

# Funkcia pre testy, kde očakávame ÚSPECH (zhodný hash a ticho na konzole)
run_hash_test() {
    local test_name=$1
    local file_in=$2
    local pass=$3

    local hash_pred=$(md5sum "$file_in" | awk '{print $1}')
    
    # Zachytenie výstupu na kontrolu, či program neobsahuje zabudnuté printf
    local out_enc=$(./zadanie1.exe -s -p "$pass" -i "$file_in" -o temp_sifra.bin 2>&1)
    local out_dec=$(./zadanie1.exe -d -p "$pass" -i temp_sifra.bin -o temp_desifra.out 2>&1)
    
    local hash_po=$(md5sum temp_desifra.out | awk '{print $1}')

    if [ "$hash_pred" == "$hash_po" ]; then
        if [ -z "$out_enc" ] && [ -z "$out_dec" ]; then
             echo -e "\033[0;32m[PASS]\033[0m $test_name"
        else
             echo -e "\033[0;33m[WARN]\033[0m $test_name -> Hashe sedia, ALE program niečo vypísal: '$out_enc'"
        fi
    else
        echo -e "\033[0;31m[FAIL]\033[0m $test_name -> Hashe NESEDIA!"
    fi
}

# Funkcia pre testy, kde očakávame CHYBU (chyba\n) a zlyhanie otvorenia súboru
run_file_error_test() {
    local test_name=$1
    local args=$2

    local output=$(./zadanie1.exe $args 2>&1)
    
    if [[ "$output" == *"chyba"* ]]; then
        echo -e "\033[0;32m[PASS]\033[0m $test_name -> Očakávaná chyba zachytená."
    else
        echo -e "\033[0;31m[FAIL]\033[0m $test_name -> Program nezachytil chybu súboru!"
    fi
}

echo "--- 1. EXTRÉMY DÁT A HASHOV ---"

# 1. Bežný text
echo "Ahoj" > t_normal.txt
run_hash_test "Bežný krátky text" "t_normal.txt" "tajne"

# 2. Úplne prázdny súbor (0 bajtov)
touch t_empty.txt
run_hash_test "Úplne prázdny súbor (0 bajtov)" "t_empty.txt" "heslo"

# 3. Súbor s 1 bajtom
echo -n "X" > t_1byte.txt
run_hash_test "Súbor má iba 1 bajt" "t_1byte.txt" "dlheheslo"

# 4. Binárny súbor (Náhoda)
dd if=/dev/urandom of=t_rand.bin bs=1K count=5 2>/dev/null
run_hash_test "Binárny súbor (5KB náhodných dát)" "t_rand.bin" "kluc"

# 5. Súbor plný nulových bajtov (\x00)
dd if=/dev/zero of=t_zero.bin bs=1K count=1 2>/dev/null
run_hash_test "Súbor plný nulových bajtov" "t_zero.bin" "heslo"

echo "--- 2. EXTRÉMY HESLA ---"

# 6. Heslo dlhé iba 1 znak
echo "Sifrujeme s jednym znakom" > t_pass1.txt
run_hash_test "Heslo má iba 1 znak" "t_pass1.txt" "A"

# 7. Heslo je oveľa dlhšie ako súbor
echo "Krátke" > t_dlheheslo.txt
run_hash_test "Heslo je dlhšie ako súbor" "t_dlheheslo.txt" "toto_je_extremne_dlhe_heslo_ktore_nema_konca"

# 8. Heslo so špeciálnymi znakmi a medzerou
echo "Tajne data" > t_spec.txt
run_hash_test "Heslo s medzerami a znakmi" "t_spec.txt" "moje heslo @#$"

echo "--- 3. CHYBY PRI PRÁCI SO SÚBORMI ---"

# 9. Neexistujúci vstupný súbor
rm -f neexistuje.txt
run_file_error_test "Neexistujúci vstupný súbor" "-s -p heslo -i neexistuje.txt -o out.bin"

# 10. Nemožný zápis do výstupu (napr. zadáme existujúci priečinok miesto súboru)
mkdir -p zly_vystup_dir
echo "Test" > t_ok.txt
run_file_error_test "Výstupný súbor sa nedá otvoriť (je to priečinok)" "-s -p heslo -i t_ok.txt -o zly_vystup_dir"


# --- UPRATOVANIE ---
rm -f t_normal.txt t_empty.txt t_1byte.txt t_rand.bin t_zero.bin t_pass1.txt t_dlheheslo.txt t_spec.txt t_ok.txt
rm -f temp_sifra.bin temp_desifra.out
rmdir zly_vystup_dir
echo "------------------------------------------------"
echo "Upratané. Všetky testy dokončené."
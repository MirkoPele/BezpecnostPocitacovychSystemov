#!/bin/bash

# Kompilácia pred testami
make compile
if [ $? -ne 0 ]; then
    echo "Chyba: Program sa nepodarilo skompilovať!"
    exit 1
fi

# Farby pre výstup
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# Funkcia na spustenie testu
# $1: argumenty, $2: ocakavany_vysledok (0 = OK, 1 = chyba)
run_test() {
    local args=$1
    local expected=$2
    
    # Spustenie programu a zachytenie vystupu
    output=$(eval ./zadanie1.exe $args 2>&1)
    exit_code=$?
    
    if [ $expected -eq 1 ]; then
        if [[ "$output" == *"chyba"* ]]; then
            echo -e "${GREEN}[PASS]${NC} Test: $args -> Očakávaná chyba zachytená."
        else
            echo -e "${RED}[FAIL]${NC} Test: $args -> Program mal vypísať 'chyba', ale nevypísal."
        fi
    else
        if [[ "$output" != *"chyba"* ]]; then
            echo -e "${GREEN}[PASS]${NC} Test: $args -> Program zbehol v poriadku."
        else
            echo -e "${RED}[FAIL]${NC} Test: $args -> Program vypísal 'chyba', ale nemal."
        fi
    fi
}

echo "--- SPUŠŤAM AUTOMATICKÉ TESTY (30 SCENÁROV) ---"

# --- SPRÁVNE KOMBINÁCIE (Rôzne poradia) ---
run_test "-s -p heslo -i vstup.txt -o vystup.txt" 0
run_test "-p heslo -s -i vstup.txt -o vystup.txt" 0
run_test "-i vstup.txt -o vystup.txt -p heslo -s" 0
run_test "-d -p heslo -i vstup.txt -o vystup.txt" 0
run_test "-p 123456 -d -i vstup.txt -o vystup.txt" 0

# --- CHÝBAJÚCE POVINNÉ PARAMETRE ---
run_test "-s -p heslo -i vstup.txt" 1      # Chýba -o
run_test "-s -p heslo -o vystup.txt" 1     # Chýba -i
run_test "-s -i vstup.txt -o vystup.txt" 1 # Chýba -p
run_test "-p heslo -i vstup.txt -o out" 1  # Chýba -s/-d
run_test "-s" 1                             # Chýba všetko ostatné

# --- KONFLIKTY REŽIMOV ---
run_test "-s -d -p heslo -i in -o out" 1   # Oba naraz
run_test "-d -s -p heslo -i in -o out" 1   # Oba naraz opačne
run_test "-s -s -p heslo -i in -o out" 1   # Duplikovaný režim

# --- PARAMETRE BEZ HODNOTY (Prepínače na konci) ---
run_test "-s -p heslo -i vstup.txt -o" 1
run_test "-s -p heslo -i" 1
run_test "-s -p" 1

# --- PREPÍNAČE NASLEDUJÚCE PO SEBE (Chýbajúca hodnota medzi nimi) ---
run_test "-s -p -i vstup.txt -o vystup.txt" 1 # -p nemá hodnotu
run_test "-s -p heslo -i -o vystup.txt" 1     # -i nemá hodnotu
run_test "-s -i vstup.txt -o -p heslo" 1      # -o nemá hodnotu

# --- NEZNÁME PARAMETRE ---
run_test "-s -p heslo -i in -o out -x" 1
run_test "-z -s -p heslo -i in -o out" 1
run_test "-s -p heslo -v -i in -o out" 1

# --- DIVNÉ VSTUPY ---
run_test "" 1                                  # Úplne bez argumentov
run_test "-s -p -p -i in -o out" 1            # Dvakrát -p
run_test "-s -i in -i in -p h -o out" 1        # Dvakrát -i
run_test "-s -o out -o out -p h -i in" 1        # Dvakrát -o

# --- EXTRÉMNE PRÍPADY ---
run_test "-s -p heslo -i -s -o out" 1         # -i má ako hodnotu -s
run_test "-s -p -d -i in -o out" 1            # -p má ako hodnotu -d
run_test "-s -p heslo -i in -o -d" 1          # -o má ako hodnotu -d
run_test "vstup.txt -s -p heslo -o out" 1     # Argument bez prepínača na začiatku

# --- ŠPECIÁLNE KOMBINÁCIE A CHYBY (Doplnok k pôvodným 30) ---

# Rôzne poradia s dlhými názvami
run_test "-i velmi_dlhy_nazov_vstupneho_suboru_ktory_ma_vela_znakov.txt -s -p heslo -o out.bin" 0
run_test "-o vystup.txt -p 123 -i vstup.txt -d" 0

# Viacnásobné pokusy o zmätenie (duplikáty prepínačov bez hodnôt)
run_test "-s -d -s -p heslo -i in -o out" 1
run_test "-s -s -s -p h -i i -o o" 1

# Hodnota prepínača je číslo alebo divný znak (toto by malo prejsť, ak to nie je prepínač)
run_test "-s -p 12345 -i vstup.txt -o vystup.txt" 0
run_test "-s -p !!! -i vstup.txt -o vystup.txt" 0

# Prázdne úvodzovky ako heslo (ak to shell dovolí, tvoj program to uvidí ako prázdny string)
# Poznámka: v C to môže byť edge-case, ale tvoj get_value by to mal zvládnuť
run_test "-s -p '' -i in -o out" 1 # Ak to tvoj get_value berie ako pomlčku (prázdny vstup)

# Prepínače rozhádzané medzi názvami
run_test "-p heslo -i vstup.txt -s -o vystup.txt" 0
run_test "-i vstup.txt -s -o vystup.txt -p heslo" 0

# Skúška na neznáme prepínače uprostred
run_test "-s -p heslo -unknown -i in -o out" 1
run_test "-s -p heslo -i in -o out -?" 1

# Kombinácia -s a -d na rôznych pozíciách
run_test "-s -p heslo -i in -o out -d" 1
run_test "-d -p heslo -i in -o out -s" 1

# Chýbajúci povinný parameter v rôznych kombináciách
run_test "-p heslo -i in -o out" 1 # Chýba -s/-d
run_test "-s -i in -o out" 1       # Chýba -p
run_test "-s -p heslo -o out" 1    # Chýba -i
run_test "-s -p heslo -i in" 1     # Chýba -o

# Viacnásobné definície s rovnakou hodnotou
run_test "-s -p heslo -p heslo -i in -o out" 1
run_test "-s -p h -i in -o out -i in" 1

# Skúška na argumenty, ktoré nie sú prepínače (nemajú pomlčku) na konci
run_test "-s -p heslo -i in -o out extra_argument" 1
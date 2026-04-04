#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/*
 * maximalna dlzka jedneho nacitaneho riadku zo vstupu
 */
#define MAX_LINE_LENGTH 2048

/*
 * predvoleny vlastnik novych suborov a adresarov
 */
#define DEFAULT_USER "balaz"

/*
 * uzol moze predstavovat bud obycajny subor, alebo adresar
 */
typedef enum {
    NODE_FILE,
    NODE_DIRECTORY
} NodeType;

/*
 * jeden uzol stromu virtualneho suboroveho systemu
 *
 * name        - nazov suboru alebo adresara
 * owner       - meno vlastnika
 * content     - textovy obsah suboru; pri adresari sa realne nepouziva
 * permissions - prava zapisane ako cislo 0..7
 * type        - ci ide o subor alebo adresar
 * parent      - ukazatel na rodica
 * children    - prvy potomok; vyuziva sa pri adresaroch
 * next        - dalsi surodenec v linked liste deti
 */
typedef struct Node {
    char *name;
    char *owner;
    char *content;
    int permissions;
    NodeType type;
    struct Node *parent;
    struct Node *children;
    struct Node *next;
} Node;

/*
 * root_directory ukazuje na koren systemu "/"
 * current_directory ukazuje na adresar, v ktorom sa prave nachadzame
 */
static Node *root_directory = NULL;
static Node *current_directory = NULL;

/* helpery pre retazce a chybove stavy */
static char *duplicate_text(const char *source_text);
static void fatal_memory_error(void);
static void print_command_error(void);

/* helpery na spracovanie vstupneho riadku */
static char *skip_spaces(char *text);
static void trim_line(char *line);
static char *next_token(char **cursor);
static bool has_more_arguments(char *arguments);
static bool read_one_argument(char *arguments, char **argument_value);
static bool read_optional_argument(char *arguments, char **argument_value);
static bool read_two_arguments(char *arguments, char **first_value, char **second_value);

/* helpery pre strom uzlov */
static Node *create_node(const char *node_name, NodeType node_type, const char *owner_name, int permissions);
static void init_system(void);
static void clean_up(void);
static void free_tree(Node *node);
static bool has_read_permission(Node *node);
static bool has_write_permission(Node *node);
static bool has_execute_permission(Node *node);
static bool is_file(Node *node);
static bool is_directory(Node *node);
static void format_permissions(int permissions, char rwx_text[4]);
static void print_node_summary(Node *node);
static void print_directory_contents(Node *directory_node);
static bool is_valid_name(const char *node_name);
static Node *find_child_by_name(Node *directory_node, const char *child_name);
static void add_child(Node *parent_node, Node *child_node);
static void detach_child(Node *child_node);
static Node *get_root(Node *node);

/* helpery pre pracu s cestami */
static bool is_permission_text(const char *permission_text);
static int permission_value(const char *permission_text);
static void trim_trailing_slashes(char *path_text);
static Node *resolve_path_from_node(Node *start_node, const char *path_text);
static Node *resolve_path(const char *path_text);
static Node *resolve_parent_and_name(const char *path_text, char **entry_name);
static Node *get_creation_parent(const char *path_text, char **entry_name);
static void swap_texts(char **left_text, char **right_text);
static void set_file_content(Node *node, const char *new_content);
static void set_owner_name(Node *node, const char *new_owner);
static void create_node_at_path(char *entry_path, NodeType entry_type);

/* implementacie prikazov */
static void command_ls(char *target_path);
static void command_touch(char *entry_path);
static void command_mkdir(char *entry_path);
static void command_rm(char *target_path);
static void command_vypis(char *target_path);
static void command_spusti(char *target_path);
static void command_cd(char *target_path);
static void command_zapis(char *target_path);
static void command_chmod(char *permission_text, char *target_path);
static void command_chown(char *owner_name, char *target_path);
static bool execute_command_line(char *line);

int main(void){
    /*
     * line je buffer, do ktoreho citame jeden riadok vstupu naraz
     */
    char line[MAX_LINE_LENGTH];

    /*
     * pripravime pociatocny stav systemu:
     * existuje len root adresar "/"
     */
    init_system();

    /*
     * citame prikazy zo standardneho vstupu, kym nepride EOF
     * alebo kym funkcia execute_command_line nevrati false
     * (to sa stane pri prikaze quit)
     */
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (!execute_command_line(line)) {
            break;
        }
    }

    /*
     * na konci uvolnime cely strom uzlov aj globalne ukazatele
     */
    clean_up();
    return 0;
}

static char *duplicate_text(const char *source_text){
    char *copy;
    size_t length;

    /*
     * ak niekto posle NULL, nekopirujeme nic
     */
    if (source_text == NULL) {
        return NULL;
    }

    /*
     * zistime dlzku retazca, alokujeme miesto aj na ukoncovaci '\0'
     * a skopirujeme cely text
     */
    length = strlen(source_text);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        fatal_memory_error();
    }

    memcpy(copy, source_text, length + 1);
    return copy;
}

static void fatal_memory_error(void){
    /*
     * pri chybe alokacie koncime program, lebo dalej by sme s nekorektnymi
     * ukazatelmi uz bezpecne pokracovat nechceli
     */
    fprintf(stderr, "chyba pamate\n");
    exit(EXIT_FAILURE);
}

static void print_command_error(void){
    /*
     * vsetky logicke chyby zadania maju jednotny vystup
     */
    printf("chyba\n");
}

static char *skip_spaces(char *text){
    /*
     * posuvame ukazatel doprava, kym narazame na whitespace
     */
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

static void trim_line(char *line){
    size_t length;

    if (line == NULL) {
        return;
    }

    /*
     * odstranujeme medzery, taby a hlavne '\n' z pravej strany
     */
    length = strlen(line);
    while (length > 0 && isspace((unsigned char)line[length - 1])) {
        line[length - 1] = '\0';
        length--;
    }
}

static char *next_token(char **cursor){
    char *token_start;
    char *token_end;

    /*
     * najprv preskocime medzery pred dalsim argumentom
     */
    *cursor = skip_spaces(*cursor);
    if (**cursor == '\0') {
        return NULL;
    }

    /*
     * token zacina tam, kde prave stojime
     */
    token_start = *cursor;
    token_end = token_start;

    /*
     * ideme doprava, kym nenarazime na medzeru alebo koniec retazca
     */
    while (*token_end != '\0' && !isspace((unsigned char)*token_end)) {
        token_end++;
    }

    /*
     * ak sme neskoncili na '\0', nahradime oddelovac ukoncovacim znakom
     * a cursor posunieme za neho
     */
    if (*token_end != '\0') {
        *token_end = '\0';
        token_end++;
    }

    *cursor = token_end;
    return token_start;
}

static bool has_more_arguments(char *arguments){
    /*
     * skusime precitat este jeden token z kopie ukazatela;
     * originalny retazec uz moze byt upraveny, ale na validaciu to staci
     */
    return next_token(&arguments) != NULL;
}

static bool read_one_argument(char *arguments, char **argument_value){
    /*
     * presne jeden argument znamena:
     * 1. jeden token musi existovat
     * 2. za nim uz nic dalsie nesmie byt
     */
    *argument_value = next_token(&arguments);
    if (*argument_value == NULL || has_more_arguments(arguments)) {
        print_command_error();
        return false;
    }

    return true;
}

static bool read_optional_argument(char *arguments, char **argument_value){
    /*
     * ls moze mat 0 alebo 1 argument
     * ak argument neexistuje, vratime NULL
     * ak existuje viac ako jeden, je to chyba
     */
    *argument_value = next_token(&arguments);
    if (has_more_arguments(arguments)) {
        print_command_error();
        return false;
    }

    return true;
}

static bool read_two_arguments(char *arguments, char **first_value, char **second_value){
    /*
     * pri chmod/chown chceme presne dva argumenty
     */
    *first_value = next_token(&arguments);
    *second_value = next_token(&arguments);
    if (*first_value == NULL || *second_value == NULL || has_more_arguments(arguments)) {
        print_command_error();
        return false;
    }

    return true;
}

static Node *create_node(const char *node_name, NodeType node_type, const char *owner_name, int permissions){
    Node *new_node = (Node *)malloc(sizeof(Node));

    if (new_node == NULL) {
        fatal_memory_error();
    }

    /*
     * kazdy uzol dostane vlastne kopie retazcov
     * aby nezaviseli od docasnych bufferov
     */
    new_node->name = duplicate_text(node_name);
    new_node->owner = duplicate_text(owner_name);
    new_node->content = duplicate_text("");
    new_node->permissions = permissions;
    new_node->type = node_type;

    /*
     * po vytvoreni este uzol nie je nikam zaradeny
     */
    new_node->parent = NULL;
    new_node->children = NULL;
    new_node->next = NULL;
    return new_node;
}

static void init_system(void){
    /*
     * system zacina jedinym adresarom "/"
     * aktualny adresar je na zaciatku tiez "/"
     */
    root_directory = create_node("/", NODE_DIRECTORY, DEFAULT_USER, 7);
    current_directory = root_directory;
}

static void clean_up(void){
    /*
     * rekurzivne uvolnime cely strom a potom vynulujeme globalne premenne
     */
    free_tree(root_directory);
    root_directory = NULL;
    current_directory = NULL;
}

static void free_tree(Node *node){
    Node *child_node;
    Node *next_child;

    if (node == NULL) {
        return;
    }

    /*
     * najprv uvolnime vsetky deti a az potom samotny uzol
     */
    child_node = node->children;
    while (child_node != NULL) {
        next_child = child_node->next;
        free_tree(child_node);
        child_node = next_child;
    }

    free(node->name);
    free(node->owner);
    free(node->content);
    free(node);
}

static bool has_read_permission(Node *node){
    /*
     * bit 4 znamena pravo citania
     */
    return node != NULL && (node->permissions & 4) != 0;
}

static bool has_write_permission(Node *node){
    /*
     * bit 2 znamena pravo zapisu
     */
    return node != NULL && (node->permissions & 2) != 0;
}

static bool has_execute_permission(Node *node){
    /*
     * bit 1 znamena pravo spustenia / vstupu do adresara
     */
    return node != NULL && (node->permissions & 1) != 0;
}

static bool is_file(Node *node){
    return node != NULL && node->type == NODE_FILE;
}

static bool is_directory(Node *node){
    return node != NULL && node->type == NODE_DIRECTORY;
}

static void format_permissions(int permissions, char rwx_text[4]){
    /*
     * cislo 0..7 prevedieme na text ako napr. rwx, r-x, ---
     */
    rwx_text[0] = (permissions & 4) ? 'r' : '-';
    rwx_text[1] = (permissions & 2) ? 'w' : '-';
    rwx_text[2] = (permissions & 1) ? 'x' : '-';
    rwx_text[3] = '\0';
}

static void print_node_summary(Node *node){
    char rwx_text[4];

    format_permissions(node->permissions, rwx_text);
    printf("%s %s %s\n", node->name, node->owner, rwx_text);
}

static void print_directory_contents(Node *directory_node){
    Node *child_node;

    /*
     * ak adresar nema deti, podla zadania vypiseme pevny text
     */
    if (directory_node->children == NULL) {
        printf("ziaden subor\n");
        return;
    }

    /*
     * deti su ulozene v linked liste, preto ich ideme postupne cez next
     */
    for (child_node = directory_node->children; child_node != NULL; child_node = child_node->next) {
        print_node_summary(child_node);
    }
}

static bool is_valid_name(const char *node_name){
    /*
     * nepovolime prazdny nazov, "." ani ".."
     * a nazov nesmie obsahovat dalsiu lomku
     */
    if (node_name == NULL || node_name[0] == '\0') {
        return false;
    }

    if (strcmp(node_name, ".") == 0 || strcmp(node_name, "..") == 0) {
        return false;
    }

    return strchr(node_name, '/') == NULL;
}

static Node *find_child_by_name(Node *directory_node, const char *child_name){
    Node *child_node;

    if (!is_directory(directory_node)) {
        return NULL;
    }

    /*
     * medzi priamymi detmi hladame prveho potomka s rovnakym menom
     */
    for (child_node = directory_node->children; child_node != NULL; child_node = child_node->next) {
        if (strcmp(child_node->name, child_name) == 0) {
            return child_node;
        }
    }

    return NULL;
}

static void add_child(Node *parent_node, Node *child_node){
    /*
     * nove dieta vkladame na zaciatok zoznamu deti
     */
    child_node->parent = parent_node;
    child_node->next = parent_node->children;
    parent_node->children = child_node;
}

static void detach_child(Node *child_node){
    Node *current_node;
    Node *previous_node;
    Node *parent_node;

    if (child_node == NULL || child_node->parent == NULL) {
        return;
    }

    parent_node = child_node->parent;
    previous_node = NULL;
    current_node = parent_node->children;

    /*
     * hladame presne ten uzol, ktory chceme odpojit
     */
    while (current_node != NULL && current_node != child_node) {
        previous_node = current_node;
        current_node = current_node->next;
    }

    if (current_node == NULL) {
        return;
    }

    /*
     * bud sa odpaja prvy prvok zoznamu, alebo niektory dalsi
     */
    if (previous_node == NULL) {
        parent_node->children = child_node->next;
    } else {
        previous_node->next = child_node->next;
    }

    child_node->parent = NULL;
    child_node->next = NULL;
}

static Node *get_root(Node *node){
    /*
     * od aktualneho uzla ideme hore cez parent, kym sa da
     */
    while (node != NULL && node->parent != NULL) {
        node = node->parent;
    }

    return node;
}

static bool is_permission_text(const char *permission_text){
    /*
     * prava musia byt presne jeden znak v rozsahu '0' az '7'
     */
    if (permission_text == NULL || permission_text[0] == '\0' || permission_text[1] != '\0') {
        return false;
    }

    return permission_text[0] >= '0' && permission_text[0] <= '7';
}

static int permission_value(const char *permission_text){
    return permission_text[0] - '0';
}

static void trim_trailing_slashes(char *path_text){
    size_t length;

    if (path_text == NULL) {
        return;
    }

    /*
     * z "a///" spravime "a", ale "/" nechame bez zmeny
     */
    length = strlen(path_text);
    while (length > 1 && path_text[length - 1] == '/') {
        path_text[length - 1] = '\0';
        length--;
    }
}

static Node *resolve_path_from_node(Node *start_node, const char *path_text){
    char *path_copy;
    char *path_part;
    Node *current_node;

    /*
     * prazdna cesta znamena "zostan tam, kde si"
     */
    if (path_text == NULL || path_text[0] == '\0') {
        return start_node;
    }

    /*
     * specialny pripad pre root
     */
    if (strcmp(path_text, "/") == 0) {
        return get_root(start_node);
    }

    /*
     * absolutna cesta zacina od rootu, relativna od aktualneho miesta
     */
    current_node = (path_text[0] == '/') ? get_root(start_node) : start_node;
    path_copy = duplicate_text(path_text);

    /*
     * strtok postupne oddeli casti cesty medzi lomkami
     * podla upresneni netreba specialne spracovavat "." ani ".." tu
     */
    for (path_part = strtok(path_copy, "/"); path_part != NULL; path_part = strtok(NULL, "/")) {
        /*
         * ak sa uz snazime ist cez subor, cesta je neplatna
         */
        if (!is_directory(current_node)) {
            free(path_copy);
            return NULL;
        }

        /*
         * prejdeme na potomka s danym menom
         */
        current_node = find_child_by_name(current_node, path_part);
        if (current_node == NULL) {
            free(path_copy);
            return NULL;
        }
    }

    free(path_copy);
    return current_node;
}

static Node *resolve_path(const char *path_text){
    /*
     * bezny preklad cesty sa robi vzdy od current_directory
     */
    return resolve_path_from_node(current_directory, path_text);
}

static Node *resolve_parent_and_name(const char *path_text, char **entry_name){
    char *path_copy;
    char *separator;
    Node *parent_node;

    *entry_name = NULL;
    if (path_text == NULL || path_text[0] == '\0') {
        return NULL;
    }

    path_copy = duplicate_text(path_text);
    trim_trailing_slashes(path_copy);

    /*
     * najdeme poslednu lomku, aby sme oddelili rodica a nazov noveho uzla
     */
    separator = strrchr(path_copy, '/');
    if (separator == NULL) {
        /*
         * ak tam nie je ziadna lomka, rodic je aktualny adresar
         * a cely retazec je meno
         */
        parent_node = current_directory;
        *entry_name = duplicate_text(path_copy);
        free(path_copy);
        return parent_node;
    }

    if (separator == path_copy) {
        /*
         * ak je posledna lomka hned na zaciatku, rodicom je root
         */
        parent_node = get_root(current_directory);
        *entry_name = duplicate_text(separator + 1);
        free(path_copy);
        return parent_node;
    }

    /*
     * inak rozdelime retazec na cast "rodic" a cast "meno"
     */
    *entry_name = duplicate_text(separator + 1);
    *separator = '\0';
    parent_node = resolve_path(path_copy);
    free(path_copy);
    return parent_node;
}

static Node *get_creation_parent(const char *path_text, char **entry_name){
    Node *parent_node = resolve_parent_and_name(path_text, entry_name);

    /*
     * nove meno musime vediet vytvorit v platnom adresari
     * a samotne meno musi byt povolene
     */
    if (!is_directory(parent_node) || !is_valid_name(*entry_name)) {
        return NULL;
    }

    return parent_node;
}

static void swap_texts(char **left_text, char **right_text){
    char *temporary_text = *left_text;
    *left_text = *right_text;
    *right_text = temporary_text;
}

static void set_file_content(Node *node, const char *new_content){
    /*
     * povodny obsah uvolnime a ulozime novu kopiu
     */
    free(node->content);
    node->content = duplicate_text(new_content == NULL ? "" : new_content);
}

static void set_owner_name(Node *node, const char *new_owner){
    free(node->owner);
    node->owner = duplicate_text(new_owner);
}

static void create_node_at_path(char *entry_path, NodeType entry_type){
    char *entry_name = NULL;
    Node *parent;
    Node *already_there;
    Node *new_node;

    /*
     * z cesty najprv zistime, kde sa ma novy uzol vytvorit
     * a ako sa ma volat
     */
    parent = get_creation_parent(entry_path, &entry_name);
    if (parent == NULL || !has_write_permission(parent)) {
        free(entry_name);
        print_command_error();
        return;
    }

    /*
     * v jednom adresari nesmu existovat dvaja potomkovia s rovnakym menom
     */
    already_there = find_child_by_name(parent, entry_name);
    if (already_there != NULL) {
        free(entry_name);
        print_command_error();
        return;
    }

    new_node = create_node(entry_name, entry_type, DEFAULT_USER, 7);
    add_child(parent, new_node);
    free(entry_name);
}

static void command_ls(char *target_path){
    Node *entry;

    /*
     * ls bez argumentu vypise obsah aktualneho adresara
     */
    if (target_path == NULL) {
        if (!has_read_permission(current_directory)) {
            print_command_error();
            return;
        }

        print_directory_contents(current_directory);
        return;
    }

    /*
     * ls s argumentom vypise info o jednom konkretnom uzle
     */
    entry = resolve_path(target_path);
    if (entry == NULL) {
        print_command_error();
        return;
    }

    print_node_summary(entry);
}

static void command_touch(char *entry_path){
    create_node_at_path(entry_path, NODE_FILE);
}

static void command_mkdir(char *entry_path){
    create_node_at_path(entry_path, NODE_DIRECTORY);
}

static void command_rm(char *target_path){
    Node *victim = resolve_path(target_path);

    /*
     * root sa mazat nesmie, preto kontrolujeme victim->parent
     * zaroven musi byt dovoleny zapis do rodicovskeho adresara
     */
    if (victim == NULL || victim->parent == NULL || !has_write_permission(victim->parent)) {
        print_command_error();
        return;
    }

    detach_child(victim);
    free_tree(victim);
}

static void command_vypis(char *target_path){
    Node *file_node = resolve_path(target_path);

    if (!is_file(file_node) || !has_read_permission(file_node)) {
        print_command_error();
        return;
    }

    /*
     * pri prazdnom subore je podla zadania vystup "ok"
     */
    if (file_node->content == NULL || file_node->content[0] == '\0') {
        printf("ok\n");
        return;
    }

    printf("%s\n", file_node->content);
}

static void command_spusti(char *target_path){
    Node *file_node = resolve_path(target_path);

    /*
     * prikaz nic nevypisuje; len overi, ci je subor spustitelny
     */
    if (!is_file(file_node) || !has_execute_permission(file_node)) {
        print_command_error();
    }
}

static void command_cd(char *target_path){
    Node *destination = resolve_path(target_path);

    /*
     * prikaz cd .. staci podporit specialne, bez vseobecnej podpory ".."
     * vo vsetkych ostatnych prikazoch
     */
    if (strcmp(target_path, "..") == 0) {
        if (current_directory->parent != NULL) {
            current_directory = current_directory->parent;
        }
        return;
    }

    /*
     * do noveho adresara sa da prejst len vtedy, ak ide o adresar
     * a mame execute pravo
     */
    if (!is_directory(destination) || !has_execute_permission(destination)) {
        print_command_error();
        return;
    }

    current_directory = destination;
}

static void command_zapis(char *target_path){
    Node *target_node = resolve_path(target_path);

    /*
     * podla doplnujucich odpovedi sa data realne neriesia,
     * prikaz len overi, ze do uzla sa smie zapisovat
     */
    if (target_node == NULL || !has_write_permission(target_node)) {
        print_command_error();
        return;
    }

    /*
     * obsah nechavame prazdny, aby vypis uspesneho suboru stale vratil "ok"
     */
    set_file_content(target_node, "");
}

static void command_chmod(char *permission_text, char *target_path){
    Node *entry;

    /*
     * ak prve nevyzera ako prava a druhe ano, argumenty prehodime
     * aby fungovali oba tvary vstupu
     */
    if (!is_permission_text(permission_text)) {
        if (!is_permission_text(target_path)) {
            print_command_error();
            return;
        }

        swap_texts(&permission_text, &target_path);
    }

    entry = resolve_path(target_path);
    if (entry == NULL) {
        print_command_error();
        return;
    }

    entry->permissions = permission_value(permission_text);
}

static void command_chown(char *owner_name, char *target_path){
    bool first_text_looks_like_path = resolve_path(owner_name) != NULL;
    Node *entry = resolve_path(target_path);

    /*
     * ak prvy argument vyzera ako cesta a druhy nie, berieme to ako
     * prehodene zadanie a argumenty vymenime
     */
    if (first_text_looks_like_path && entry == NULL) {
        swap_texts(&owner_name, &target_path);
        entry = resolve_path(target_path);
    }

    if (owner_name == NULL || owner_name[0] == '\0' || entry == NULL) {
        print_command_error();
        return;
    }

    set_owner_name(entry, owner_name);
}

static bool execute_command_line(char *line){
    char *cursor;
    char *command_name;
    char *first_arg = NULL;
    char *second_arg = NULL;

    /*
     * odstranime '\n', preskocime uvodne medzery a prazdny riadok ignorujeme
     */
    trim_line(line);
    cursor = skip_spaces(line);
    if (*cursor == '\0') {
        return true;
    }

    /*
     * prve slovo v riadku je nazov prikazu
     */
    command_name = next_token(&cursor);

    if (strcmp(command_name, "quit") == 0) {
        /*
         * quit nesmie mat ziadne dalsie argumenty
         */
        if (has_more_arguments(cursor)) {
            print_command_error();
            return true;
        }

        return false;
    }

    if (strcmp(command_name, "zapis") == 0) {
        /*
         * zapis ma mat len jeden argument - cielovy subor alebo adresar
         */
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_zapis(first_arg);
        return true;
    }

    if (strcmp(command_name, "ls") == 0) {
        if (!read_optional_argument(cursor, &first_arg)) {
            return true;
        }

        command_ls(first_arg);
        return true;
    }

    if (strcmp(command_name, "cd") == 0) {
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_cd(first_arg);
        return true;
    }

    if (strcmp(command_name, "touch") == 0) {
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_touch(first_arg);
        return true;
    }

    if (strcmp(command_name, "mkdir") == 0) {
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_mkdir(first_arg);
        return true;
    }

    if (strcmp(command_name, "vypis") == 0) {
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_vypis(first_arg);
        return true;
    }

    if (strcmp(command_name, "spusti") == 0) {
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_spusti(first_arg);
        return true;
    }

    if (strcmp(command_name, "rm") == 0) {
        if (!read_one_argument(cursor, &first_arg)) {
            return true;
        }

        command_rm(first_arg);
        return true;
    }

    if (strcmp(command_name, "chmod") == 0) {
        if (!read_two_arguments(cursor, &first_arg, &second_arg)) {
            return true;
        }

        command_chmod(first_arg, second_arg);
        return true;
    }

    if (strcmp(command_name, "chown") == 0) {
        if (!read_two_arguments(cursor, &first_arg, &second_arg)) {
            return true;
        }

        command_chown(first_arg, second_arg);
        return true;
    }

    /*
     * ak sa nazov prikazu nezhoduje so ziadnym znamym prikazom
     */
    print_command_error();
    return true;
}

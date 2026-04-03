#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 2048
#define DEFAULT_USER "balaz"

typedef enum {
    NODE_FILE,
    NODE_DIRECTORY
} NodeType;

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

Node *root_directory = NULL;
Node *current_directory = NULL;

char *copy_text(const char *text);
void memory_error();
void print_error();

Node *create_node(const char *name, NodeType type, const char *owner, int permissions);
void init_system();
void clean_up();
void free_tree(Node *node);

bool has_read_permission(Node *node);
bool has_write_permission(Node *node);
bool has_execute_permission(Node *node);
bool is_file(Node *node);
bool is_directory(Node *node);

void permissions_to_text(int permissions, char text[4]);
void print_node_info(Node *node);
void print_directory_listing(Node *directory);

bool is_valid_name(const char *name);
Node *find_child(Node *directory, const char *name);
void add_child(Node *parent, Node *child);
void detach_child(Node *child);
Node *get_root(Node *node);

char *skip_spaces(char *text);
void trim_line(char *text);
char *read_token(char **cursor);
bool has_extra_arguments(char *cursor);
bool parse_no_arguments(char *cursor);
bool parse_one_argument(char *cursor, char **arg);
bool parse_optional_argument(char *cursor, char **arg);
bool parse_two_arguments(char *cursor, char **first, char **second);
bool parse_zapis_arguments(char *cursor, char **path, char **content);

bool is_permission_text(const char *text);
int permission_value(const char *text);
void remove_last_slashes(char *path);

Node *resolve_path_from(Node *start, const char *path);
Node *resolve_path(const char *path);
Node *resolve_parent_directory(const char *path, char **name_part);
Node *get_create_parent(const char *path, char **name_part);

void swap_args(char **first, char **second);

void replace_content(Node *node, const char *new_content);
void change_owner(Node *node, const char *owner);

void command_ls(char *arg);
void command_touch(char *path);
void command_mkdir(char *path);
void command_rm(char *path);
void command_vypis(char *path);
void command_spusti(char *path);
void command_cd(char *path);
void command_zapis(char *path, char *content);
void command_chmod(char *first, char *second);
void command_chown(char *first, char *second);
void create_entry(char *path, NodeType type);

bool execute_simple_command(char *command, char *cursor);
bool process_command(char *line);

// cita prikazy zo vstupu a postupne ich vykonava
int main(void){
    char line[MAX_LINE_LENGTH];

    init_system();

    while (fgets(line, sizeof(line), stdin) != NULL){
        if (!process_command(line)) {
            break;
        }
    }

    clean_up();
    return 0;
}


// pripravi pociatocny stav virtualneho suboroveho systemu
void init_system(){
    root_directory = create_node("/", NODE_DIRECTORY, DEFAULT_USER, 7);
    current_directory = root_directory;
}

// uvolni cely strom a vynuluje globalne premenne
void clean_up(){
    free_tree(root_directory);
    root_directory = NULL;
    current_directory = NULL;
}

// ukonci program pri chybe s pamatou
void memory_error(){ fprintf(stderr, "chyba pamate\n"); exit(EXIT_FAILURE); }

// vypise jednotny chybovy vystup
void print_error(){ printf("chyba\n"); }

// vytvori kopiu textu v novom bloku pamate
char *copy_text(const char *text){
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) memory_error();
    memcpy(copy, text, length + 1);
    return copy;
}

// vytvori novy uzol reprezentujuci subor alebo adresar
Node *create_node(const char *name, NodeType type, const char *owner, int permissions){
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL) memory_error();

    node->name = copy_text(name);
    node->owner = copy_text(owner);
    node->content = copy_text("");
    node->permissions = permissions;
    node->type = type;
    node->parent = NULL;
    node->children = NULL;
    node->next = NULL;

    return node;
}

// rekurzivne uvolni cely podstrom od zadaneho uzla
void free_tree(Node *node){
    Node *child;
    Node *next_child;

    if (node == NULL) {
        return;
    }

    child = node->children;
    while (child != NULL) {
        next_child = child->next;
        free_tree(child);
        child = next_child;
    }

    free(node->name);
    free(node->owner);
    free(node->content);
    free(node);
}

// zisti, ci sa da z uzla citat
bool has_read_permission(Node *node){ return node != NULL && (node->permissions & 4) != 0; }

// zisti, ci sa do uzla da zapisovat
bool has_write_permission(Node *node){ return node != NULL && (node->permissions & 2) != 0; }

// zisti, ci sa uzol da spustit alebo otvorit
bool has_execute_permission(Node *node){ return node != NULL && (node->permissions & 1) != 0; }

// overi, ci uzol predstavuje obycajny subor
bool is_file(Node *node){ return node != NULL && node->type == NODE_FILE; }

// overi, ci uzol predstavuje adresar
bool is_directory(Node *node){ return node != NULL && node->type == NODE_DIRECTORY; }

// prevedie ciselne prava do textovej podoby rwx
void permissions_to_text(int permissions, char text[4]){
    text[0] = (permissions & 4) ? 'r' : '-';
    text[1] = (permissions & 2) ? 'w' : '-';
    text[2] = (permissions & 1) ? 'x' : '-';
    text[3] = '\0';
}

// vypise meno, vlastnika a prava jedneho uzla
void print_node_info(Node *node){
    char permissions_text[4];

    permissions_to_text(node->permissions, permissions_text);
    printf("%s %s %s\n", node->name, node->owner, permissions_text);
}

// vypise obsah adresara alebo oznam o prazdnom adresari
void print_directory_listing(Node *directory){
    Node *current = directory->children;

    if (current == NULL) {
        printf("ziaden subor\n");
        return;
    }

    while (current != NULL) {
        print_node_info(current);
        current = current->next;
    }
}

// skontroluje, ci nazov uzla neobsahuje nepovolene hodnoty
bool is_valid_name(const char *name){
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }

    return strchr(name, '/') == NULL;
}

// najde priameho potomka s danym menom
Node *find_child(Node *directory, const char *name){
    Node *current;

    if (!is_directory(directory)) {
        return NULL;
    }

    current = directory->children;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

// vlozi novy uzol medzi deti daneho rodica
void add_child(Node *parent, Node *child){
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

// odpoji uzol od jeho rodica bez uvolnenia pamate
void detach_child(Node *child){
    Node *current;
    Node *parent;

    if (child == NULL || child->parent == NULL) {
        return;
    }

    parent = child->parent;
    current = parent->children;

    if (current == child) {
        parent->children = child->next;
        child->parent = NULL;
        child->next = NULL;
        return;
    }

    while (current != NULL && current->next != child) {
        current = current->next;
    }

    if (current != NULL) {
        current->next = child->next;
    }

    child->parent = NULL;
    child->next = NULL;
}

// vrati korenovy adresar pre dany uzol
Node *get_root(Node *node){
    while (node != NULL && node->parent != NULL) {
        node = node->parent;
    }

    return node;
}

// preskoci uvodne medzery v texte
char *skip_spaces(char *text){
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

// odstrani medzery a konce riadkov z pravej strany
void trim_line(char *text){
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[length - 1] = '\0';
        length--;
    }
}

// precita dalsi argument oddeleny medzerou
char *read_token(char **cursor){
    char *start;
    char *end;

    *cursor = skip_spaces(*cursor);
    if (**cursor == '\0') {
        return NULL;
    }

    start = *cursor;
    end = start;

    while (*end != '\0' && !isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != '\0') {
        *end = '\0';
        end++;
    }

    *cursor = end;
    return start;
}

// zisti, ci po spracovani este ostali dalsie argumenty
bool has_extra_arguments(char *cursor){ return read_token(&cursor) != NULL; }

// overi prikaz bez argumentov
bool parse_no_arguments(char *cursor){
    if (has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

// overi prikaz s presne jednym argumentom
bool parse_one_argument(char *cursor, char **arg){
    *arg = read_token(&cursor);

    if (*arg == NULL || has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

// overi prikaz s volitelnym argumentom
bool parse_optional_argument(char *cursor, char **arg){
    *arg = read_token(&cursor);

    if (has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

// overi prikaz s dvoma argumentmi
bool parse_two_arguments(char *cursor, char **first, char **second){
    *first = read_token(&cursor);
    *second = read_token(&cursor);

    if (*first == NULL || *second == NULL || has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

// oddeli cestu a zvysok riadku pre prikaz zapis
bool parse_zapis_arguments(char *cursor, char **path, char **content){
    *path = read_token(&cursor);
    if (*path == NULL) {
        print_error();
        return false;
    }

    *content = skip_spaces(cursor);
    return true;
}

// skontroluje, ci retazec predstavuje jedno cislo prav
bool is_permission_text(const char *text){
    if (text == NULL || text[0] == '\0' || text[1] != '\0') {
        return false;
    }

    return text[0] >= '0' && text[0] <= '7';
}

// premeni textovu podobu prav na cislo
int permission_value(const char *text){ return text[0] - '0'; }

// odstrani nadbytocne lomky na konci cesty
void remove_last_slashes(char *path){
    size_t length;

    if (path == NULL) {
        return;
    }

    length = strlen(path);
    while (length > 1 && path[length - 1] == '/') {
        path[length - 1] = '\0';
        length--;
    }
}

// prejde cestu od zadaneho uzla a vrati cielovy uzol
Node *resolve_path_from(Node *start, const char *path){
    Node *current;
    char *path_copy;
    char *token;

    if (path == NULL || path[0] == '\0') {
        return start;
    }

    if (strcmp(path, "/") == 0) {
        return get_root(start);
    }

    // absolutna cesta sa riesi od rootu, inak od aktualneho miesta
    current = (path[0] == '/') ? get_root(start) : start;
    path_copy = copy_text(path);

    token = strtok(path_copy, "/");
    while (token != NULL) {
        // bodka znamena ostat v tom istom adresari
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        }

        // dve bodky posunu o uroven vyssie ak sa da
        if (strcmp(token, "..") == 0) {
            if (current->parent != NULL) {
                current = current->parent;
            }
            token = strtok(NULL, "/");
            continue;
        }

        if (!is_directory(current)) {
            free(path_copy);
            return NULL;
        }

        current = find_child(current, token);
        if (current == NULL) {
            free(path_copy);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
    return current;
}

// prelozi cestu od aktualneho adresara
Node *resolve_path(const char *path){ return resolve_path_from(current_directory, path); }

// vrati rodicovsky adresar a oddeli nazov poslednej casti
Node *resolve_parent_directory(const char *path, char **name_part){
    Node *parent;
    char *path_copy;
    char *last_slash;

    *name_part = NULL;
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    path_copy = copy_text(path);
    remove_last_slashes(path_copy);

    // posledna cast bude nazov a vsetko pred nou rodic
    last_slash = strrchr(path_copy, '/');
    if (last_slash == NULL) {
        parent = current_directory;
        *name_part = copy_text(path_copy);
        free(path_copy);
        return parent;
    }

    if (last_slash == path_copy) {
        parent = get_root(current_directory);
        *name_part = copy_text(last_slash + 1);
        free(path_copy);
        return parent;
    }

    *last_slash = '\0';
    *name_part = copy_text(last_slash + 1);
    parent = resolve_path(path_copy);
    free(path_copy);

    return parent;
}

// pripravi rodica pre vytvorenie noveho uzla
Node *get_create_parent(const char *path, char **name_part){
    Node *parent = resolve_parent_directory(path, name_part);
    if (!is_directory(parent) || !is_valid_name(*name_part)) {
        return NULL;
    }

    return parent;
}

// prehodi poradie dvoch argumentov
void swap_args(char **first, char **second){
    char *temp = *first;
    *first = *second;
    *second = temp;
}

// nahradi obsah suboru novym textom
void replace_content(Node *node, const char *new_content){
    free(node->content);
    node->content = copy_text(new_content == NULL ? "" : new_content);
}

// zmeni vlastnika vybraneho uzla
void change_owner(Node *node, const char *owner){
    free(node->owner);
    node->owner = copy_text(owner);
}

// vytvori subor alebo adresar podla zadaneho typu
void create_entry(char *path, NodeType type){
    Node *parent;
    Node *existing;
    Node *new_node;
    char *name_part = NULL;

    parent = get_create_parent(path, &name_part);
    if (parent == NULL || !has_write_permission(parent)) {
        free(name_part);
        print_error();
        return;
    }

    existing = find_child(parent, name_part);
    if (existing != NULL) {
        free(name_part);
        print_error();
        return;
    }

    new_node = create_node(name_part, type, DEFAULT_USER, 7);
    add_child(parent, new_node);
    free(name_part);
}

// spracuje prikaz na vypis informacii o uzle
void command_ls(char *arg){
    Node *target;

    if (arg == NULL) {
        if (!is_directory(current_directory) || !has_read_permission(current_directory)) {
            print_error();
            return;
        }

        print_directory_listing(current_directory);
        return;
    }

    target = resolve_path(arg);
    if (target == NULL) {
        print_error();
        return;
    }

    print_node_info(target);
}

// vytvori novy subor v zadanej ceste
void command_touch(char *path){
    create_entry(path, NODE_FILE);
}

// vytvori novy adresar v zadanej ceste
void command_mkdir(char *path){
    create_entry(path, NODE_DIRECTORY);
}

// odstrani uzol aj s jeho podstromom
void command_rm(char *path){
    Node *target = resolve_path(path);

    if (target == NULL || target->parent == NULL || !has_write_permission(target->parent)) {
        print_error();
        return;
    }

    detach_child(target);
    free_tree(target);
}

// vypise obsah suboru alebo ok pri prazdnom obsahu
void command_vypis(char *path){
    Node *target = resolve_path(path);

    if (!is_file(target) || !has_read_permission(target)) {
        print_error();
        return;
    }

    if (target->content == NULL || target->content[0] == '\0') {
        printf("ok\n");
        return;
    }

    printf("%s\n", target->content);
}

// overi, ci sa dany subor da spustit
void command_spusti(char *path){
    Node *target = resolve_path(path);

    if (!is_file(target) || !has_execute_permission(target)) {
        print_error();
    }
}

// zmeni aktualny adresar
void command_cd(char *path){
    Node *target = resolve_path(path);

    if (!is_directory(target) || !has_execute_permission(target)) {
        print_error();
        return;
    }

    current_directory = target;
}

// zapise novy obsah do suboru
void command_zapis(char *path, char *content){
    Node *target = resolve_path(path);

    if (target == NULL || !has_write_permission(target)) {
        print_error();
        return;
    }

    replace_content(target, content);
}

// zmeni pristupove prava uzla
void command_chmod(char *first, char *second){
    Node *target;

    if (!is_permission_text(first)) {
        if (is_permission_text(second)) {
            swap_args(&first, &second);
        } else {
            print_error();
            return;
        }
    }

    target = resolve_path(second);
    if (target == NULL) {
        print_error();
        return;
    }

    target->permissions = permission_value(first);
}

// zmeni vlastnika uzla
void command_chown(char *first, char *second){
    Node *target;
    Node *first_node = resolve_path(first);
    Node *second_node = resolve_path(second);

    // ak prvy argument vyzera ako cesta a druhy nie, prehodime ich
    if (first_node != NULL && second_node == NULL) {
        swap_args(&first, &second);
    }

    target = resolve_path(second);

    if (first == NULL || first[0] == '\0' || target == NULL) {
        print_error();
        return;
    }

    change_owner(target, first);
}

// vykona bezne prikazy okrem zapis a quit
bool execute_simple_command(char *command, char *cursor){
    char *first = NULL;
    char *second = NULL;

    if (strcmp(command, "ls") == 0) {
        if (!parse_optional_argument(cursor, &first)) {
            return true;
        }
        command_ls(first);
    } else if (strcmp(command, "touch") == 0) {
        if (!parse_one_argument(cursor, &first)) {
            return true;
        }
        command_touch(first);
    } else if (strcmp(command, "mkdir") == 0) {
        if (!parse_one_argument(cursor, &first)) {
            return true;
        }
        command_mkdir(first);
    } else if (strcmp(command, "rm") == 0) {
        if (!parse_one_argument(cursor, &first)) {
            return true;
        }
        command_rm(first);
    } else if (strcmp(command, "vypis") == 0) {
        if (!parse_one_argument(cursor, &first)) {
            return true;
        }
        command_vypis(first);
    } else if (strcmp(command, "spusti") == 0) {
        if (!parse_one_argument(cursor, &first)) {
            return true;
        }
        command_spusti(first);
    } else if (strcmp(command, "cd") == 0) {
        if (!parse_one_argument(cursor, &first)) {
            return true;
        }
        command_cd(first);
    } else if (strcmp(command, "chmod") == 0) {
        if (!parse_two_arguments(cursor, &first, &second)) {
            return true;
        }
        command_chmod(first, second);
    } else if (strcmp(command, "chown") == 0) {
        if (!parse_two_arguments(cursor, &first, &second)) {
            return true;
        }
        command_chown(first, second);
    } else {
        print_error();
    }

    return true;
}

// nacita jeden riadok a rozhodne, ako sa ma spracovat
bool process_command(char *line){
    char *cursor;
    char *command;
    char *path = NULL;
    char *content = NULL;

    trim_line(line);
    cursor = skip_spaces(line);

    if (*cursor == '\0') {
        return true;
    }

    command = read_token(&cursor);

    // quit ukonci citanie prikazov
    if (strcmp(command, "quit") == 0) {
        if (!parse_no_arguments(cursor)) {
            return true;
        }
        return false;
    }

    // zapis berie ako obsah cely zvysok riadku
    if (strcmp(command, "zapis") == 0) {
        if (!parse_zapis_arguments(cursor, &path, &content)) {
            return true;
        }
        command_zapis(path, content);
        return true;
    }

    return execute_simple_command(command, cursor);
}

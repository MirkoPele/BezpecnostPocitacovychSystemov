#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 2048
#define DEFAULT_OWNER "balaz"

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

bool execute_simple_command(char *command, char *cursor);
bool process_command(char *line);

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


void init_system(){
    root_directory = create_node("/", NODE_DIRECTORY, DEFAULT_OWNER, 7);
    current_directory = root_directory;
}

void clean_up(){
    free_tree(root_directory);
    root_directory = NULL;
    current_directory = NULL;
}

void memory_error(){ fprintf(stderr, "chyba pamate\n"); exit(EXIT_FAILURE); }
void print_error(){ printf("chyba\n"); }

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

bool has_read_permission(Node *node){ return node != NULL && (node->permissions & 4) != 0; }
bool has_write_permission(Node *node){ return node != NULL && (node->permissions & 2) != 0; }
bool has_execute_permission(Node *node){ return node != NULL && (node->permissions & 1) != 0; }
bool is_file(Node *node){ return node != NULL && node->type == NODE_FILE; }
bool is_directory(Node *node){ return node != NULL && node->type == NODE_DIRECTORY; }

void permissions_to_text(int permissions, char text[4]){
    text[0] = (permissions & 4) ? 'r' : '-';
    text[1] = (permissions & 2) ? 'w' : '-';
    text[2] = (permissions & 1) ? 'x' : '-';
    text[3] = '\0';
}

void print_node_info(Node *node){
    char permissions_text[4];

    permissions_to_text(node->permissions, permissions_text);
    printf("%s %s %s\n", node->name, node->owner, permissions_text);
}

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

bool is_valid_name(const char *name){
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }

    return strchr(name, '/') == NULL;
}

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

void add_child(Node *parent, Node *child){
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

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

Node *get_root(Node *node){
    while (node != NULL && node->parent != NULL) {
        node = node->parent;
    }

    return node;
}

char *skip_spaces(char *text){
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

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

bool has_extra_arguments(char *cursor){ return read_token(&cursor) != NULL; }

bool parse_no_arguments(char *cursor){
    if (has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

bool parse_one_argument(char *cursor, char **arg){
    *arg = read_token(&cursor);

    if (*arg == NULL || has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

bool parse_optional_argument(char *cursor, char **arg){
    *arg = read_token(&cursor);

    if (has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

bool parse_two_arguments(char *cursor, char **first, char **second){
    *first = read_token(&cursor);
    *second = read_token(&cursor);

    if (*first == NULL || *second == NULL || has_extra_arguments(cursor)) {
        print_error();
        return false;
    }

    return true;
}

bool parse_zapis_arguments(char *cursor, char **path, char **content){
    *path = read_token(&cursor);
    if (*path == NULL) {
        print_error();
        return false;
    }

    *content = skip_spaces(cursor);
    return true;
}

bool is_permission_text(const char *text){
    if (text == NULL || text[0] == '\0' || text[1] != '\0') {
        return false;
    }

    return text[0] >= '0' && text[0] <= '7';
}

int permission_value(const char *text){ return text[0] - '0'; }

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

    current = (path[0] == '/') ? get_root(start) : start;
    path_copy = copy_text(path);

    token = strtok(path_copy, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        }

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

Node *resolve_path(const char *path){ return resolve_path_from(current_directory, path); }

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

Node *get_create_parent(const char *path, char **name_part){
    Node *parent = resolve_parent_directory(path, name_part);
    if (!is_directory(parent) || !is_valid_name(*name_part)) return NULL;
    return parent;
}

void swap_args(char **first, char **second){
    char *temp = *first;
    *first = *second;
    *second = temp;
}

void replace_content(Node *node, const char *new_content){
    free(node->content);
    node->content = copy_text(new_content == NULL ? "" : new_content);
}

void change_owner(Node *node, const char *owner){
    free(node->owner);
    node->owner = copy_text(owner);
}

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

void command_touch(char *path){
    Node *parent;
    Node *existing;
    Node *new_file;
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

    new_file = create_node(name_part, NODE_FILE, DEFAULT_OWNER, 7);
    add_child(parent, new_file);
    free(name_part);
}

void command_mkdir(char *path){
    Node *parent;
    Node *existing;
    Node *new_directory;
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

    new_directory = create_node(name_part, NODE_DIRECTORY, DEFAULT_OWNER, 7);
    add_child(parent, new_directory);
    free(name_part);
}

void command_rm(char *path){
    Node *target = resolve_path(path);

    if (target == NULL || target->parent == NULL || !has_write_permission(target->parent)) {
        print_error();
        return;
    }

    detach_child(target);
    free_tree(target);
}

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

void command_spusti(char *path){
    Node *target = resolve_path(path);

    if (!is_file(target) || !has_execute_permission(target)) {
        print_error();
    }
}

void command_cd(char *path){
    Node *target = resolve_path(path);

    if (!is_directory(target) || !has_execute_permission(target)) {
        print_error();
        return;
    }

    current_directory = target;
}

void command_zapis(char *path, char *content){
    Node *target = resolve_path(path);

    if (target == NULL || !has_write_permission(target)) {
        print_error();
        return;
    }

    replace_content(target, content);
}

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

void command_chown(char *first, char *second){
    Node *target;
    Node *first_node = resolve_path(first);
    Node *second_node = resolve_path(second);

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

bool execute_simple_command(char *command, char *cursor){
    char *first = NULL;
    char *second = NULL;

    if (strcmp(command, "ls") == 0) { if (!parse_optional_argument(cursor, &first)) return true; command_ls(first); return true; }
    if (strcmp(command, "touch") == 0) { if (!parse_one_argument(cursor, &first)) return true; command_touch(first); return true; }
    if (strcmp(command, "mkdir") == 0) { if (!parse_one_argument(cursor, &first)) return true; command_mkdir(first); return true; }
    if (strcmp(command, "rm") == 0) { if (!parse_one_argument(cursor, &first)) return true; command_rm(first); return true; }
    if (strcmp(command, "vypis") == 0) { if (!parse_one_argument(cursor, &first)) return true; command_vypis(first); return true; }
    if (strcmp(command, "spusti") == 0) { if (!parse_one_argument(cursor, &first)) return true; command_spusti(first); return true; }
    if (strcmp(command, "cd") == 0) { if (!parse_one_argument(cursor, &first)) return true; command_cd(first); return true; }
    if (strcmp(command, "chmod") == 0) { if (!parse_two_arguments(cursor, &first, &second)) return true; command_chmod(first, second); return true; }
    if (strcmp(command, "chown") == 0) { if (!parse_two_arguments(cursor, &first, &second)) return true; command_chown(first, second); return true; }

    print_error();
    return true;
}

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

    if (strcmp(command, "quit") == 0) {
        if (!parse_no_arguments(cursor)) {
            return true;
        }
        return false;
    }

    if (strcmp(command, "zapis") == 0) {
        if (!parse_zapis_arguments(cursor, &path, &content)) {
            return true;
        }
        command_zapis(path, content);
        return true;
    }

    return execute_simple_command(command, cursor);
}

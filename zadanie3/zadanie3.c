#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/* vytvori kopiu retazca v dynamickej pamati */
static char *duplicate_string(const char *text) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        fprintf(stderr, "chyba pamate\n");
        exit(EXIT_FAILURE);
    }

    memcpy(copy, text, length + 1);
    return copy;
}

/* vytvori novy subor alebo adresar s nastavenymi udajmi */
static Node *create_node(const char *name, NodeType type, const char *owner, int permissions) {
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL) {
        fprintf(stderr, "chyba pamate\n");
        exit(EXIT_FAILURE);
    }

    node->name = duplicate_string(name);
    node->owner = duplicate_string(owner);
    node->content = duplicate_string("");
    node->permissions = permissions;
    node->type = type;
    node->parent = NULL;
    node->children = NULL;
    node->next = NULL;

    return node;
}

/* uvolni uzol aj cely jeho podstrom */
static void free_node(Node *node) {
    Node *child;
    Node *next_child;

    if (node == NULL) {
        return;
    }

    child = node->children;
    while (child != NULL) {
        next_child = child->next;
        free_node(child);
        child = next_child;
    }

    free(node->name);
    free(node->owner);
    free(node->content);
    free(node);
}

/* zisti, ci ma uzol pravo na citanie */
static int has_read_permission(const Node *node) {
    return (node->permissions & 4) != 0;
}

/* zisti, ci ma uzol pravo na zapis */
static int has_write_permission(const Node *node) {
    return (node->permissions & 2) != 0;
}

/* zisti, ci ma uzol pravo na spustenie alebo vstup */
static int has_execute_permission(const Node *node) {
    return (node->permissions & 1) != 0;
}

/* prevedie ciselne prava na textovy tvar rwx */
static void permissions_to_text(int permissions, char text[4]) {
    text[0] = (permissions & 4) != 0 ? 'r' : '-';
    text[1] = (permissions & 2) != 0 ? 'w' : '-';
    text[2] = (permissions & 1) != 0 ? 'x' : '-';
    text[3] = '\0';
}

/* skontroluje, ci je nazov polozky platny */
static int is_valid_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }

    return strchr(name, '/') == NULL;
}

/* najde priameho potomka v adresari podla nazvu */
static Node *find_child(Node *directory, const char *name) {
    Node *current = directory->children;

    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/* prida potomka do adresara */
static void add_child(Node *parent, Node *child) {
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

/* odpoji potomka od rodica bez uvolnenia pamate */
static void detach_child(Node *child) {
    Node *parent;
    Node *current;

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

/* porovnanie uzlov pre triedenie podla nazvu */
static int compare_nodes_by_name(const void *left, const void *right) {
    const Node *left_node = *(const Node * const *)left;
    const Node *right_node = *(const Node * const *)right;
    return strcmp(left_node->name, right_node->name);
}

/* spocita pocet poloziek v adresari */
static int count_children(const Node *directory) {
    int count = 0;
    const Node *current = directory->children;

    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}

/* vypise jednu polozku v tvare nazov vlastnik prava */
static void print_node_info(const Node *node) {
    char permissions_text[4];

    permissions_to_text(node->permissions, permissions_text);
    printf("%s %s %s\n", node->name, node->owner, permissions_text);
}

/* vypise obsah adresara v utriedenom poradi */
static void print_directory_listing(const Node *directory) {
    int child_count;
    int index = 0;
    Node **sorted_children;
    Node *current;

    child_count = count_children(directory);
    if (child_count == 0) {
        printf("ziaden subor\n");
        return;
    }

    sorted_children = (Node **)malloc(sizeof(Node *) * (size_t)child_count);
    if (sorted_children == NULL) {
        fprintf(stderr, "chyba pamate\n");
        exit(EXIT_FAILURE);
    }

    current = directory->children;
    while (current != NULL) {
        sorted_children[index++] = current;
        current = current->next;
    }

    qsort(sorted_children, (size_t)child_count, sizeof(Node *), compare_nodes_by_name);

    for (index = 0; index < child_count; index++) {
        print_node_info(sorted_children[index]);
    }

    free(sorted_children);
}

/* vrati korenovy adresar stromu */
static Node *get_root(Node *node) {
    while (node != NULL && node->parent != NULL) {
        node = node->parent;
    }

    return node;
}

/* najde uzol podla relativnej alebo absolutnej cesty */
static Node *resolve_path(Node *current_directory, const char *path) {
    Node *current;
    char *path_copy;
    char *token;

    if (path == NULL || path[0] == '\0') {
        return current_directory;
    }

    if (strcmp(path, "/") == 0) {
        return get_root(current_directory);
    }

    current = path[0] == '/' ? get_root(current_directory) : current_directory;
    path_copy = duplicate_string(path);

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

        if (current->type != NODE_DIRECTORY) {
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

/* z cesty ziska rodicovsky adresar a meno ciela */
static Node *resolve_parent_directory(Node *current_directory, const char *path, char **name_part) {
    Node *parent_directory;
    char *path_copy;
    char *last_separator;

    *name_part = NULL;
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    path_copy = duplicate_string(path);

    while (strlen(path_copy) > 1 && path_copy[strlen(path_copy) - 1] == '/') {
        path_copy[strlen(path_copy) - 1] = '\0';
    }

    last_separator = strrchr(path_copy, '/');
    if (last_separator == NULL) {
        parent_directory = current_directory;
        *name_part = duplicate_string(path_copy);
        free(path_copy);
        return parent_directory;
    }

    if (last_separator == path_copy) {
        parent_directory = get_root(current_directory);
        *name_part = duplicate_string(last_separator + 1);
        free(path_copy);
        return parent_directory;
    }

    *last_separator = '\0';
    *name_part = duplicate_string(last_separator + 1);
    parent_directory = resolve_path(current_directory, path_copy);
    free(path_copy);
    return parent_directory;
}

/* prepise obsah suboru alebo adresara novym textom */
static void replace_content(Node *node, const char *new_content) {
    free(node->content);
    node->content = duplicate_string(new_content);
}

/* preskoci uvodne medzery a tabulatory */
static char *skip_spaces(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

/* odstrani koncove biele znaky zo vstupneho riadku */
static void trim_trailing_spaces(char *text) {
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

/* nacita dalsi argument oddeleny bielymi znakmi */
static char *read_next_token(char **cursor) {
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

/* vypise chybu pri nedostatocnych pravach */
static void print_permission_error(void) {
    printf("chyba prav\n");
}

/* vypise vseobecnu chybu */
static void print_generic_error(void) {
    printf("chyba\n");
}

/* spracuje prikaz ls */
static void handle_ls(Node *current_directory, const char *argument) {
    Node *target;

    if (argument == NULL) {
        if (!has_read_permission(current_directory)) {
            print_permission_error();
            return;
        }

        print_directory_listing(current_directory);
        return;
    }

    target = resolve_path(current_directory, argument);
    if (target == NULL) {
        print_generic_error();
        return;
    }

    print_node_info(target);
}

/* spracuje prikaz touch */
static void handle_touch(Node *current_directory, const char *path) {
    Node *parent_directory;
    Node *existing;
    Node *new_file;
    char *name_part;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    parent_directory = resolve_parent_directory(current_directory, path, &name_part);
    if (parent_directory == NULL || name_part == NULL || !is_valid_name(name_part)) {
        free(name_part);
        print_generic_error();
        return;
    }

    if (parent_directory->type != NODE_DIRECTORY) {
        free(name_part);
        print_generic_error();
        return;
    }

    if (!has_write_permission(parent_directory)) {
        free(name_part);
        print_permission_error();
        return;
    }

    existing = find_child(parent_directory, name_part);
    if (existing != NULL) {
        free(name_part);
        print_generic_error();
        return;
    }

    new_file = create_node(name_part, NODE_FILE, DEFAULT_OWNER, 7);
    add_child(parent_directory, new_file);
    free(name_part);
}

/* spracuje prikaz mkdir */
static void handle_mkdir(Node *current_directory, const char *path) {
    Node *parent_directory;
    Node *existing;
    Node *new_directory;
    char *name_part;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    parent_directory = resolve_parent_directory(current_directory, path, &name_part);
    if (parent_directory == NULL || name_part == NULL || !is_valid_name(name_part)) {
        free(name_part);
        print_generic_error();
        return;
    }

    if (parent_directory->type != NODE_DIRECTORY) {
        free(name_part);
        print_generic_error();
        return;
    }

    if (!has_write_permission(parent_directory)) {
        free(name_part);
        print_permission_error();
        return;
    }

    existing = find_child(parent_directory, name_part);
    if (existing != NULL) {
        free(name_part);
        print_generic_error();
        return;
    }

    new_directory = create_node(name_part, NODE_DIRECTORY, DEFAULT_OWNER, 7);
    add_child(parent_directory, new_directory);
    free(name_part);
}

/* spracuje prikaz rm */
static void handle_rm(Node *current_directory, const char *path) {
    Node *target;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    target = resolve_path(current_directory, path);
    if (target == NULL || target->parent == NULL) {
        print_generic_error();
        return;
    }

    if (!has_write_permission(target->parent)) {
        print_permission_error();
        return;
    }

    detach_child(target);
    free_node(target);
}

/* spracuje prikaz vypis */
static void handle_vypis(Node *current_directory, const char *path) {
    Node *target;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    target = resolve_path(current_directory, path);
    if (target == NULL || target->type != NODE_FILE) {
        print_generic_error();
        return;
    }

    if (!has_read_permission(target)) {
        print_permission_error();
        return;
    }

    if (target->content == NULL || target->content[0] == '\0') {
        printf("ok\n");
        return;
    }

    printf("%s\n", target->content);
}

/* spracuje prikaz spusti */
static void handle_spusti(Node *current_directory, const char *path) {
    Node *target;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    target = resolve_path(current_directory, path);
    if (target == NULL || target->type != NODE_FILE) {
        print_generic_error();
        return;
    }

    if (!has_execute_permission(target)) {
        print_permission_error();
        return;
    }

    printf("ok\n");
}

/* spracuje prikaz cd */
static void handle_cd(Node **current_directory, const char *path) {
    Node *target;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    target = resolve_path(*current_directory, path);
    if (target == NULL || target->type != NODE_DIRECTORY) {
        print_generic_error();
        return;
    }

    if (!has_execute_permission(target)) {
        print_permission_error();
        return;
    }

    *current_directory = target;
}

/* spracuje prikaz zapis */
static void handle_zapis(Node *current_directory, const char *path, const char *content) {
    Node *target;

    if (path == NULL) {
        print_generic_error();
        return;
    }

    target = resolve_path(current_directory, path);
    if (target == NULL) {
        print_generic_error();
        return;
    }

    if (!has_write_permission(target)) {
        print_permission_error();
        return;
    }

    replace_content(target, content == NULL ? "" : content);
    printf("ok\n");
}

/* spracuje prikaz chmod */
static void handle_chmod(Node *current_directory, const char *permissions_text, const char *path) {
    Node *target;
    long permissions_value;
    char *end_pointer;

    if (permissions_text == NULL || path == NULL) {
        print_generic_error();
        return;
    }

    permissions_value = strtol(permissions_text, &end_pointer, 10);
    if (*end_pointer != '\0' || permissions_value < 0 || permissions_value > 7) {
        print_generic_error();
        return;
    }

    target = resolve_path(current_directory, path);
    if (target == NULL) {
        print_generic_error();
        return;
    }

    target->permissions = (int)permissions_value;
}

/* spracuje prikaz chown */
static void handle_chown(Node *current_directory, const char *owner, const char *path) {
    Node *target;

    if (owner == NULL || path == NULL || owner[0] == '\0') {
        print_generic_error();
        return;
    }

    target = resolve_path(current_directory, path);
    if (target == NULL) {
        print_generic_error();
        return;
    }

    free(target->owner);
    target->owner = duplicate_string(owner);
}

/* rozpozna prikaz zo vstupu a zavola prislusnu obsluhu */
static int process_command(Node **current_directory, char *line) {
    char *cursor = line;
    char *command;
    char *first_argument;
    char *second_argument;
    char *remaining_text;

    trim_trailing_spaces(line);
    cursor = skip_spaces(cursor);
    if (*cursor == '\0') {
        return 1;
    }

    command = read_next_token(&cursor);

    if (strcmp(command, "quit") == 0) {
        return 0;
    }

    if (strcmp(command, "ls") == 0) {
        first_argument = read_next_token(&cursor);
        handle_ls(*current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "touch") == 0) {
        first_argument = read_next_token(&cursor);
        handle_touch(*current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "mkdir") == 0) {
        first_argument = read_next_token(&cursor);
        handle_mkdir(*current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "rm") == 0) {
        first_argument = read_next_token(&cursor);
        handle_rm(*current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "vypis") == 0) {
        first_argument = read_next_token(&cursor);
        handle_vypis(*current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "spusti") == 0) {
        first_argument = read_next_token(&cursor);
        handle_spusti(*current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "cd") == 0) {
        first_argument = read_next_token(&cursor);
        handle_cd(current_directory, first_argument);
        return 1;
    }

    if (strcmp(command, "zapis") == 0) {
        first_argument = read_next_token(&cursor);
        remaining_text = skip_spaces(cursor);
        handle_zapis(*current_directory, first_argument, remaining_text);
        return 1;
    }

    if (strcmp(command, "chmod") == 0) {
        first_argument = read_next_token(&cursor);
        second_argument = read_next_token(&cursor);
        handle_chmod(*current_directory, first_argument, second_argument);
        return 1;
    }

    if (strcmp(command, "chown") == 0) {
        first_argument = read_next_token(&cursor);
        second_argument = read_next_token(&cursor);
        handle_chown(*current_directory, first_argument, second_argument);
        return 1;
    }

    print_generic_error();
    return 1;
}

/* nacitava prikazy az do ukoncenia programu */
int main(void) {
    Node *root = create_node("/", NODE_DIRECTORY, DEFAULT_OWNER, 7);
    Node *current_directory = root;
    char line[MAX_LINE_LENGTH];

    /* hlavny cyklus prikazoveho riadku */
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (!process_command(&current_directory, line)) {
            break;
        }
    }

    free_node(root);
    return 0;
}

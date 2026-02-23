#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_LINE 512
#define HASH_KEY "moj_tajny_kluc"
char* name = NULL;
char* password = NULL;
char* key = NULL;
FILE* users = NULL;

void declare_memory();
void validate_user();
void hash_password(char* plain, char* out_hex, char* key_str);
void open_file();
void trim(char *s);
bool check_keys(char* secret_keys, bool* line_changed, char* new_keys);


int main(){
    declare_memory();

    printf("meno: ");
    scanf("%99s", name);
    printf("heslo: ");
    scanf("%99s", password);
    printf("overovaci kluc: ");
    scanf("%99s", key);

    trim(name);
    trim(password);
    trim(key);

    open_file();
    validate_user();

    if (users) fclose(users);
    free(name);
    free(password);
    free(key);
    return 0;
}

void trim(char *s) {
    // odstrani \n a \r z konca stringu
    s[strcspn(s, "\r\n")] = 0;
}

bool check_keys(char* secret_keys, bool* line_changed, char* new_keys){
    *line_changed = false;
    bool found = false;           
    char *token = strtok(secret_keys, ",");

    while (token != NULL) {
        if (strcmp(token, key) == 0 && !found) { 
            found = true; // najdena zhoda medzi klucmi
            *line_changed = true;
        } else {
            if (strlen(new_keys) > 0) {
                strcat(new_keys, ",");
            }
            strcat(new_keys, token); // zaplnanie klucmi
        }
        token = strtok(NULL, ",");
    }

    return found;
}

void hash_password(char* plain, char* out_hex, char* key_str) {
    int key_len = strlen(key_str);
    int plain_len = strlen(plain);
    for (int i = 0; i < plain_len; i++) {
        sprintf(out_hex + (i * 2), "%02x", (unsigned char)plain[i] ^ (unsigned char)key_str[i % key_len]);
    }
    out_hex[plain_len * 2] = '\0';
}

void validate_user(){
    char row[MAX_LINE];
    char* user_name;
    char* password_hash;
    char input_password_hash[256];
    char* secret_keys;
    bool success = false;

    hash_password(password, input_password_hash, HASH_KEY);
    FILE *temp_file = fopen("temp.csv", "w"); // pomocny subor
    if (!temp_file) return;

    while (fgets(row, MAX_LINE, users) != NULL){
        char original_row[MAX_LINE];
        strcpy(original_row, row); // zaloha riadku
        char row_for_tokens[MAX_LINE];
        strcpy(row_for_tokens, row);
        trim(row_for_tokens);
        
        bool line_changed = false;
        char new_keys[MAX_LINE] = "";

        user_name = strtok(row_for_tokens, ":");
        password_hash = strtok(NULL, ":");
        secret_keys = strtok(NULL, ":");

        if (user_name && password_hash && secret_keys) {            
            if (strcmp(password_hash, input_password_hash) == 0 && strcmp(user_name, name) == 0) {
                if (check_keys(secret_keys, &line_changed, new_keys)){
                    success = true;
                    fprintf(temp_file, "%s:%s:%s\n", user_name, password_hash, new_keys); // zapis pozmeneneho riadku

                    while (fgets(row, MAX_LINE, users) != NULL) { // skopirovanie zvysku suboru
                        fprintf(temp_file, "%s", row);
                    }
                    break;
                }
            }
        }
        
        // kopirovanie riadku
        if (!line_changed) {
            fprintf(temp_file, "%s", original_row);
        }
    }

    fclose(users);
    users = NULL;
    fclose(temp_file);

    // vymena suborov
    if (success) {
        remove("hesla.csv");             
        if (rename("temp.csv", "hesla.csv") == 0) {
            printf("ok\n");
        } else {
            printf("chyba\n");
        }
    } else {
        remove("temp.csv");         
        printf("chyba\n");
    }
}

void open_file(){
    users = fopen("hesla.csv","r");

    if (users == NULL) {
        printf("chyba\n");
        exit(0);
    }
    
}

void declare_memory(){
    name = malloc(100 * sizeof(char));
    if (name == NULL){
        printf("chyba\n");
        exit(0);
    } 

    password = malloc(100 * sizeof(char));
    if (password == NULL) {
        printf("chyba\n");
        exit(0);
    } 

    key = malloc(100 * sizeof(char));
    if (key == NULL) {
        printf("chyba\n");
        exit(0);
    } 
}


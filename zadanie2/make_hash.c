#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

void hash_password(char* plain, char* out_hex, char* key);

int main() {
    FILE *in = fopen("info.txt", "r");
    FILE *out = fopen("hesla.csv", "w");

    if (!in || !out) {
        printf("chyba\n");
        return 1;
    }

    char line[100];
    char *key = "moj_tajny_kluc";

    while (fgets(line, sizeof(line), in)) {
        line[strcspn(line, "\n")] = 0; // odstranenie newline
        
        char *name = strtok(line, ":");
        char *password = strtok(NULL, ":");
        
        if (name && password) {
            char hashed[256];
            hash_password(password, hashed, key);            
            
            srand(time(NULL)); 
          
            fprintf(out, "%s:%s:", name, hashed);
            for (int i = 0; i < 10; i++) {
                int r = (rand() % 9000) + 1000; // nahodne 4-ciferne cislo
                fprintf(out, "%d%s", r, (i == 9) ? "" : ",");
            }
            fprintf(out, "\n");
        }
    }


    fclose(in);
    fclose(out);
    return 0;
}

void hash_password(char* plain, char* out_hex, char* key) {
    int passwd_len = strlen(key);
    for (int i = 0; i < strlen(plain); i++) {
        sprintf(out_hex + (i * 2), "%02x", (unsigned char)plain[i] ^ (unsigned char)key[i % passwd_len]);
    }
    out_hex[strlen(plain) * 2] = '\0';
}
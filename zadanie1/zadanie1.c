#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool encrypt = false;
bool decrypt = false;
char* password = NULL;
char *input_file = NULL;
char *output_file = NULL;
FILE *file_in = NULL;
FILE *file_out = NULL;

void validate_arguments();
void parse_arguments(int argc, char* argv[]);
char* get_value(int argc, char* argv[], int* i);
void open_files();
void process_file();


int main(int argc, char* argv[]){
    parse_arguments(argc, argv);
    validate_arguments();
    open_files();
    process_file();
    
    fclose(file_in);
    fclose(file_out);

    return 0;
}


void process_file(){
    int c;
    int i = 0;
    int passwd_len = strlen(password);

    while ((c = fgetc(file_in)) != EOF){
        int byte = c ^ (unsigned char)password[i % passwd_len];
        fputc(byte, file_out);
        i++;
    }
}

char* get_value(int argc, char* argv[], int* i){
    (*i)++;
    if (*i >= argc) {
        printf("chyba\n");
        exit(1);
    }

    if (strlen(argv[*i]) == 0) {
        printf("chyba\n");
        exit(1);
    }

    if (argv[*i][0] == '-') {   // ak nasleduje prepinac
        printf("chyba\n");
        exit(1);
    }    

    return argv[*i];
}

// otvorenie suborov
void open_files(){
    file_in = fopen(input_file, "rb");
    if (file_in == NULL) {
        printf("chyba\n");
        exit(1);
    }

    file_out = fopen(output_file, "wb");
    if (file_out == NULL) {
        fclose(file_in);
        printf("chyba\n");
        exit(1);
    }
}

void validate_arguments(){
    if (encrypt == decrypt) {   // nemozeme sifrovat aj desifrovat zaroven
        printf("chyba\n");
        exit(1);
    }

    if (password == NULL || input_file == NULL || output_file == NULL){
        printf("chyba\n");
        exit(1);
    }
}

void parse_arguments(int argc, char* argv[]){
    for (int i = 1; i < argc; i++){
        char* arg = argv[i];

        // kontrola encryptovania 
        if (strcmp(arg, "-s") == 0) {                                                
            if (encrypt || decrypt) { 
                printf("chyba\n"); 
                exit(1); 
            }    
            encrypt = true;
        }                        
        // kontrola decryptovania
        else if (strcmp(arg, "-d") == 0) {                                          
            if (encrypt || decrypt) { 
                printf("chyba\n"); 
                exit(1); 
            }                                        
            decrypt = true;  
        }                      
        // ziskanie hesla ak bolo zadane   
        else if (strcmp(arg, "-p") == 0) { 
            if (password != NULL) { 
                printf("chyba\n"); 
                exit(1); 
            }
            password = get_value(argc, argv, &i); 
        }     
        // ziskanie vstupneho suboru
        else if (strcmp(arg, "-i") == 0) {
            if (input_file != NULL) { 
                printf("chyba\n"); 
                exit(1); 
            }
            input_file = get_value(argc, argv, &i); 
        }
        // ziskanie vystupneho suboru
        else if (strcmp(arg, "-o") == 0) {
            if (output_file != NULL) { 
                printf("chyba\n"); 
                exit(1); 
            }
            output_file = get_value(argc, argv, &i);  
        } 
        // vsetky ostatne pripady
        else {                                                                      
            printf("chyba\n");
            exit(1);        
        }
    }
}


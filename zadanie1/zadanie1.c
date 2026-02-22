#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool encrypt = false;
bool decrypt = false;
char* password = NULL;
char *input_file = NULL;
char *output_file = NULL;

void validate_arguments();
void parse_arguments(int argc, char* argv[]);
char* get_value(int argc, char* argv[], int* i);


int main(int argc, char* argv[]){
    printf("Entered: %d", argc);
    parse_arguments(argc, argv);
    validate_arguments();
    

    return 0;
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


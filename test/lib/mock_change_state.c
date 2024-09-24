#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "include/ref_syscall.h"
#include "include/mock.h"

const char *startup_error_message = "Cannot operate test without password\nlaunch 'program.out -pwd password -conf conf_file-path'";
void mock_password(int argc, char **argv, password_t *password){
    
    int check = 0;
    int i;
    size_t len;

    printf("preparing mock\n");
    for (i = 1; i < argc; i++){
        if (!strcmp(argv[i], "-pwd")){
            password->good_password = argv[i + 1];
            check = 1;
            break;
        }
    } 
    
    if (!check){
        printf("Cannot operate test without password");
        exit(EXIT_FAILURE);
    }
    
    len = strlen(password->good_password);
    
    

    password->bad_password = malloc(sizeof(char)*len);

    if (!password->bad_password){
        perror("malloc bad_password");
        exit(EXIT_FAILURE);
    }
    
    
    for (i = 0; i < len; i++){
        password->bad_password[i] = password->good_password[i] + 1;
    }
    password->bad_password[len] = '\0';
    printf("mock ready good password: %s - %s bad_password\n", password->good_password, password->bad_password);
}


int mock_get_state()
{
    FILE *file;
    int state;
    char buff[16];
    long byte_read;
    file = fopen("/sys/module/the_reference_monitor/parameters/current_state", "r");
    if (file == NULL){
        printf("%s cannot open current_state", __func__);
        perror("");
        return -errno;
    }

    byte_read = fread(buff, sizeof(char), sizeof(buff), file);
    buff[byte_read] = 0;

    state = atoi(buff);
    return state;
}





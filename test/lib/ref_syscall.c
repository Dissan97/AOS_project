#include "include/ref_syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int syscall_cache[3];
int empty = 1;

int get_syscal_number (int sys_number){
    if (sys_number < 0 || sys_number > 2){
        return -1;
    }
    if (empty){
        FILE *f;
        int count = 0;
        char *token;
        
        f = fopen("/sys/module/the_usctm/parameters/free_entries", "r");
        
        if (f == NULL){
            perror("cannot open");
            return EXIT_FAILURE;
        }
    
        char buff [256];
        long r = fread(buff, sizeof(char), sizeof(buff), f);
        buff[r] = 0;
        fclose(f);
        token = strtok(buff, ",");

        while (token != NULL && count < 3) {
            syscall_cache[count] = atoi(token);
            count++;
            token = strtok(NULL, ",");
        }

        empty = 0;
    }

    return syscall_cache[sys_number];

}

int change_state(const char *pwd, int state)
{
    return syscall(get_syscal_number(CHANGE_STATE), pwd, state);
}

int change_path(const char *pwd, const char *path, int op)
{
    return syscall(get_syscal_number(CHANGE_PATH), pwd, path, op);
}
int change_password(const char *old_pwd, const char *new_pwd)
{
    return syscall(get_syscal_number(CHANGE_PASSWORD), old_pwd, new_pwd);
}
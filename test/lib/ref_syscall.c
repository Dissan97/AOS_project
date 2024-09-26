#include "include/ref_syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

int syscall_cache[3];
int empty = 1;
#define PATH_BUFFER (1 << 8)
#define ACCESS_POINT "/proc/ref_syscall/access_point"
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

const char *on = "ON";
const char *off = "OFF";
const char *rec_on = "RON";
const char *rec_off = "ROF";
const char *add_path = "ADPTH";
const char *remove_path = "RMPHT";
const char *einval = "-1";

const char *get_str_state (int state){
    switch (state)
    {
    case ON:
        return on;
    case OFF:
        return off;
    case REC_ON:
       return rec_on;
    case REC_OFF:
        return rec_off;
    default:
        return einval;
    }
}

const char *get_str_path(int add_or_remove){
    if (add_or_remove == ADD_PATH){
        return add_path;
    }
    if (add_or_remove == REMOVE_PATH){
        return remove_path;
    }
    return einval;
}

int path_change_state(const char *password, int state){
    int fd;
    char buffer[PATH_BUFFER];    
    int ret = 0;
    
    fd = open(ACCESS_POINT, O_WRONLY, 0666);
    if (fd < 0){
		perror("file not exists");
        return -1;
	}

    sprintf(buffer, "cngst -pwd %s -opt %s", password, get_str_state(state));    
	
    if (write(fd, buffer, strlen(buffer)) < 0){
        ret = -1;
    }
    close(fd);
    return ret;
}

int path_change_path(const char *password, const char *the_path, int add_or_remove)
{
    int fd;
    char buffer[PATH_BUFFER];    
    int ret = 0;
    
    fd = open(ACCESS_POINT, O_WRONLY, 0666);
    if (fd < 0){
            perror("file not exists");
            return -1;
    }
    sprintf(buffer, "cngpth -pwd %s -pth %s -opt %s", password, the_path, get_str_path(add_or_remove));
        
    if (write(fd, buffer, strlen(buffer)) < 0){
            ret = -1;
    }
    close(fd);
    return ret;

}

int path_change_password(const char *old_password, const char *new_password)
{
    int fd;
    char buffer[PATH_BUFFER];    
    int ret = 0;
    
    fd = open(ACCESS_POINT, O_WRONLY, 0666);
    if (fd < 0){
            perror("file not exists");
            return -1;
    }
    sprintf(buffer, "cngpwd -old %s -new %s", old_password, new_password);
        
    if (write(fd, buffer, strlen(buffer)) < 0){
            ret = -1;
    }
    close(fd);
    return ret;
}
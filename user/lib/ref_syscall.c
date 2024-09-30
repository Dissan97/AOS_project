#include "include/ref_syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int syscall_cache[3];
int empty = 1;
#define PATH_BUFFER (1 << 8)
#define ACCESS_POINT "/proc/ref_syscall/access_point"

int get_syscal_number(int sys_number)
{
    if (sys_number < 0 || sys_number > 2) {
        return -1;
    }
    if (empty) {
        FILE *f;
        int count = 0;
        char *token;

        f = fopen("/sys/module/the_usctm/parameters/free_entries", "r");

        if (f == NULL) {
            perror("cannot open");
            return EXIT_FAILURE;
        }

        char buff[256];
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
int path_change_state(const char *password, int state)
{
    FILE *access_point = fopen(ACCESS_POINT, "w");
    char buffer[PATH_BUFFER];
    if (!access_point) {
        return -1;
    }

    sprintf(buffer, "cngst -pwd %s -opt %d", password, state);
    if (fprintf(access, "%s", buffer) < 0) {
        return -1;
    }

    return 0;
}

int path_change_path(const char *password, const char *the_path,
                     int add_or_remove)
{
    FILE *access_point = fopen(ACCESS_POINT, "w");
    char buffer[PATH_BUFFER];
    if (!access_point) {
        return -1;
    }

    sprintf(buffer, "cngst -pwd %s -pth %s -opt %d", password, the_path,
            add_or_remove);
    if (fprintf(access, "%s", buffer) < 0) {
        return -1;
    }

    return 0;
}

int path_change_password(const char *old_password, const char *new_password)
{
    FILE *access_point = fopen(ACCESS_POINT, "w");
    char buffer[PATH_BUFFER];
    if (!access_point) {
        return -1;
    }

    sprintf(buffer, "cngst -old %s -new %d", old_password, new_password);
    if (fprintf(access, "%s", buffer) < 0) {
        return -1;
    }

    return 0;
}

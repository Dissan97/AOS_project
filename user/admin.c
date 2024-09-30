#include "lib/include/ref_syscall.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYS_CS 0x1
#define SYS_CP 0x2
#define SYSS (SYS_CS | SYS_CP)
#define AUDIT if (1)

void setup_state(char *p, int *state)
{
    unsigned int i;
    char dummy[8] = {0};
    for (i = 0; i < strlen(p); i++) {
        dummy[i] = toupper(p[i]);
    }

    if (!strcmp(dummy, "ON")) {
        *state = ON;
    }
    if (!strcmp(dummy, "OFF")) {
        *state = OFF;
    }
    if (!strcmp(dummy, "REC_ON")) {
        *state = REC_ON;
    }
    if (!strcmp(dummy, "REC_OFF")) {
        *state = REC_OFF;
    }
}

void setup_op(char *p, int *op)
{
    unsigned int i;
    char dummy[8] = {0};

    for (i = 0; i < strlen(p); i++) {
        dummy[i] = toupper(p[i]);
    }
    if (!strcmp(dummy, "ADD")) {
        *op = ADD_PATH;
    }
    if (!strcmp(dummy, "REM")) {
        *op = REMOVE_PATH;
    }
}

int main(int argc, char **argv)
{
    char *pwd;
    int sys = 0x0;
    int i;
    char *path = NULL;
    int state = -1;
    int op = -1;

    if (argc < 4) {
        printf("pass at least this arguments (change state) [password -cs "
               "state] or (change path) [password -cp add|rem path]\n");
        return -1;
    }
    pwd = argv[1];
    AUDIT
    {
        printf("_____________________Program_debug_____________________\n");
        printf("ARGUMENT PASSED = %d\n", argc - 1);
        for (int i = 0; i < argc; i++) {
            printf("%s ", argv[i]);
            fflush(stdout);
        }
        printf("\n_______________________________________________________\n");
        fflush(stdout);
    }

    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-cs")) {
            sys |= SYS_CS;
            setup_state(argv[i + 1], &state);
        }
        if (!strcmp(argv[i], "-cp")) {
            sys |= SYS_CP;
            setup_op(argv[i + 1], &op);
            path = argv[i + 2];
        }
    }

    if (!sys) {
        printf("argument 1 -cs or -cp\n");
        return -1;
    }

    if ((sys & SYS_CS) && state != -1) {
        if (!change_state(pwd, state)) {
            printf("change state done successfully type dmesg\n");
        } else {
            perror("change state problem");
        }
    }

    if ((sys & SYS_CP) && op != -1 && path) {
        if (!change_path(pwd, path, op)) {
            printf("change path done successfully type dmesg\n");
        } else {
            perror("change path problem");
        }
    }
}

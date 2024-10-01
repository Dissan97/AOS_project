#include "lib/include/ref_syscall.h"

#include "lib/include/ref_syscall.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>


#define MAX_PASSWORD (1 << 6)
#define PATH_MAX (1 << 12)
#define MAX_COMMAND (1 << 5)



void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void show_help() {
    printf("Available commands:\n");
    printf("[0]: change_state\n");
    printf("[1]: change_path\n");
    printf("[2]: change_password\n");
    printf("[3]: help (to show this message)\n");
    printf("Type 'quit' to exit the program.\n");
}

void read_line(char *buffer, size_t size) {
    if (fgets(buffer, size, stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    }
}

void handle_change_state() {
    char password[MAX_PASSWORD];
    char input[MAX_COMMAND];
    int state = -1;

    printf("Enter the password: ");
    read_line(password, MAX_PASSWORD);

    printf("Select the state:\n");
    printf("[0]: ON\n");
    printf("[1]: OFF\n");
    printf("[2]: REC_ON\n");
    printf("[3]: REC_OFF\n");

    read_line(input, MAX_COMMAND);
    to_lowercase(input);

    if (strcmp(input, "0") == 0 || strcmp(input, "on") == 0) {
        state = ON;
    } else if (strcmp(input, "1") == 0 || strcmp(input, "off") == 0) {
        state = OFF;
    } else if (strcmp(input, "2") == 0 || strcmp(input, "rec_on") == 0) {
        state = REC_ON;
    } else if (strcmp(input, "3") == 0 || strcmp(input, "rec_off") == 0) {
        state = REC_OFF;
    } else {
        printf("Invalid input for change_state.\n");
        return;
    }

    if (change_state(password, state) < 0) {
        printf("change_state(%s, %d): ", password, state);
        fflush(stdout);
        perror("failed");
    }else {
        printf("state=%s changed successfully\n", (state == ON ? "on" : state == OFF ?
          "off" : state == REC_ON ? "rec_on" : "rec_off"));
        fflush(stdout);
    }
}

void handle_change_path() {
    char password[MAX_PASSWORD];
    char path[PATH_MAX];
    char input[MAX_COMMAND];
    int op = -1;

    printf("Enter the password: ");
    read_line(password, MAX_PASSWORD);

    printf("Select operation:\n");
    printf("[0]: addpath\n");
    printf("[1]: rmpath\n");

    read_line(input, MAX_COMMAND);
    to_lowercase(input);

    if (strcmp(input, "0") == 0 || strcmp(input, "addpath") == 0) {
        op = ADD_PATH;
    } else if (strcmp(input, "1") == 0 || strcmp(input, "rmpath") == 0) {
        op = REMOVE_PATH;
    } else {
        printf("Invalid input for change_path.\n");
        return;
    }

    printf("Enter the path: ");
    read_line(path, PATH_MAX);

    if (change_path(password, path, op) < 0) {
        printf("change_path(%s, %s, %d): ", password, path, op);
        fflush(stdout);
        perror("failed");
    }else {
        printf("path=%s added successfully\n", path);
        fflush(stdout);
    }
}

void handle_change_password() {
    char old_password[MAX_PASSWORD], new_password[MAX_PASSWORD];
    char input[MAX_COMMAND];

    printf("Enter the old password: ");
    read_line(old_password, MAX_PASSWORD);

    printf("Enter the new password: ");
    read_line(new_password, MAX_PASSWORD);

    printf("Confirm password change (y/n): ");
    read_line(input, MAX_COMMAND);
    if (tolower(input[0]) == 'y') {
        if (change_password(old_password, new_password) < 0) {
            printf("change_password failed\n");
            perror("error");
        }else {
            printf("password changed correctly\n");
        }
    } else {
        printf("Password change canceled.\n");
    }
}

int main() {
    char input[MAX_COMMAND];

    if (geteuid() != 0){
        printf("to run this application you need root priviledge\n");
        fflush(stdout);
        exit(EXIT_SUCCESS);
    }

    show_help();

    while (1) {
        printf("\nEnter a command >> ");
        read_line(input, MAX_COMMAND);

        to_lowercase(input); 

        if (strcmp(input, "quit") == 0) {
            printf("Exiting the program.\n");
            break;
        } else if (strcmp(input, "0") == 0 || strcmp(input, "change_state") == 0) {
            handle_change_state();
        } else if (strcmp(input, "1") == 0 || strcmp(input, "change_path") == 0) {
            handle_change_path();
        } else if (strcmp(input, "2") == 0 || strcmp(input, "change_password") == 0) {
            handle_change_password();
        } else if (strcmp(input, "3") == 0 || strcmp(input, "help") == 0) {
            show_help();
        } else {
            printf("Invalid command. Showing help:\n");
            show_help();
        }
    }

    return 0;
}

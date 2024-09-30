#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 256
#define PATH_MAX 4096
#define ARGS "-pwd <password> -threads <num_threads> -filepath <path_to_conf>"

char password[BUFFER_SIZE];
char pathname[PATH_MAX];
int threads;

typedef struct csv {
    char command[BUFFER_SIZE];
    char path[PATH_MAX];
    int add; // not used for execv
} csv_t;

// Function to execute command using execv
int execute_command(csv_t *cmd) {
    char *args[] = {cmd->command, cmd->path, NULL};
    pid_t pid = fork();

    if (pid == 0) { // Child process
        if (execv(cmd->command, args) == -1) {
            perror("execv failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("fork failed");
        return -1;
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Command '%s %s' executed successfully\n", cmd->command, cmd->path);
            return 0;
        } else {
            printf("Command '%s %s' failed\n", cmd->command, cmd->path);
            return -1;
        }
    }
    return 0;
}

// Function to undo all executed commands
void undo_commands(csv_t *commands) {
    pid_t pid = fork();
            
        if (pid == 0) { // Child process
            // Construct arguments for execv
            char *args[] = {"/bin/rm", "-rf", commands[0].path, NULL};

            // Execute rm -rf <path>
            if (execv("/bin/rm", args) == -1) {
                perror("execv failed for rm -rf");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            perror("fork failed for undo command");
        } else { // Parent process
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                printf("Successfully undid command: %s by removing %s\n", commands[0].command, commands[0].path);
            } else {
                printf("Failed to undo command: %s\n", commands[0].command);
            }
        }
}

// Function to read CSV file and populate the commands array
csv_t *read_csv(const char *filepath, int *command_count) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    csv_t *commands = NULL;
    char line[BUFFER_SIZE];
    int count = 0;

    while (fgets(line, sizeof(line), file)) {
        commands = realloc(commands, sizeof(csv_t) * (count + 1));
        if (!commands) {
            perror("Memory allocation failed");
            fclose(file);
            return NULL;
        }

        // Parse each line of the CSV file. Assuming the CSV format is "command,path,add"
        char *command = strtok(line, ",");
        char *path = strtok(NULL, ",");
        char *add = strtok(NULL, ",");

        if (command && path && add) {
            strncpy(commands[count].command, command, BUFFER_SIZE);
            strncpy(commands[count].path, path, PATH_MAX);
            commands[count].add = atoi(add); // Convert "add" to an integer
            count++;
        }
    }

    fclose(file);
    *command_count = count;
    return commands;
}

int main(int argc, char **argv) {
    int i;
    pthread_t *tids;

    if (argc < 7) {
        printf("%s cannot be open please pass %s %s\n", argv[0], argv[0], ARGS);
        exit(EXIT_FAILURE);
    }

    for (i = 1; i < argc; i++) {
        if (!strcmp("-pwd", argv[i])) {
            strcpy(password, argv[i + 1]);
        }
        if (!strcmp("-threads", argv[i])) {
            threads = (int)strtol(argv[i + 1], NULL, 10);
            if (threads == 0) {
                printf("your arg [%s %s] and threads set up to=%d cannot init the program\n", argv[i], argv[i + 1], threads);
                exit(EXIT_FAILURE);
            }
        }
        if (!strcmp("-filepath", argv[i])) {
            strcpy(pathname, argv[i + 1]);
        }
    }

    if (strlen(password) == 0 || strlen(pathname) == 0 || threads == 0) {
        printf("cannot init the program\n");
        exit(EXIT_FAILURE);
    }

    printf("password=%s, pathname=%s, threads=%d\n", password, pathname, threads);

    // Dynamically load commands from CSV file
    int command_count = 0;
    csv_t *commands = read_csv(pathname, &command_count);

    if (!commands) {
        printf("Failed to load commands from CSV file\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < command_count; ++i) {
        if (execute_command(&commands[i]) == -1) {
            printf("Execution failed, undoing previous commands...\n");
            break;
        }
    }

    sleep(5);
    undo_commands(commands);

    free(commands); // Free the dynamically allocated array
    return 0;
}

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "include/ref_syscall.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include "include/mock.h"



node_t* create_node(const char *path, int absolute, int id_dir, int add, int is_hardlink, const char *which_file, int create) {
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node) {
        return NULL;  
    }

    
    new_node->path = malloc(strlen(path) + 1);
    if (!new_node->path) {
        free(new_node);  
        return NULL;
    }
    strcpy(new_node->path, path);

    new_node->absolute = absolute;
    new_node->id_dir = id_dir;
    new_node->add = add;
    new_node->is_hardlink = is_hardlink;

    if (is_hardlink) {
        new_node->which_file = malloc(strlen(which_file) + 1);
        if (!new_node->which_file) {
            free(new_node->path);  
            free(new_node);
            return NULL;
        }
        strcpy(new_node->which_file, which_file);
    } else {
        new_node->which_file = NULL; 
    }

    new_node->create = create;
    new_node->is_black_listed = 0;

    new_node->next = NULL;
    new_node->prev = NULL; 
    return new_node;
}

void append_node(node_list_t *list, const char *path, int absolute, int id_dir, int add,
 int is_hardlink, const char *which_file, int create) {
    node_t *new_node = create_node(path, absolute, id_dir, add, is_hardlink, which_file, create);
    if (!new_node) {
        fprintf(stderr, "Failed to create a new node.\n");
        return;
    }

    
    if (!list->head) {
        list->head = new_node;  
        list->tail = new_node;  
    } else {     
        list->tail->next = new_node;  
        new_node->prev = list->tail;   
        list->tail = new_node;          
    }
    
}

void free_node(node_t *node) {
    if (node) {
        free(node->path);  
        if (node->which_file) {
            free(node->which_file);  
        }
        free(node);  
    }
}

void free_list(node_list_t *list) {
    node_t *current;

    
    current = list->head;
    while (current) {
        node_t *next = current->next;
        free_node(current);
        current = next;
    }

    list->head = NULL;     
    list->tail = NULL;     
}

int parse_csv(const char *filename, node_list_t *list) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open CSV file");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1; 
    }

    
    while (fgets(line, sizeof(line), file)) {
        char path[MAX_LINE_LENGTH];
        int absolute, id_dir, add, is_hardlink, create;
        char which_file[MAX_LINE_LENGTH] = "";  
        
        if (sscanf(line, "%255[^;];%d;%d;%d;%d;%255[^;];%d", path, &absolute, &id_dir, &add, &is_hardlink, which_file, &create) == 7) {
            append_node(list, path, absolute, id_dir, add, is_hardlink, which_file, create);  
        }
    }

    fclose(file);
    return 0;
}

void print_list(const node_list_t *list) {
    node_t *current;

    printf("Add List:\n");
    current = list->head;
    while (current) {
        printf("Path: %s, Absolute: %d, ID: %d, Add: %d, Is Hardlink: %d, Which File: %s, create: %d\n", 
            current->path, current->absolute, current->id_dir, current->add, current->is_hardlink, 
            current->which_file ? current->which_file : "NULL", current->create);
        current = current->next;
    }
}
int mkdir_p(const char *path, mode_t mode) {
    char temp_path[1024];
    char *p = NULL;
    size_t len;

    
    snprintf(temp_path, sizeof(temp_path), "%s", path);
    len = strlen(temp_path);

    
    if (temp_path[len - 1] == '/') {
        temp_path[len - 1] = '\0';
    }

    
    for (p = temp_path + 1; *p; p++) {
        if (*p == '/') {
            

            
            if (mkdir(temp_path, mode) == -1) {
                if (errno != EEXIST) {
                    perror("mkdir failed");
                    return -1;
                }
            }

            
        }
    }

    
    if (mkdir(temp_path, mode) == -1) {
        if (errno != EEXIST) {
            perror("mkdir failed");
            return -1;
        }
    }

    return 0;
}

int mock_gen_test_file_env(int argc, char **argv, node_list_t *current_list) {
    node_t *curr;
    char *conf_file;
    int is_correct = 0;
    for (int i = 1; i < argc; i++){
        if (strcmp(!argv[i], "-conf") == 0){
            is_correct = 1;
            conf_file = argv[i + 1];
        }
    }

    if (!is_correct){
        perror("Cannot run mock without configuration file");
    }

    if (parse_csv(conf_file, current_list)) {
        return -1;
    }

    for (curr = current_list->head; curr != NULL; curr = curr->next) {
        if (curr->id_dir) {
            
            if (mkdir_p(curr->path, 0755) == -1) {
                perror("mkdir_failed");
                exit(EXIT_FAILURE);
            }
        }else {
            if (curr->is_hardlink){
                if (link(curr->which_file, curr->path) < 0){
                    perror("link failed");
                    exit(EXIT_FAILURE);
                }
            }else {
                FILE *file = fopen(curr->path, "w");
                if (file == NULL){
                    perror("fopen failed");
                    exit(EXIT_FAILURE);
                }
                fprintf(file, "This is a test file");
                fclose(file);
            }
        }
    }

    return 0;
}

int mock_remove_recursive(const char *path) {
    struct stat statbuf;
    
    
    if (stat(path, &statbuf) != 0) {
        perror("stat failed");
        return -1;
    }

    
    if (S_ISDIR(statbuf.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            perror("opendir failed");
            return -1;
        }

        struct dirent *entry;
        char filepath[MAX_PATH_LENGTH];

        
        while ((entry = readdir(dir)) != NULL) {
            
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            
            snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

            
            if (mock_remove_recursive(filepath) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);

        
        if (rmdir(path) != 0) {
            perror("rmdir failed");
            return -1;
        }
    } else {
        
        if (unlink(path) != 0) {
            perror("unlink failed");
            return -1;
        }
    }

    return 0;
}

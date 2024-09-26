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
#include "include/mock_list.h"


#define MAX_DIR_STACK (1 << 8)

int parse_csv(const char *filename, node_list_t *list) {

    fflush(stdout);
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open CSV file");
        return -1;
    }
    fflush(stdout);
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


int mock_remove_recursive(const char *path, const char *conf_file) {
    struct stat statbuf;

    char conf_file_full_path[MAX_PATH_LENGTH];
    if (realpath(conf_file, conf_file_full_path) == NULL) {
        perror("realpath failed for conf_file");
        return -1;
    }

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
            snprintf(filepath, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);

            char full_entry_path[MAX_PATH_LENGTH];
            if (realpath(filepath, full_entry_path) == NULL) {
                perror("realpath failed");
                closedir(dir);
                return -1;
            }

    
            if (strcmp(full_entry_path, conf_file_full_path) == 0) {
                continue;
            }

    
            if (mock_remove_recursive(filepath, conf_file) != 0) {
                closedir(dir);
                return -1;
            }
        }

        closedir(dir);


        if (strcmp(path, "resources") != 0) {
    
            if (rmdir(path) != 0) {
                perror("rmdir failed");
                return -1;
            }
        }
    } else {

        char file_full_path[MAX_PATH_LENGTH];
        if (realpath(path, file_full_path) == NULL) {
            perror("realpath failed for file");
            return -1;
        }


        if (strcmp(file_full_path, conf_file_full_path) == 0) {
    
            return 0;
        }


        if (unlink(path) != 0) {
            perror("unlink failed");
            return -1;
        }
    }

    return 0;
}

int mock_gen_test_file_env(int argc, char **argv, change_path_mock_t *cngpth_mock) {
    node_t *curr;
    int is_correct = 0;
    cngpth_mock->list;
    
    for (int i = 1; i < argc - 1; i++){
        if (strcmp(argv[i], "-conf") == 0){
            
            cngpth_mock->conf_file = malloc(sizeof(char)*(strlen(argv[i + 1]) + 1));
            if (!cngpth_mock->conf_file){
                perror("error in malloc");
                exit(EXIT_FAILURE);
            }
    
            strcpy(cngpth_mock->conf_file, argv[i + 1]);
            cngpth_mock->conf_file;
    
            is_correct = 1;
            break;
        }
    }
    mock_remove_recursive("resources",cngpth_mock->conf_file);
    
    if (!is_correct){
        perror("Cannot run mock without configuration file");
        exit(EXIT_FAILURE);
    }
    fflush(stdout);
    if (parse_csv(cngpth_mock->conf_file, &cngpth_mock->list)) {
        exit(EXIT_FAILURE);
    }
    for (curr = cngpth_mock->list.head; curr != NULL; curr = curr->next) {
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
			printf("%s: %s ", __func__, curr->path);
			fflush(stdout);		
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



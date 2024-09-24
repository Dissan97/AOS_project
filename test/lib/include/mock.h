#pragma once

#define MAX_LINE_LENGTH (1 << 8) 
#define MAX_PATH_LENGTH (1 << 12)  

typedef struct password_struct {
    char *good_password;
    char *bad_password;
}password_t;

typedef struct node {
    char *path; 
    int absolute;
    int id_dir;
    int add;
    int is_hardlink;
    char *which_file;
    int create;
    int is_black_listed;
    struct node *next; 
    struct node *prev; 
} node_t;

typedef struct node_list {
    node_t *head;    
    node_t *tail;    
} node_list_t;


extern const char *password_error_message;
void mock_password(int argc, char **argv, password_t *password);
int mock_get_state();
int mock_gen_test_file_env(int argc, char **argv, node_list_t *current_list);
int mock_remove_recursive(const char *path);

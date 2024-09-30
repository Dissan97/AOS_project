#pragma once
#include "mock_list.h"
#define MAX_LINE_LENGTH (1 << 8)
#define MAX_PATH_LENGTH (1 << 12)

typedef struct password_struct {
        char *good_password;
        char *bad_password;
} password_t;

typedef struct change_path_mock {
        node_list_t list;
        char *conf_file;
} change_path_mock_t;

extern const char *password_error_message;
void mock_password(int argc, char **argv, password_t *password);
int mock_get_state();
int mock_gen_test_file_env(int argc, char **argv,
                           change_path_mock_t *cngpth_mock);
int mock_remove_recursive(const char *path, const char *conf_file);

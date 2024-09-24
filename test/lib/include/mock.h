#pragma once

typedef struct password_struct {
    char *good_password;
    char *bad_password;
}password_t;

extern const char *password_error_message;
void mock_password(int argc, char **argv, password_t *password);
int mock_get_state();

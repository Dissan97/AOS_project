#pragma once

#define ON 0x01          // 0000 0001
#define OFF 0x02         // 0000 0010
#define REC_ON 0x04      // 0000 0100
#define REC_OFF 0x08     // 0000 1000
#define ADD_PATH 0x10    // 0001 0000
#define REMOVE_PATH 0x20 // 0010 0000

#define CHANGE_STATE (0)
#define CHANGE_PATH (1)
#define CHANGE_PASSWORD (2)

int change_state(const char *password, int state);
int change_path(const char *password, const char *the_path, int add_or_remove);
int change_password(const char *old_password, const char *new_password);
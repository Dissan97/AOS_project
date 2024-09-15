#include "include/ref_syscall.h"
#include <unistd.h>

int change_state(const char *pwd, int state)
{
    return syscall(CHANGE_STATE, pwd, state);
}

int change_path(const char *pwd, const char *path, int op)
{
    return syscall(CHANGE_PATH, pwd, path, op);
}

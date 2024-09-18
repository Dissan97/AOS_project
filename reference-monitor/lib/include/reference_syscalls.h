#pragma once
#include <linux/atomic.h>
#include "reference_rcu_restrict_list.h"

#include <linux/atomic.h>

extern struct list_head restrict_path_list;
#ifdef NO_LOCK
extern atomic_long_t atomic_current_state;
#endif
int do_change_state(char*, int);
int do_change_path(char *,char *, int);
int do_change_password(char *, char *);

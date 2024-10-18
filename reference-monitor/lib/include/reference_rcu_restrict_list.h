#pragma once
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/path.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/stat.h>
#define RDONLY_MAKS ~(S_IWUSR | S_IWGRP | S_IWOTH)

struct rcu_restrict {
        char *path;
        unsigned long i_ino;
        unsigned int o_mode;  // old/original mode
        unsigned int o_flags; // old/original flags
        struct list_head node;
        struct rcu_head rcu;
};

extern spinlock_t restrict_path_lock;
extern struct list_head restrict_path_list;
// wait free

int forbitten_path(const char *);
int restore_black_list_entries(void);
int add_path(char *the_path, struct path path);
int del_path(char *);
int check_black_list(const char *path, int is_open);

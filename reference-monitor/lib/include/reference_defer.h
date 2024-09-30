#pragma once

#include "reference_hash.h"
#include "reference_types.h"
#include <linux/types.h>
#include <linux/workqueue.h>

#define DEVICE_WORK_DEFER_APPEND "APPEND ONLY DEFER DEVICE"
#define DEFER_WORK_QUEUE "DEFER WORK QUEUE"
struct work_data {
        struct work_struct work;
        pid_t tgid;
        pid_t tid;
        uid_t uid;
        uid_t euid;
        char *path_name;
        char *hash;
};
extern struct file_operations append_fops;
extern struct workqueue_struct *deferred_queue;

void defer_top_half(void (*work_func)(struct work_struct *));
void defer_bottom_half(struct work_struct *);
ssize_t append_defer_wrt(struct file *, const char *, size_t, loff_t *);
ssize_t append_defer_rd(struct file *, char *, size_t, loff_t *);
int append_defer_opn(struct inode *, struct file *);
int append_defer_rls(struct inode *, struct file *);

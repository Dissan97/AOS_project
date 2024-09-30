#pragma once
#include "reference_types.h"
#include <linux/kprobes.h>

extern unsigned long audit_counter;
extern struct kprobe probes[HOOKS_SIZE];

int pre_do_filp_open_handler(struct kprobe *, struct pt_regs *);
int pre_vfs_mk_rmdir_and_unlink_handler(struct kprobe *, struct pt_regs *);
// int pre_vfs_rmdir_handler(struct kprobe*, struct pt_regs *);
// int pre_vfs_unlink_handler(struct kprobe*, struct pt_regs *);

#pragma once
#include <linux/kprobes.h>
#include "reference_types.h"

extern unsigned long audit_counter;
extern struct kprobe probes[HOOKS_SIZE];

int vfs_open_wrapper(struct kprobe*, struct pt_regs *);
int vfs_mkrmdir_unlink_wrapper(struct kprobe*, struct pt_regs *);


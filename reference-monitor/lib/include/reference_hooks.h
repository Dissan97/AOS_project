#pragma once
#include "reference_types.h"
#include <linux/kprobes.h>

extern unsigned long audit_counter;
extern struct kretprobe probes[HOOKS_SIZE];
struct hook_return {
        int hook_error;
};
int pre_do_filp_open_handler(struct kretprobe_instance *ri, struct pt_regs *);
int pre_vfs_mk_rmdir_handler(struct kretprobe_instance *ri, struct pt_regs *);
int pre_vfs_unlink_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
int reference_kret_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
int post_dir_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
// int pre_vfs_rmdir_handler(struct kprobe*, struct pt_regs *);

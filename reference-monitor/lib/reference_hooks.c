#define EXPORT_SYMTAB
#include <asm/apic.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/dcache.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "include/reference_defer.h"
#include "include/reference_hooks.h"
#include "include/reference_path_solver.h"
#include "include/reference_rcu_restrict_list.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dissan Uddin Ahmed <dissanahmed@gmail.com>");

//#define REFERENCE_HOOK "reference-hook"
struct path dummy_path;
struct dentry dummy_dentry;

#define FORBITTEN_MASK (O_CREAT | O_RDWR | O_WRONLY | O_TRUNC | O_APPEND)
#define COMMON_MASK 0x8000

char *get_full_user_path(int dfd, const __user char *user_path)
{
        struct path path_struct;
        char *dummy_path;
        char *full_path;
        int error = -EINVAL;
        unsigned int lookup_flags = LOOKUP_FOLLOW;

        dummy_path = kmalloc(PATH_MAX, GFP_KERNEL);
        if (dummy_path == NULL)
                return NULL;

        error = user_path_at(dfd, user_path, lookup_flags, &path_struct);
        if (error) {
                kfree(dummy_path);
                return NULL;
        }

        full_path = d_path(&path_struct, dummy_path, PATH_MAX);
        path_put(&path_struct);
        // kfree(dummy_path);
        return full_path;
}

struct open_flags {
        int open_flag;
        umode_t mode;
        int acc_mode;
        int intent;
        int lookup_flags;
};

int pre_do_filp_open_handler(struct kprobe *ri, struct pt_regs *regs)
{
        struct filename *pathname = (struct filename *)regs->si;
        struct open_flags *op_flag = (struct open_flags *)(regs->dx); // arg2
        const char *filename = pathname->name;
        char *buffer;
        int ret;
        int flags = op_flag->open_flag;
        int is_parent = 0;

        if (filename == NULL) {
                return 0;
        }

        // for routine execution avoid to much overhead
        if (forbitten_path(filename)) {
                return 0;
        }

        buffer = kmalloc(PATH_MAX, GFP_KERNEL);
        if (!buffer) {
                pr_err("%s[%s]: kmalloc error\n", MODNAME, __func__);
                goto do_filp_open_pre_handler_mem_problem;
        }

        if (pathname->uptr == NULL) {
                kfree(buffer);
                return 0;
        }
        if (copy_from_user(buffer, pathname->uptr, PATH_MAX)) {
                kfree(buffer);
                return 0;
        }
        ret = get_filter_swap_or_parent(buffer, &is_parent);
        if (ret) {
                kfree(buffer);
                if (ret == -ENOMEM) {
                        goto do_filp_open_pre_handler_mem_problem;
                }
                return 0;
        }

        ret = check_black_list(buffer);

        if (!ret) {

                if (is_parent) {
                        if (flags & (O_CREAT | O_TMPFILE)) {
                                pr_err("%s[%s]: cannot create create file in "
                                       "this path=%s\n",
                                       MODNAME, __func__, buffer);
                                goto do_filp_open_defer_work;
                        }
                } else {
                        if (flags & (O_RDWR | O_WRONLY | O_TRUNC | O_APPEND)) {
                                pr_err(
                                    "%s[%s]: the file =%s cannot be modified\n",
                                    MODNAME, __func__, buffer);
                                goto do_filp_open_defer_work;
                        }
                }
        }

        kfree(buffer);
        return 0;

do_filp_open_defer_work:
        if (mark_indoe_read_only_by_pathname(buffer)) {
                pr_err("%s[%s]: unable to find inode for this pathname=%s\n",
                       MODNAME, __func__, buffer);
                kfree(buffer);
                return 0;
        }
        kfree(buffer);
        AUDIT
        pr_info("%s[%s]: calling work queue", MODNAME, __func__);
        defer_top_half(defer_bottom_half);
        // setting up bad file descriptor to make fail do_filp_open
do_filp_open_pre_handler_mem_problem:
#if defined(__x86_64__)
        regs->di = -1;
        ((struct open_flags *)(regs->dx))->open_flag = 0xFFFFFFFF;
#elif defined(__aarch64__)
        regs->regs[0] = -1;
        ((struct open_flags *)(regs->regs[1]))->open_flag = 0xFFFFFFFF;
#elif defined(__i386__)
        regs->bx = -1;
        ((struct open_flags *)(regs->cx))->open_flag = 0xFFFFFFFF;
#elif defined(__arm__)
        regs->ARM_r0 = -1;
        ((struct open_flags *)(regs->ARM_r1))->open_flag = 0xFFFFFFFF;
#endif
        return 0;
}

// todo check unlink

int pre_vfs_mk_rmdir_and_unlink_handler(struct kprobe *ri, struct pt_regs *regs)
{
#if defined(__x86_64__)
        // struct inode *dir = (struct inode *)regs->di;
        struct dentry *dentry = (struct dentry *)regs->si;
#elif defined(__aarch64__)
        struct dentry *dentry =
            (struct dentry *)
                regs->regs[1]; // AArch64: Second argument is in `x1` (regs[1])

#elif defined(__i386__)
        struct dentry *dentry =
            (struct dentry *)
                regs->cx; // x86 (32-bit): Second argument is in `cx`

#elif defined(__arm__)
        struct dentry *dentry =
            (struct dentry *)regs
                ->ARM_r1; // ARM (32-bit): Second argument is in `r1` (ARM_r1)
#endif
        char *buffer;
        int ret;

        buffer = kzalloc(PATH_MAX, GFP_KERNEL);
        if (!buffer) {
                pr_err("%s[%s]: kernel out of memory\n", MODNAME, __func__);
                return -ENOMEM;
        }
        // check the dentry if
        if (likely(dentry)) {
                if (dentry->d_parent) {
                        ret = fill_path_by_dentry(dentry->d_parent, buffer);
                        if (!check_black_list(buffer)) {
                                pr_err(
                                    "%s[%s]: cannot modify this directory=%s\n",
                                    MODNAME, __func__, buffer);
                                mark_inode_read_only(dentry->d_parent->d_inode);
                                goto store_thread_info_mkrmdir_unlink;
                        }
                }
        }
        kfree(buffer);
        return 0;

store_thread_info_mkrmdir_unlink:
        AUDIT
        pr_info("%s[%s]: calling work queue", MODNAME, __func__);
        defer_top_half(defer_bottom_half);
        kfree(buffer);
        return 0;
}

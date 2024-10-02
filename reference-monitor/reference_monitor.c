/*
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This module is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * @file reference_monitor.c
 * @brief This is the main source for the Linux Kernel Module which implements
 *       a Reference Monitor for locking writing operation on files or
 * directories
 *
 * @author Dissan Uddin Ahmed
 *
 * @date 2024, March, 11
 */

#define EXPORT_SYMTAB
#include <asm/apic.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/proc_fs.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/rwlock.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

/* *****************************************************
 *            CUSTOM LIBRARIES
 * *****************************************************
 */

#include "lib/include/reference_defer.h"
#include "lib/include/reference_hash.h"
#include "lib/include/reference_hooks.h"
#include "lib/include/reference_path_based_syscall.h"
#include "lib/include/reference_rcu_restrict_list.h"
#include "lib/include/reference_syscalls.h"
#include "lib/include/reference_types.h"
#include "lib/include/scth.h"

#define unload_syscall                                                         \
        unprotect_memory();                                                    \
        for (i = 0; i < HACKED_ENTRIES; i++) {                                 \
                ((unsigned long *)the_syscall_table)[restore[i]] =             \
                    the_ni_syscall;                                            \
        }                                                                      \
        protect_memory()

/**
 * *****************************************************
 *                 MODULE PARAMS
 * *****************************************************
 */

char pwd[DIGEST_SIZE] = {0};
module_param_string(pwd, pwd, sizeof(pwd), 0000);

unsigned int admin = 0xFFFFFFFF;
module_param(admin, uint, 0000);

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0444);

unsigned long current_state = OFF;
module_param(current_state, ulong, 0444);

unsigned long audit_counter = 0x0;
module_param(audit_counter, ulong, 0444);

unsigned long path_syscall_device_major = -1;
module_param(path_syscall_device_major, ulong, 0444);

char single_file_name[PATH_MAX] = {0};
module_param_string(single_file_name, single_file_name,
                    sizeof(single_file_name), 0444);

/**
 * *****************************************************
 *                 GLOBALS ARGS
 * *****************************************************
 */

rwlock_t password_lock;
unsigned long the_ni_syscall;
char salt[(SALT_LENGTH + 1)];

/*
 * *****************************************************
 *              STATE HANDLING SPINLOCK
 * *****************************************************
 */
// state_t reference_state;
#ifdef NO_LOCK
atomic_long_t atomic_current_state = ATOMIC_LONG_INIT(OFF);
#else
rwlock_t state_lock;
#endif

/*
 * *****************************************************
 *              SYSTEM CALLS
 * *****************************************************
 */

unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array) / sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ...(HACKED_ENTRIES - 1)] - 1};

/**
 *  *****************************************************
 *       SYSCALL DEFINITIONS
 *  *****************************************************
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
__SYSCALL_DEFINEx(2, _change_state, char *, pwd, int, state)
{

#else
asmlinkage long sys_change_state(char *pwd, int state)
{
#endif
        int ret;
        char from_user_password[MAX_FROM_USER] = {0};

        if (pwd == NULL) {
                pr_err("%s[%s]: password cannot be null\n", MODNAME, __func__);
                return -EINVAL;
        }

        ret = copy_from_user(from_user_password, pwd, MAX_FROM_USER);

        if (ret < 0) {
                pr_err("%s[%s]: cannot copy from the user the password\n",
                       MODNAME, __func__);
                return -EINVAL;
        }
        return do_change_state(from_user_password, state);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
__SYSCALL_DEFINEx(3, _change_paths, char *, pwd, char *, path, int, op)
{
#else
asmlinkage long sys_change_paths(char *pwd, char *path, int op)
{
#endif
        int ret;
        char from_user_password[MAX_FROM_USER] = {0};
        char *from_user_path;
        if (pwd == NULL || path == NULL) {
                pr_err("%s[%s]: path or password cannot be NULL\n", MODNAME,
                       __func__);
                return -EINVAL;
        }
        ret = copy_from_user(from_user_password, pwd, MAX_FROM_USER);
        if (ret < 0) {
                pr_warn("%s[%s]: cannot copy from the user the password\n",
                        MODNAME, __func__);
                return -EINVAL;
        }

        from_user_path = kzalloc(PATH_MAX, GFP_KERNEL);
        if (!from_user_path) {
                return -ENOMEM;
        }

        ret = copy_from_user(from_user_path, path, MAX_FROM_USER);
        if (ret < 0) {
                pr_warn("%s[%s]: cannot copy from the user the path\n", MODNAME,
                        __func__);
                return -EINVAL;
        }
        ret = do_change_path(from_user_password, from_user_path, op);
        kfree(from_user_path);
        return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
__SYSCALL_DEFINEx(2, _change_password, char *, old_password, char *,
                  new_password)
{
        int ret;
        char from_user_password_old[MAX_FROM_USER] = {0};
        char from_user_password_new[MAX_FROM_USER] = {0};

        ret =
            copy_from_user(from_user_password_old, old_password, MAX_FROM_USER);
        if (ret < 0) {
                pr_err("%s[%s]: cannot copy from the user the password old\n",
                       MODNAME, __func__);
                return -EACCES;
        }

        ret =
            copy_from_user(from_user_password_new, new_password, MAX_FROM_USER);
        if (ret < 0) {
                pr_err("%s[%s]: cannot copy from the user the password new\n",
                       MODNAME, __func__);
                return -EACCES;
        }

        return do_change_password(from_user_password_old,
                                  from_user_password_new);
#else
asmlinkage long sys_change_password(char *old_password, char *new_password)
{
        return do_change_password(old_password, new_password);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
long sys_change_state = (unsigned long)__x64_sys_change_state;
long sys_change_paths = (unsigned long)__x64_sys_change_paths;
long sys_change_password = (unsigned long)__x64_sys_change_password;
#else
#endif

/**
 * restrict_path_list
 */

// static struct kretprobe retprobe;

/*
 * *****************************************************
 *             RESTRICT PATH RCU_LIST
 * *****************************************************
 */

LIST_HEAD(restrict_path_list);
spinlock_t restrict_path_lock;

/*
 * *****************************************************
 *             KPROBES' STRUCT
 *             target_functions={do_filp_open, (vfs_mkdir, vfs_rmdir, vfs_unlink)}
 * *****************************************************
 */

struct kprobe probes[HOOKS_SIZE];

/*
 * *****************************************************
 *             PATH BASED SYSCALL DEVICE
 * *****************************************************
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static struct file_operations fops;
#else
struct proc_ops fops;
#endif
static struct proc_dir_entry *proc_folder;
static struct proc_dir_entry *proc_file;
#define proc_folder_name "ref_syscall"
#define proc_syscall_access_file "access_point"
/*
 * *****************************************************
 *             DEFER WORK 
 * *****************************************************
 */
struct workqueue_struct *deferred_queue;
char *file_appendonly;

static int __init reference_monitor_init(void)
{

        int i;
        int j;
        int ret;
        kuid_t kuid;

        const char target_functions[HOOKS_SIZE][16] = {
            "do_filp_open", "vfs_mkdir", "vfs_rmdir", "vfs_unlink"};
        const kprobe_pre_handler_t pre_handlers[HOOKS_SIZE] = {
            pre_do_filp_open_handler, pre_vfs_mk_rmdir_handler,
            pre_vfs_mk_rmdir_handler,
            pre_vfs_unlink_handler};

#if defined(__x86_64__)
        pr_info("Running on x86_64 architecture.\n");

#elif defined(__aarch64__)
        pr_info("Running on AArch64 (ARM 64-bit) architecture.\n");

#elif defined(__i386__)
        pr_info("Running on x86 (32-bit) architecture.\n");

#elif defined(__arm__)
        pr_info("Running on ARM (32-bit) architecture.\n");

#else
        pr_err("%s: Unsupported architecture. This module supports only "
               "x86_64, aarch64, i386, and arm\n",
               MODNAME);
        return -EPERM;
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 4, 233)
        pr_warn("%s: WARNING!!! THIS MODULE IS DEVELOPED ON KERNEL 5.4.105 ON "
                "UBUNTU 18 YOUR VERSION %d\n",
                MODNAME, LINUX_VERSION_CODE);
#endif
        // this parameter can be layter configurable cause administrator can be
        // another user
        admin = 0;
        kuid.val = admin;
        if (!uid_valid(kuid)) {
                pr_err("%s: cannot setup root=as_admin valid admin %d\n",
                       MODNAME, admin);
                return -EINVAL;
        }
        if (strlen(single_file_name) < 4) {
                pr_err("%s: not valid append filename\n", MODNAME);
                return -EINVAL;
        }
        AUDIT
        pr_info("%s: the reference monitor admin is (uid=%d)\n", MODNAME,
                admin);
        if (the_syscall_table == 0x0) {
                pr_err("%s: cannot manage sys_call_table address set to 0x0\n",
                       MODNAME);
                return -EINVAL;
        }

        if (strlen(pwd) <= 8) {
                pr_err("%s: password to short\n", MODNAME);
                return -EINVAL;
        }
        rwlock_init(&password_lock);
        rwlock_init(&state_lock);
        ret = generate_salt(salt);

        if (ret) {
                pr_err("%s: salt generation problem\n", MODNAME);
                return ret;
        }

        ret = hash_plaintext(salt, pwd, pwd);

        if (ret) {
                pr_err("%s: hash password problem\n", MODNAME);
                return ret;
        }
        // SYSCALL INSTALLATION
        AUDIT
        {
                pr_info("%s: queuing received sys_call_table address %px\n",
                        MODNAME, (void *)the_syscall_table);
                pr_info("%s: initializing - hacked entries %d\n", MODNAME,
                        HACKED_ENTRIES);
        }

        new_sys_call_array[0] = (unsigned long)sys_change_state;
        new_sys_call_array[1] = (unsigned long)sys_change_paths;
        new_sys_call_array[2] = (unsigned long)sys_change_password;
        ret = get_entries(restore, HACKED_ENTRIES,
                          (unsigned long *)the_syscall_table, &the_ni_syscall);

        if (ret != HACKED_ENTRIES) {
                pr_err("%s: could not hack %d entries (just %d)\n", MODNAME,
                       HACKED_ENTRIES, ret);

                return -1;
        }

        unprotect_memory();

        for (i = 0; i < HACKED_ENTRIES; i++) {
                ((unsigned long *)the_syscall_table)[restore[i]] =
                    (unsigned long)new_sys_call_array[i];
        }

        protect_memory();
        AUDIT
        pr_info(
            "%s: all new system-calls correctly installed on sys-call table\n",
            MODNAME);
        // KPROBE INSTALLATION
        for (i = 0; i < HOOKS_SIZE; i++) {

                probes[i].pre_handler = pre_handlers[i];
                probes[i].symbol_name = target_functions[i];

                ret = register_kprobe(&probes[i]);
                if (ret < 0) {
                        pr_err("%s: unbale to register kprobe for %s unloading "
                               "previous probes\n",
                               MODNAME, target_functions[i]);
                        for (j = 0; j < i; j++) {
                                pr_err(
                                    "%s: probe for target func=%s unrestired\n",
                                    MODNAME, target_functions[j]);
                                unregister_kprobe(&probes[j]);
                        }
                        goto error_syscall_installed;
                }
                disable_kprobe(&probes[i]);
                AUDIT
                pr_info(
                    "%s: kprobe for target_fun -> %s installed but disbled\n",
                    MODNAME, target_functions[i]);
        }

        AUDIT
        pr_info("%s: read hook module correctly loaded\n", MODNAME);

        // PATH BASED SYSTEM CALL INSTALLATION
        fops.owner = THIS_MODULE;
        fops.write = syscall_proc_write;
        fops.read = syscall_proc_read;

        proc_folder = proc_mkdir(proc_folder_name, NULL);
        if (proc_folder == NULL) {
                ret = -ENOMEM;
                pr_err("%s: proc directory creation failed failed\n", MODNAME);
                goto error_kprobe_installed;
        }

        proc_file =
            proc_create(proc_syscall_access_file, 0666, proc_folder, &fops);
        if (proc_file == NULL) {
                ret = -ENOMEM;
                pr_err("%s: proc access file creation_failed failed\n",
                       MODNAME);
                goto error_proc_folder_created;
        }

        AUDIT
        pr_info("%s: char device driver for syscall registered\n", MODNAME);

        deferred_queue =
            create_singlethread_workqueue(DEVICE_WORK_DEFER_APPEND);
        if (!deferred_queue) {
                pr_err("%s: cannot create the work queue=%s\n", MODNAME,
                       DEVICE_WORK_DEFER_APPEND);
                ret = -ENOMEM;
                goto error_proc_file_created;
        }
        AUDIT
        pr_info("%s: workqueue created\n", MODNAME);

        return ret;

error_proc_file_created:
        proc_remove(proc_file);
error_proc_folder_created:
        proc_remove(proc_folder);

error_kprobe_installed:
        for (i = 0; i < HOOKS_SIZE; i++) {
                unregister_kprobe(&probes[i]);
        }
error_syscall_installed:
        unload_syscall;

        return ret;
}

static void __exit reference_monitor_cleanup(void)
{

        int i;
        struct rcu_restrict *p;
        AUDIT
        pr_info("%s: shutting down\n", MODNAME);
        for (i = 0; i < HOOKS_SIZE; i++) {
                unregister_kprobe(&probes[i]);
        }
        AUDIT
        pr_info("%s: unregistered all kprobes\n", MODNAME);

        proc_remove(proc_file);
        proc_remove(proc_folder);
        AUDIT
        pr_info("%s: remoced the proc interface \n", MODNAME);

        AUDIT
        pr_info(
            "%s: safe restoring and releasing nodes in restrict_path_list\n",
            MODNAME);
        restore_black_list_entries();
        spin_lock(&restrict_path_lock);
        list_for_each_entry(p, &restrict_path_list, node)
        {
                list_del_rcu(&p->node);
                synchronize_rcu();
                kfree(p);
        }
        spin_unlock(&restrict_path_lock);
        AUDIT
        pr_info("%s:  deleted the_rcu_list\n", MODNAME);
        unload_syscall;
        AUDIT
        pr_info("%s: sys-call table restored to its original content\n",
                MODNAME);
        destroy_workqueue(deferred_queue);
        AUDIT
        pr_info("%s: work queue destryed\n", MODNAME);
}

module_init(reference_monitor_init);
module_exit(reference_monitor_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dissan Uddin Ahmed <DissanAhmed@gmail.com>");
MODULE_DESCRIPTION("Reference Monitor that protects updates on files or "
                   "directories based on some pathname");

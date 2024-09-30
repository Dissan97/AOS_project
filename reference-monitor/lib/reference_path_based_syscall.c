#define EXPORT_SYMTAB

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pid.h> /* For pid types */
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/signal_types.h>
#include <linux/syscalls.h>
#include <linux/tty.h>     /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */

#include "include/reference_path_based_syscall.h"
#include "include/reference_syscalls.h"
#include "include/reference_types.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dissan Uddin Ahmed <DissanAhmed@gmail.com>");

#define VFS_SYSCALL_MSG                                                        \
        "This pseudo file can be used to call system call of reference "       \
        "monitor\n"

char *list_sys_call[4] = {CHANGE_STATE, CHANGE_PATH, CHANGE_PASSWORD, NULL};
const char *list_sys_call_string =
    "[cngst=sys_change_state, cngpth=sys_chang_path, "
    "cngpwd=sys_change_password]";
DEFINE_MUTEX(syscall_proc_file_lock);

int check_avilable_sys_list(char *sys_name)
{

        int i;
        int len;
        int ret = -1;

        AUDIT
        pr_info("%s[%s]: request for this: %s", MODNAME, __func__, sys_name);
        len = strlen(sys_name);

        for (i = 0; list_sys_call[i] != NULL; i++) {

                if (!strncmp(list_sys_call[i], sys_name, strlen(sys_name))) {
                        AUDIT
                        pr_info("%s[%s]: found %s on avilable syscalls %s\n",
                                MODNAME, __func__, sys_name,
                                list_sys_call_string);
                        return i;
                }
                ret++;
        }

        return -EINVAL;
}

int parse_opt(char *op_str)
{

        int i;
        int j;
        char *argv[9];
        size_t len;
        int format = 0;
        char c;
        int ret;
        len = strlen(op_str);

        for (i = 0; i < len; i++) {
                c = op_str[i];

                if (c >= 'a' && c <= 'z') {
                        op_str[i] -= 32;
                }
        }
        AUDIT
        pr_info("%s[%s]: start parsing this %s\n", MODNAME, __func__, op_str);
        j = 1;
        for (i = 0; i < len; i++) {
                if (op_str[i] == '|') {
                        op_str[i] = '\0';
                        argv[j++] = op_str + i + 1;
                        format++;
                }
        }
        argv[0] = op_str;
        argv[j] = NULL;
        ret = 0;
        for (i = 0; argv[i] != NULL; i++) {
                len = strlen(argv[i]);
                if (!strncmp(argv[i], __on, len)) {
                        ret |= ON;
                } else if (!strncmp(argv[i], __off, len)) {
                        ret |= OFF;
                } else if (!strncmp(argv[i], __rec_on, len)) {
                        ret |= REC_ON;
                } else if (!strncmp(argv[i], __rec_off, len)) {
                        ret |= REC_OFF;
                } else if (!strncmp(argv[i], __add_path, len)) {
                        ret |= ADD_PATH;
                } else if (!strncmp(argv[i], __remove_path, len)) {
                        ret |= REMOVE_PATH;
                }
                pr_info("%s[%s]: argv[i]=%s -> ret =%d\n", MODNAME, __func__,
                        argv[i], ret);
        }

        return ret;
}

int change_state_vfs_sys_wrapper(char *argv[], int format)
{

        char *pwd = NULL;
        int op = -1;
        int i;
        AUDIT
        pr_info("%s[%s]: got request for %s\n", MODNAME, __func__,
                CHANGE_STATE);

        if (format != 4) {
                return -EINVAL;
        }

        i = 0;
        while (argv[i] != NULL) {
                if (!strncmp(argv[i], PWD, 4)) {
                        pwd = argv[i + 1];
                }
                if (!strncmp(argv[i], OPT, 4)) {
                        op = parse_opt(argv[i + 1]);
                }
                i++;
        }

        if (pwd == NULL) {
                return -EINVAL;
        }

        if (strcmp(pwd, "(null)") == 0) {
                return -EINVAL;
        }

        AUDIT
        pr_info("%s[%s]: ready to call actual syscall sys_change_state with "
                "pwd=****** opt=%d\n",
                MODNAME, __func__, op);
        return do_change_state(pwd, op);
}

int change_path_vfs_sys_wrapper(char *argv[], int format)
{

        char *pwd = NULL;
        char *pathname = NULL;
        int op = -1;
        int i;
        int ret;

        if (format != 6) {
                return -EINVAL;
        }

        pathname = kzalloc(PATH_MAX, GFP_KERNEL);
        if (!pathname) {
                pr_err("%s[%s]: kernel out of memory\n", MODNAME, __func__);
                return -ENOMEM;
        }

        i = 0;
        while (argv[i] != NULL) {
                if (!strncmp(argv[i], PWD, 4)) {
                        pwd = argv[i + 1];
                }
                if (!strncmp(argv[i], PTH, 4)) {
                        if (argv[i + 1] == NULL) {
                                return -EINVAL;
                        }

                        if (strcmp(argv[i + 1], "(null)") == 0) {
                                return -EINVAL;
                        }
                        strncpy(pathname, argv[i + 1], strlen(argv[i + 1]));
                }
                if (!strncmp(argv[i], OPT, 4)) {
                        op = parse_opt(argv[i + 1]);
                }
                i++;
        }
        AUDIT
        pr_info(
            "%s[%s]: calling the sys_change_path with pwd=%s, path=%s, op=%d\n",
            MODNAME, __func__, pwd, pathname, op);

        if (pwd == NULL) {
                kfree(pathname);
                return -EINVAL;
        }

        if (strcmp(pwd, "(null)") == 0) {
                kfree(pathname);
                return -EINVAL;
        }

        ret = do_change_path(pwd, pathname, op);
        kfree(pathname);
        return ret;
}

int change_password_vfs_sys_wrapper(char *argv[], int format)
{
        char *old_pwd = NULL;
        char *new_pwd = NULL;
        int i;

        if (format != 4) {
                return -EINVAL;
        }

        i = 0;
        while (argv[i] != NULL) {
                if (!strncmp(argv[i], OLD_PWD, 4)) {
                        old_pwd = argv[i + 1];
                }
                if (!strncmp(argv[i], NEW_PWD, 4)) {
                        new_pwd = argv[i + 1];
                }
                i++;
        }

        if (old_pwd == NULL) {
                return -EINVAL;
        }

        if (strcmp(old_pwd, "(null)") == 0) {
                return -EINVAL;
        }

        if (new_pwd == NULL) {
                return -EINVAL;
        }

        if (strcmp(new_pwd, "(null)") == 0) {
                return -EINVAL;
        }

        AUDIT
        pr_info("%s[%s]: calling the sys_change_password with old=*******, "
                "new=*******\n",
                MODNAME, __func__);
        return do_change_password(old_pwd, new_pwd);
}

ssize_t syscall_proc_write(struct file *filp, const char *buff, size_t len,
                           loff_t *off)
{

        char *buffer;
        int i;
        int j;
        int ret;
        char *argv[16];
        int format = 0;

        // cngpth -pwd password -opt ADD_PATH -pth pathname
        // cngst -pwd password -opt OFF
        // cngpwd -old old_password -new new_password

        if (len >= (PATH_MAX + NAME_MAX + 1)) {
                pr_warn("%s[%s]: to much element passed", MODNAME, __func__);
                return -1;
        }

        buffer = kzalloc((len + 1), GFP_KERNEL);
        if (!buffer) {
                pr_err("%s[%s]: no mem available\n", MODNAME, __func__);
                return -ENOMEM;
        }

        mutex_lock(&syscall_proc_file_lock);
        ret = copy_from_user(buffer, buff, len);

        if (ret) {
                pr_err("%s[%s]: cannot copy from user\n", MODNAME, __func__);
                kfree(buffer);
                mutex_unlock(&syscall_proc_file_lock);
                return -1;
        }

        buffer[len] = '\0';
        j = 1;

        for (i = 0; i < len; i++) {
                if (buffer[i] == '\n') {
                        buffer[i] = '\0';
                }
        }

        for (i = 0; i < len; i++) {
                if (buffer[i] == ' ') {
                        buffer[i] = '\0';
                        argv[j++] = buffer + i + 1;
                        format++;
                }
        }

        argv[j] = NULL;

        argv[0] = buffer;
        AUDIT
        pr_info("%s[%s]: requested syscall %s checking availability\n", MODNAME,
                __func__, argv[0]);

        ret = check_avilable_sys_list(argv[0]);

        switch (ret) {
        case 0:
                ret = change_state_vfs_sys_wrapper(&argv[1], format);
                break;

        case 1:
                ret = change_path_vfs_sys_wrapper(&argv[1], format);
                break;
        case 2:
                ret = change_password_vfs_sys_wrapper(&argv[1], format);
                break;
        default:
                mutex_unlock(&syscall_proc_file_lock);
                kfree(buffer);
                return -EBADMSG;
        }

        if (ret < 0) {
                mutex_unlock(&syscall_proc_file_lock);
                kfree(buffer);
                return ret;
        }

        AUDIT
        printk("%s[%s]: the administrator requested this %s\n", MODNAME,
               __func__, buffer);
        mutex_unlock(&syscall_proc_file_lock);
        kfree(buffer);
        return (ssize_t)len;
}

ssize_t syscall_proc_read(struct file *filp, char *buff, size_t len,
                          loff_t *off)
{
        char *temp_buffer;
        int to_copy;
        int not_copied;
        int i = 0;
        struct rcu_restrict *entry;
        int written = 0;

        temp_buffer = kmalloc(PATH_MAX, GFP_KERNEL);
        if (!temp_buffer) {
                pr_err("Failed to allocate memory\n");
                return -ENOMEM;
        }

        mutex_lock(&syscall_proc_file_lock);

        if (*off >= PATH_MAX) {
                mutex_unlock(&syscall_proc_file_lock);
                kfree(temp_buffer);
                return 0;
        }

        rcu_read_lock();
        list_for_each_entry_rcu(entry, &restrict_path_list, node)
        {
                if (written < PATH_MAX) {
                        written +=
                            scnprintf(temp_buffer + written, PATH_MAX - written,
                                      "(i=%d, path=%s)\n", i++, entry->path);
                }
        }
        rcu_read_unlock();

        pr_info("vfs: says len %ld, available buffer size %d\n", len, written);

        to_copy = min(len, (size_t)(written - *off));
        not_copied = copy_to_user(buff, temp_buffer + *off, to_copy);

        *off += to_copy - not_copied;

        mutex_unlock(&syscall_proc_file_lock);

        kfree(temp_buffer);
        return to_copy - not_copied;
}

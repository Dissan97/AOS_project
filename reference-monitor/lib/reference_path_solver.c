
#include <asm-generic/errno-base.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "include/reference_path_solver.h"
#include "include/reference_types.h"
#define SWP ".swp"
#define SWPX ".swpx"

struct fs_struct {
        int users;
        spinlock_t lock;
        seqcount_t seq;
        int umask;
        int in_exec;
        struct path root, pwd;
} __randomize_layout;

int fill_absolute_path(char *pathname)
{
        struct path path;
        char *buffer;
        char *temp;
        char *absolute_path_ret;

        if (pathname[0] == '/') {
                return 0;
        }

        buffer = kzalloc(PATH_MAX, GFP_KERNEL);

        if (!buffer) {
                pr_err("%s[%s]: kernel no memory available\n", MODNAME,
                       __func__);
                return -ENOMEM;
        }

        temp = kzalloc((PATH_MAX - NAME_MAX + 2), GFP_KERNEL);
        if (!temp) {
                pr_err("%s[%s]: kernel no memory available\n", MODNAME,
                       __func__);
                kfree(buffer);
                return -ENOMEM;
        }
        strncpy(temp, pathname, (PATH_MAX - NAME_MAX + 1));
        memset(pathname, 0, PATH_MAX);
        path = current->fs->pwd;
        absolute_path_ret = dentry_path_raw(path.dentry, buffer, PATH_MAX);
        snprintf(pathname, PATH_MAX, "%s/%s", absolute_path_ret, temp);
        kfree(buffer);
        kfree(temp);
        return 0;
}

int fill_with_swap_filter(char *pathname)
{
        int ret = 0;
        int len;
        int dot_index = -1;
        int index;
        ret = fill_absolute_path(pathname);
        if (ret) {
                return ret;
        }

        len = strlen(pathname);
        if (len < 6) {
                return ret;
        }

        if ((strncmp(&pathname[len - 4], SWP, 4) == 0) ||
            (strncmp(&pathname[len - 5], SWPX, 5) == 0)) {
                pr_info("%s[%s]: found swap file file=%s\n", MODNAME, __func__,
                        pathname);
                for (index = len - 1; index >= 0; index--) {

                        if (pathname[index] == '/') {
                                if (pathname[index + 1] == '.') {
                                        dot_index = index + 1;
                                }
                                break;
                        }
                }

                if (dot_index != -1) {
                        memmove(&pathname[dot_index], &pathname[dot_index + 1],
                                len - dot_index);
                        len--;
                        pathname[len] = '\0';
                }

                if (strncmp(&pathname[len - 4], SWP, 4) == 0) {
                        memset(&pathname[len - 4], 0, 4);
                } else if (strncmp(&pathname[len - 5], SWPX, 5) == 0) {
                        memset(&pathname[len - 5], 0, 5);
                }
        }

        return ret;
}

int get_filter_swap_or_parent(char *pathname, int *is_parent)
{
        int ret = 0;
        struct path path;
        int len;
        int index;
        ret = fill_with_swap_filter(pathname);

        if (ret) {
                return ret;
        }

        ret = kern_path(pathname, LOOKUP_FOLLOW, &path);
        if (ret) {
                // Get parent directory assuming at least pwd is setup
                len = strlen(pathname);
                for (index = len - 1; index >= 0; index--) {
                        if (pathname[index] == '/') {
                                memset(&pathname[index], 0, len - index);
                                break;
                        }
                }
                ret = kern_path(pathname, LOOKUP_FOLLOW, &path);
                if (ret) {
                        return ret;
                }
                *is_parent = 1;
        }
        path_put(&path);
        return ret;
}

int path_struct(char *pathname, struct path *path, int flag)
{
        return 0;
}

void mark_inode_read_only(struct inode *inode)
{

        inode->i_mode &= ~S_IWUSR;
        inode->i_mode &= ~S_IWGRP;
        inode->i_mode &= ~S_IWOTH;
        inode->i_flags |= S_IMMUTABLE;
        mark_inode_dirty(inode);
}

int mark_indoe_read_only_by_pathname(char *pathname)
{
        struct path path;
        int ret;
        ret = kern_path(pathname, LOOKUP_FOLLOW, &path);
        if (ret) {
                return ret;
        }

        if (!path.dentry) {
                return -ENOENT;
        }

        mark_inode_read_only(path.dentry->d_inode);
        path_put(&path);
        return 0;
}

int fill_path_by_dentry(struct dentry *dentry, char *pathname)
{
        char *buffer;
        char *absolute_path;
        buffer = kzalloc(PATH_MAX, GFP_KERNEL);

        if (!dentry) {
                return -EINVAL;
        }

        if (!buffer) {
                pr_err("%s[%s]: kernel out of memory\n", MODNAME, __func__);
                return -ENOMEM;
        }

        absolute_path = dentry_path_raw(dentry, buffer, PATH_MAX);
        if (IS_ERR(absolute_path)) {
                kfree(buffer);
                return -ENOENT;
        }

        memset(pathname, 0, PATH_MAX);
        strncpy(pathname, absolute_path, strlen(absolute_path));
        kfree(buffer);
        return 0;
}
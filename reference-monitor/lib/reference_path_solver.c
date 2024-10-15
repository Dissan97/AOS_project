
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
#include <linux/fs_struct.h>
#include "include/reference_path_solver.h"
#include "include/reference_types.h"
#define SWP ".swp"
#define SWPX ".swpx"


// devo comunque controllare meglio il path che mi viene ritornato


void get_parent_path(char *pathname)
{


        int len;
        if (!pathname){
                return;
        }
        len = strlen(pathname);

        while (len > 0 && pathname[len] != '/'){
                len--;
        }

        if (len > 0){
                pathname[len] = '\0';
        }

        return;
}

int resolve_relative_path(char *path_str)
{
        char *resolved_path;
        char *cwd;
        struct path pwd;

        if (!path_str){
                return -EINVAL;
        }

        get_fs_pwd(current->fs, &pwd);

        resolved_path = (char *)kmalloc(PATH_MAX, GFP_KERNEL);
        if (!resolved_path) {
        pr_err("%s[%s]: cannot allocate memory for resolve path\n", MODNAME, __func__);
                return -ENOMEM;
        }

        cwd = dentry_path_raw(pwd.dentry, resolved_path, PATH_MAX);

        snprintf(resolved_path, PATH_MAX, "%s/%s", cwd, path_str);
        memset(path_str, 0, PATH_MAX);
        strncpy(path_str, resolved_path, strlen(resolved_path));
        path_put(&pwd);
        return 0;
}

int fill_absolute_path(char *pathname)
{
        struct path path;
        char *buffer;
        char *full_path;
        int err;

        if (!pathname){
                return -EINVAL;
        }

        if (pathname[0] != '/') {
                err = resolve_relative_path(pathname);
                if (err){
                        pr_err("%s[%s]: cannot resolve the path\n", MODNAME, __func__);
                        return err;
                }
        }

        if (kern_path(pathname, LOOKUP_FOLLOW, &path) == 0) {


                buffer = kzalloc(PATH_MAX, GFP_KERNEL);

                if (!buffer) {
                        pr_err("%s[%s]: kernel no memory available\n", MODNAME,
                        __func__);
                        return -ENOMEM;
                }
        
                full_path = dentry_path_raw(path.dentry, buffer, PATH_MAX);
                memset(pathname, 0, strlen(pathname));
                strncpy(pathname, full_path, strlen(full_path));
                path_put(&path);
                kfree(buffer);
                return 0;
        }
       
        return -ENOENT;
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

int mark_inode_read_only_by_pathname(char *pathname)
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
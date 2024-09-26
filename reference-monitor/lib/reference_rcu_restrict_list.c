
#include <linux/slab.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <asm-generic/errno-base.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/dcache.h>
#include <linux/path.h>

#include "include/reference_types.h"
#include "include/reference_rcu_restrict_list.h"
#define RCU_RESTRICT_LIST "rcu-restrict_list"


const char *proc = "/proc";
const char *sys = "/sys";
const char *run = "/run";
const char *var = "/var";
const char *tmp = "/tmp";


int is_path_in(const char *the_path)
{ 
        struct rcu_restrict *p;
        
        if (!the_path){
                return -EINVAL;
        }
        rcu_read_lock();
        list_for_each_entry_rcu(p, &restrict_path_list, node) {
		if(!strcmp(the_path, p->path)) {
			rcu_read_unlock();
			return 0;
		}
	}
	rcu_read_unlock();
        return 1;
}


int restore_black_list_entries (void)
{
        struct rcu_restrict *entry;
        struct path path;
        int ret;
        AUDIT 
        pr_info("%s[%s]: restoring inodes\n", MODNAME, __func__);
        spin_lock(&restrict_path_lock);
        list_for_each_entry_rcu(entry, &restrict_path_list, node) {
		ret = kern_path(entry->path, LOOKUP_FOLLOW, &path);
                if (ret) {
                        continue;
                }
                if (!path.dentry->d_inode){
                        continue;
                }
                
                path.dentry->d_inode->i_mode = entry->o_mode;
                path.dentry->d_inode->i_flags = entry->o_flags;
                path.dentry->d_inode-> i_ctime = current_time(path.dentry->d_inode);
                path.dentry->d_inode-> i_mtime = current_time(path.dentry->d_inode);
                path.dentry->d_inode-> i_atime = current_time(path.dentry->d_inode);
                mark_inode_dirty(path.dentry->d_inode);               
                
        }       
        synchronize_rcu();
	spin_unlock(&restrict_path_lock);
        return ret;
}

int add_path(char *the_path)
{

        struct rcu_restrict *entry;
        struct path path;
        int err;
        long len;
        char *absolute_path;
        char *buf;
        if (!the_path){
                return -EINVAL;
        }

        if (!is_path_in(the_path)){
                return -EEXIST;
        }

        entry = kzalloc(sizeof(struct rcu_restrict), GFP_KERNEL);
        if (!entry){
                pr_err("%s[%s]: cannot alloc memory for struct path\n",MODNAME, RCU_RESTRICT_LIST);
                return -ENOMEM;
        }
        
        err = kern_path(the_path, LOOKUP_FOLLOW, &path);
        
        if (err){
                kfree(entry);
                return err;
        }

        buf = (char *)kzalloc(PATH_MAX, GFP_KERNEL);
        if (!buf) {
                pr_warn("%s[%s]: Failed to allocate memory\n", MODNAME, __func__);
                path_put(&path);
                return -ENOMEM;
        }

        absolute_path = dentry_path_raw(path.dentry, buf, PATH_MAX);
        if (IS_ERR(absolute_path)) {
                pr_warn("%s[%s]: Failed to get absolute path\n", MODNAME, __func__);
                kfree(entry);
                kfree(buf);
                path_put(&path);
                return PTR_ERR(absolute_path);
        }

        len = strlen(absolute_path);
        entry->path = kzalloc(sizeof(char)*(len + 1), GFP_KERNEL);
        
        if (!entry->path){
                pr_warn("%s[%s]: cannot alloc memory to insert new path\n",MODNAME, RCU_RESTRICT_LIST);
                kfree(entry);
                kfree(buf);
                return -ENOMEM;
        }
        strncpy(entry->path, absolute_path, len);
        kfree(buf);
        pr_info("%s[%s]: adding this path: %s entry->path\n", MODNAME, __func__, entry->path);

        if (!path.dentry->d_inode){
                kfree(entry->path);
                kfree(entry);
                return -EEXIST;
        }


        inode_lock_shared(path.dentry->d_inode);
        entry->i_ino = path.dentry->d_inode->i_ino;   
        entry->o_mode = path.dentry->d_inode->i_mode;
        entry->o_flags = path.dentry->d_inode->i_flags;
        inode_unlock_shared(path.dentry->d_inode);

        spin_lock(&restrict_path_lock);
        list_add_rcu(&entry->node, &restrict_path_list);
        spin_unlock(&restrict_path_lock);

        return 0;
}



int del_path(char *the_path)
{
        struct rcu_restrict *entry;
        struct path path;
        long len;
        char *absolute_path;
        char *buf;
        int err;

        if (!the_path){
                return -EINVAL;
        }

        err = kern_path(the_path, LOOKUP_FOLLOW, &path);
        if (err){
                pr_warn("%s[%s]: the path=%s provided not exist", MODNAME, __func__, the_path);
                return -ENOENT;
        }
        buf = (char *)kzalloc(PATH_MAX, GFP_KERNEL);
        if (!buf) {
                pr_warn("%s[%s]: Failed to allocate memory\n", MODNAME, __func__);
                path_put(&path);
                return -ENOMEM;
        }

        absolute_path = dentry_path_raw(path.dentry, buf, PATH_MAX);
        if (IS_ERR(absolute_path)) {
                pr_warn("%s[%s]: Failed to get absolute path\n", MODNAME, __func__);
                kfree(buf);
                path_put(&path);
                return PTR_ERR(absolute_path);
        }


        len = strlen(absolute_path);
        spin_lock(&restrict_path_lock);
        list_for_each_entry(entry, &restrict_path_list, node){
                if (!strncmp(absolute_path, entry->path, len)){
                        
                        if (kern_path(entry->path, LOOKUP_FOLLOW, &path)) {
                                return -ENOENT;
                        }
                        if (!path.dentry->d_inode){
                                return -ENOENT;
                        }       
                        
                        path.dentry->d_inode->i_mode = entry->o_mode;
                        path.dentry->d_inode->i_flags = entry->o_flags;
                        path.dentry->d_inode-> i_ctime = current_time(path.dentry->d_inode);
                        path.dentry->d_inode-> i_mtime = current_time(path.dentry->d_inode);
                        path.dentry->d_inode-> i_atime = current_time(path.dentry->d_inode);
                        mark_inode_dirty(path.dentry->d_inode);
                        
                        list_del_rcu(&entry->node);
                        spin_unlock(&restrict_path_lock);
                        synchronize_rcu();
                        kfree(entry->path);
                        kfree(entry);
                        return 0;
                }
        }
        spin_unlock(&restrict_path_lock);

   return -ENOENT;   
}
int forbitten_path(const char *the_path){
        if (!the_path) {
                return 1;
        }
        if ((strlen(the_path) == 1 && the_path[0] == '/')){
                return 1;
        }
       return (strncmp(the_path, proc, 5) == 0 ||
        strncmp(the_path, sys, 4) == 0 ||
        strncmp(the_path, run, 4) == 0 ||
        strncmp(the_path, var, 4) == 0 ||
        strncmp(the_path, tmp, 4) == 0 ||
        strcmp(single_file_name, the_path) == 0) ? 1 : 0;
}


int is_black_listed(const char *path, unsigned long i_ino, struct black_list_op *i_info)
{

        struct rcu_restrict *entry;
        if (!path){
                return -EINVAL;
        }
        
        rcu_read_lock();
        list_for_each_entry_rcu(entry, &restrict_path_list, node) {
		if(!strcmp(path, entry->path) || entry->i_ino == i_ino) {
                        i_info->mode = entry->o_mode;
                        i_info->flags = entry->o_flags;
			rcu_read_unlock();
			return 0;
		}
                
	}
	rcu_read_unlock();
        return -EEXIST;


}

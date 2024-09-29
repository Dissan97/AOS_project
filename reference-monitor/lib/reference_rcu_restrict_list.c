
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
const char *bin = "/bin";
const char *etc = "/etc";
const char *lib = "/lib";


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

int add_path(char *the_path, struct path path) // here is passed absolute path
{

        struct rcu_restrict *entry;
        int err;
        long len;

        if (!the_path){
                return -EINVAL;
        }

        if (!is_path_in(the_path)){
                return -EEXIST;
        }
        
        if (err){
                pr_warn("%s[%s]: the path provided does not exists\n", MODNAME, __func__);
                return err;
        }

        entry = kzalloc(sizeof(struct rcu_restrict), GFP_KERNEL);
        if (!entry){
                pr_err("%s[%s]: cannot alloc memory for struct path\n",MODNAME, RCU_RESTRICT_LIST);
                return -ENOMEM;
        }


        len = strlen(the_path);
        entry->path = kzalloc(sizeof(char)*(len + 1), GFP_KERNEL);
        
        if (!entry->path){
                pr_warn("%s[%s]: cannot alloc memory to insert new path\n",MODNAME, RCU_RESTRICT_LIST);
                kfree(entry);
                return -ENOMEM;
        }
        strncpy(entry->path, the_path, len);
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
        path_put(&path);

        return 0;
}



int del_path(char *the_path)
{
        struct rcu_restrict *entry;
        long len;
        int err;
        struct path path;
        int ret=0;
        if (!the_path){
                return -EINVAL;
        }

        

        if (err){
                pr_warn("%s[%s]: the path=%s provided not exist", MODNAME, __func__, the_path);
                return -ENOENT;
        }
        	
        len = strlen(the_path);
        spin_lock(&restrict_path_lock);
        list_for_each_entry(entry, &restrict_path_list, node){
                if (!strncmp(the_path, entry->path, len)){
                        
                        if ((ret = kern_path(entry->path, LOOKUP_FOLLOW, &path))) {
                                goto delete_not_exists_kernel_path;
                        }
                        if (!path.dentry->d_inode){
                                path_put(&path);
                                goto delete_not_exists_kernel_path;
                                return -ENOENT;
                        }       
                        
                        path.dentry->d_inode->i_mode = entry->o_mode;
                        path.dentry->d_inode->i_flags = entry->o_flags;
                        path.dentry->d_inode-> i_ctime = current_time(path.dentry->d_inode);
                        path.dentry->d_inode-> i_mtime = current_time(path.dentry->d_inode);
                        path.dentry->d_inode-> i_atime = current_time(path.dentry->d_inode);
                        mark_inode_dirty(path.dentry->d_inode);
                        path_put(&path);
delete_not_exists_kernel_path:                        
                        list_del_rcu(&entry->node);
                        spin_unlock(&restrict_path_lock);
                        synchronize_rcu();
                        kfree(entry->path);
                        kfree(entry);
                        return ret;
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
        strncmp(the_path, bin, 4) == 0 ||
        strncmp(the_path, etc, 4) == 0 ||
        strncmp(the_path, lib, 4) == 0 ||
        strcmp(single_file_name, the_path) == 0) ? 1 : 0;
}


int check_black_list(const char *path)
{

        struct rcu_restrict *entry;
        struct path target_path;
        int err = 1;
        
        if (!path){
                return -EINVAL;
        }
        
        err = kern_path(path, LOOKUP_FOLLOW, &target_path);

        rcu_read_lock();
        list_for_each_entry_rcu(entry, &restrict_path_list, node) {
		if(!strcmp(path, entry->path)) {
			rcu_read_unlock();
                        if (!err){
                                path_put(&target_path);
                        }
			return 0;
		}
                // whatever if the path exists in black list check for hardlinks
                if (!err){
                        if (target_path.dentry){
                                if (entry->i_ino == target_path.dentry->d_inode->i_ino){
                                        rcu_read_unlock();
                                        path_put(&target_path);
                                        return 0;
                                }
                        }
                }

	}
	rcu_read_unlock();
        return -EEXIST;


}

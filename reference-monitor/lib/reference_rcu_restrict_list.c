
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
const char *usr = "/usr";
const char *var = "/var";
const char *bin = "/bin";


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
        return -1;
}


int restore_black_list_entries (void)
{
        struct rcu_restrict *entry;
        struct path dummy_path;
        int ret;
        AUDIT 
        pr_info("%s[%s]: restoring inodes\n", MODNAME, __func__);
        spin_lock(&restrict_path_lock);
        list_for_each_entry_rcu(entry, &restrict_path_list, node) {
		ret = kern_path(entry->path, LOOKUP_FOLLOW, &dummy_path);
                if (ret) {
                        continue;
                }
                if (!dummy_path.dentry->d_inode){
                        continue;
                }
                
                dummy_path.dentry->d_inode->i_mode = entry->o_mode;
                dummy_path.dentry->d_inode->i_flags = entry->o_flags;
                dummy_path.dentry->d_inode-> i_ctime = current_time(dummy_path.dentry->d_inode);
                dummy_path.dentry->d_inode-> i_mtime = current_time(dummy_path.dentry->d_inode);
                dummy_path.dentry->d_inode-> i_atime = current_time(dummy_path.dentry->d_inode);
                mark_inode_dirty(dummy_path.dentry->d_inode);               
                
        }       
        synchronize_rcu();
	spin_unlock(&restrict_path_lock);
        return ret;
}

int add_path(char *the_path)
{

        struct rcu_restrict *entry;
        struct path dummy_path;
        int err;
        long len;

        if (!the_path){
                return -EINVAL;
        }

        if (!is_path_in(the_path)){
                return -ECANCELED;
        }

        entry = kzalloc(sizeof(struct rcu_restrict), GFP_KERNEL);
        if (!entry){
                pr_err("%s[%s]: cannot alloc memory for struct path\n",MODNAME, RCU_RESTRICT_LIST);
                return -ENOMEM;
        }
        
        
        //TODO add dentry search
        len = strlen(the_path);
        entry->path = kzalloc(sizeof(char)*(len + 1), GFP_KERNEL);
        
        if (!entry->path){
                pr_warn("%s[%s]: cannot alloc memory to insert new path\n",MODNAME, RCU_RESTRICT_LIST);
                kfree(entry);
                return -ENOMEM;
        }
        strncpy(entry->path, the_path, len);

        err = kern_path(the_path, LOOKUP_FOLLOW, &dummy_path);
        if (err){
                kfree(entry->path);
                kfree(entry);
                return err;
        }

        if (!dummy_path.dentry->d_inode){
                kfree(entry->path);
                kfree(entry);
                return -ECANCELED;
        }
        inode_lock_shared(dummy_path.dentry->d_inode);
        entry->i_ino = dummy_path.dentry->d_inode->i_ino;   
        entry -> o_mode = dummy_path.dentry->d_inode->i_mode;
        entry -> o_flags = dummy_path.dentry->d_inode->i_flags;
        inode_unlock_shared(dummy_path.dentry->d_inode);

        spin_lock(&restrict_path_lock);
        list_add_rcu(&entry->node, &restrict_path_list);
        spin_unlock(&restrict_path_lock);
        return 0;
}



int del_path(char *the_path)
{
        struct rcu_restrict *entry;
        struct path dummy_path;
        long len;
        if (!the_path){
                return -EINVAL;
        }
        len = strlen(the_path);
        spin_lock(&restrict_path_lock);
        list_for_each_entry(entry, &restrict_path_list, node){
                if (!strncmp(the_path, entry->path, len)){
                        
                        if (kern_path(entry->path, LOOKUP_FOLLOW, &dummy_path)) {
                                return -ENOKEY;
                        }
                        if (!dummy_path.dentry->d_inode){
                                return -ENOKEY;
                        }       
                        
                        dummy_path.dentry->d_inode->i_mode = entry->o_mode;
                        dummy_path.dentry->d_inode->i_flags = entry->o_flags;
                        dummy_path.dentry->d_inode-> i_ctime = current_time(dummy_path.dentry->d_inode);
                        dummy_path.dentry->d_inode-> i_mtime = current_time(dummy_path.dentry->d_inode);
                        dummy_path.dentry->d_inode-> i_atime = current_time(dummy_path.dentry->d_inode);
                        mark_inode_dirty(dummy_path.dentry->d_inode);
                        
                        list_del_rcu(&entry->node);
                        spin_unlock(&restrict_path_lock);
                        synchronize_rcu();
                        kfree(entry->path);
                        kfree(entry);
                        return 0;
                }
        }
        spin_unlock(&restrict_path_lock);

   return -ENOKEY;   
}
int forbitten_path(const char *the_path){
        if (!the_path) {
                return -1;
        }
       return (strncmp(the_path, proc, 5) == 0 ||
        strncmp(the_path, sys, 4) == 0 ||
        strncmp(the_path, run, 4) == 0 ||
        strncmp(the_path, usr, 4) == 0 ||
        strncmp(the_path, var, 4) == 0 ||
        strncmp(the_path, bin, 4) == 0  ||
        strcmp(single_file_name, the_path) == 0) ? -1 : 0;
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

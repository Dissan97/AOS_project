#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/io.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>

#include "include/reference_hooks.h"
#include "include/reference_rcu_restrict_list.h"
#include "include/reference_defer.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dissan Uddin Ahmed <dissanahmed@gmail.com>");

//#define REFERENCE_HOOK "reference-hook"
struct path dummy_path;
struct dentry dummy_dentry;

#define FORBITTEN_MASK (O_RDWR | O_WRONLY | O_TRUNC | O_APPEND)
#define COMMON_MASK 0x8000 
int vfs_open_wrapper(struct kprobe *ri, struct pt_regs * regs)
{
        struct path* path = (struct path*) (regs->di);
        struct dentry *dentry;
		struct dentry *d_parent;
        struct file *filp = (struct file *)regs->si;
		struct black_list_op i_info;
		unsigned int f_flags;
		char dir[MAX_FILE_NAME];
		char *p;
		unsigned long i_ino = 0;

		if (likely(filp)){
			f_flags = (filp->f_flags ^ COMMON_MASK);
			if (!(f_flags & FORBITTEN_MASK))
			/*if (!(filp->f_flags & FORBITTEN_MASK)){
				return 0;
			}*/
				return 0;
		}
		
        if (likely(path)){
                if (path->dentry){
					dentry = path->dentry;
				   	if (dentry->d_inode){
						i_ino = dentry->d_inode->i_ino;
					}			  
			   		p=dentry_path_raw(dentry, dir, MAX_FILE_NAME);
					if (forbitten_path(p) < 0){
						return 0;
				   }
		
					if (p){
						//pr_info("Dissan dice: %s\n", p);

						if (!is_black_listed(p, i_ino, &i_info)){
							atomic_inc((atomic_t*)&audit_counter);
							//inode = dentry -> d_inode;
							AUDIT
							pr_info("%s[%s]: somebody try to access the file in write mode\n",MODNAME, __func__);
							pr_info("F_FLAGS: %u\n", f_flags);
							if (filp != NULL){
								if (filp->f_flags & (O_RDWR | O_WRONLY | O_TRUNC | O_APPEND) ){
									pr_err("%s[%s]: cannot open this file=%s in write|append|trunc\n",
									MODNAME, __func__, dentry->d_name.name);
									dentry->d_inode->i_mode = i_info.mode &= RDONLY_MAKS;
									dentry->d_inode->i_flags = i_info.flags |= S_IMMUTABLE;
									goto store_thread_info_vfs_open;
								}
							}
						}
						// if is a directory then
						d_parent = dentry->d_parent;
						if (!d_parent) return 0;
						

						memset(dir, 0, MAX_FILE_NAME);
						p = dentry_path_raw(d_parent, dir, MAX_FILE_NAME);

						if (!is_black_listed(p, 0, &i_info)){
							atomic_inc((atomic_t*)&audit_counter);
							if (filp != NULL){
								if (filp->f_flags & (O_CREAT | O_TRUNC | O_APPEND)){
									pr_err("%s[%s]: file cannot be created on this directory=%s cannot be modified\n",
									MODNAME, __func__, d_parent->d_name.name);
									dentry->d_inode->i_mode = i_info.mode &= RDONLY_MAKS;
									dentry->d_inode->i_flags = i_info.flags |= S_IMMUTABLE;
									dentry->d_inode-> i_ctime = current_time(dentry->d_inode);
									dentry->d_inode-> i_mtime = current_time(dentry->d_inode);
									dentry->d_inode-> i_atime = current_time(dentry->d_inode);
									goto store_thread_info_vfs_open;	
								}
							}
						}	
					}	          	

        	}

	}
	return 0;

	store_thread_info_vfs_open:
	AUDIT
	pr_info("%s[%s]: calling work queue", MODNAME, __func__);
	defer_top_half(defer_bottom_half);
	return 0;
}

//todo change unlink

int vfs_mkrmdir_unlink_wrapper(struct kprobe *ri, struct pt_regs * regs)
{
    //struct inode *d = (struct inode *)regs->di;
	struct dentry *dentry = (struct dentry *)regs->si;
	struct dentry *d_parent;
	struct black_list_op i_info;
	char dir[MAX_FILE_NAME];
	char *p;
	if (likely(dentry)){
		
		d_parent = dentry->d_parent;
		if (d_parent){
			p = dentry_path_raw(d_parent, dir, MAX_FILE_NAME);
			if (!is_black_listed(p, 0, &i_info)){
				atomic_inc((atomic_t*)&audit_counter);
				pr_err("%s[%s]: cannot modify directory in this for directory %s\n", MODNAME, __func__, d_parent->d_name.name);
				regs->si = (unsigned long)(NULL);
				regs->di = (unsigned long)(NULL);	
			}
		}
	}
	
	return 0;
}



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
#include <linux/err.h>
#include <linux/namei.h>
#include <linux/dcache.h>
#include <linux/path.h>
#include "include/reference_hooks.h"
#include "include/reference_rcu_restrict_list.h"
#include "include/reference_defer.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dissan Uddin Ahmed <dissanahmed@gmail.com>");

//#define REFERENCE_HOOK "reference-hook"
struct path dummy_path;
struct dentry dummy_dentry;

#define FORBITTEN_MASK (O_CREAT | O_RDWR | O_WRONLY | O_TRUNC | O_APPEND)
#define COMMON_MASK 0x8000 

char *get_full_user_path(int dfd, const __user char *user_path) {
    struct path path_struct;
    char *dummy_path;
    char *full_path;
    int error = -EINVAL;
    unsigned int lookup_flags = LOOKUP_FOLLOW;

    dummy_path = kmalloc(PATH_MAX, GFP_KERNEL);
    if (dummy_path == NULL) return NULL;

    error = user_path_at(dfd, user_path, lookup_flags, &path_struct);
    if (error) {
        kfree(dummy_path);
        return NULL;
    }

    full_path = d_path(&path_struct, dummy_path, PATH_MAX);
    path_put(&path_struct);
    kfree(dummy_path);
    return full_path;
}

struct open_flags {
	int open_flag;
	umode_t mode;
	int acc_mode;
	int intent;
	int lookup_flags;
};

void mark_inode_read_only(struct inode *inode)
{
    
    inode->i_mode &= ~S_IWUSR;
    inode->i_mode &= ~S_IWGRP;
    inode->i_mode &= ~S_IWOTH;
    inode->i_flags |= S_IMMUTABLE;
}


int pre_do_filp_open_handler(struct kprobe *ri, struct pt_regs * regs)
{

#if defined(__x86_64__)
    int dfd = (int)(regs->di); // arg0
    const __user char *user_path = ((struct filename *)(regs->si))->uptr; // arg1
    const char *real_path = ((struct filename *)(regs->si))->name;
    struct open_flags *op_flag = (struct open_flags *)(regs->dx); // arg2

#elif defined(__aarch64__)
    int dfd = (int)(regs->regs[0]); // arg0 (dfd)
    const __user char *user_path = ((struct filename *)(regs->regs[1]))->uptr; // arg1 
    const char *real_path = ((struct filename *)(regs->regs[1]))->name;
    struct open_flags *op_flag = (struct open_flags *)(regs->regs[2]); // arg2 (open_flags)

#elif defined(__i386__)
    // x86 32-bit
    int dfd = (int)(regs->bx); // arg0 (dfd)
    const __user char *user_path = ((struct filename *)(regs->cx))->uptr; // arg1 
    const char *real_path = ((struct filename *)(regs->cx))->name;
    struct open_flags *op_flag = (struct open_flags *)(regs->dx); // arg2 (open_flags)

#elif defined(__arm__)
    // ARM 32-bit
    int dfd = (int)(regs->ARM_r0); // arg0 (dfd)
    const __user char *user_path = ((struct filename *)(regs->ARM_r1))->uptr; // arg1 
    const char *real_path = ((struct filename *)(regs->ARM_r1))->name;
    struct open_flags *op_flag = (struct open_flags *)(regs->ARM_r2); // arg2 (open_flags)
#endif

    char *full_path;
	int flags = op_flag -> open_flag;
	int is_allowed;
	struct path path;
	
	struct black_list_op i_info;
	int is_dir = 0;
	char *last_slash;
	unsigned long old_len;

	
	// they are just opening in read mode no problem here
	if (!(flags & FORBITTEN_MASK)){
		return 0;
	}

	if (user_path == NULL) {
        full_path = (char *)real_path;
    } else {
        full_path = get_full_user_path(dfd, user_path);
        if (full_path == NULL) {
            full_path = (char *)real_path;
        }
    }
	
	if (forbitten_path(full_path)){
		return 0;
	}

	if (kern_path(full_path, LOOKUP_FOLLOW, &path)){
		//path does not exist
		last_slash = strrchr(full_path, '/');
		if (!last_slash){
			// nor the father exists
			return 0;
		}
		old_len = strlen(full_path);
		*last_slash = '\0';
		memset(last_slash, 0, old_len - strlen(full_path));
		if (kern_path(full_path, LOOKUP_FOLLOW, &path)){
			//pr_warn("%s[%s]: unable to find struct path for %s\n", MODNAME, __func__, full_path);
			return 0;
		}

		is_dir = 1;
	}

	//pr_warn("dissan: stiamo dopo is_dir %d\n", is_dir);

	//additional check avoid null pointer

	if (likely(!path.dentry)){
		//dentry is null
		return 0;
	}

	is_allowed = is_black_listed(full_path, path.dentry->d_inode->i_ino, &i_info);

	if (!is_allowed){
		// the path is not black listed
		pr_err("dissan: is allowed full %s\n", full_path);
		if (is_dir){
			pr_err("%s[%s]: cannot create file in this protected directory %s\n",MODNAME, __func__, full_path);	
		}else {
			pr_err("%s[%s]: the file %s cannot be modified\n",MODNAME, __func__, full_path);
		}
		mark_inode_read_only(path.dentry->d_inode);	
		goto do_filp_open_defer_work;
	}

	return 0;


do_filp_open_defer_work:
	AUDIT
	pr_info("%s[%s]: calling work queue", MODNAME, __func__);
	defer_top_half(defer_bottom_half);
	//setting up bad file descriptor to make fail do_filp_open
#if defined(__x86_64__)
    regs->di = -1;
#elif defined(__aarch64__)
    regs->regs[0] = -1; 

#elif defined(__i386__)
    regs->bx = -1;  

#elif defined(__arm__)
    regs->ARM_r0 = -1;  
#endif
	return 0;
}


//todo change unlink

int pre_vfs_mk_rmdir_and_unlink_handler(struct kprobe *ri, struct pt_regs * regs)
{
#if defined(__x86_64__)
    //struct inode *dir = (struct inode *)regs->di;
	struct dentry *dentry = (struct dentry *)regs->si;
#elif defined(__aarch64__)
    struct dentry *dentry = (struct dentry *)regs->regs[1];  // AArch64: Second argument is in `x1` (regs[1])

#elif defined(__i386__)
    struct dentry *dentry = (struct dentry *)regs->cx;  // x86 (32-bit): Second argument is in `cx`

#elif defined(__arm__)
    struct dentry *dentry = (struct dentry *)regs->ARM_r1;  // ARM (32-bit): Second argument is in `r1` (ARM_r1)
#endif
	struct dentry *parent_dentry;
	struct black_list_op i_info;
	char *parent_full_path;
	char *dummy;


	// check the dentry if 
	if (likely(dentry)){
		
		parent_dentry = dentry->d_parent;
		// 
		if (parent_dentry){

			dummy = kmalloc(MAX_FILE_NAME, GFP_KERNEL);

			if (!dummy){
				pr_err("%s[%s]: Cannot allocate dummy with kmalloc\n", MODNAME, __func__);
				return -ENOMEM;
			}

			parent_full_path = dentry_path_raw(parent_dentry, dummy, MAX_FILE_NAME);
			if (!is_black_listed(parent_full_path, 0, &i_info)){
				atomic_inc((atomic_t*)&audit_counter);
				pr_err("%s[%s]: cannot cannot (CREATE | REMOVE) files in this for directory %s\n", MODNAME, __func__, parent_dentry->d_name.name);
				mark_inode_read_only(parent_dentry->d_inode);
				goto store_thread_info_mkrmdir_unlink;
			}
		}
	}
	
	return 0;

store_thread_info_mkrmdir_unlink:
	AUDIT
	pr_info("%s[%s]: calling work queue", MODNAME, __func__);
	defer_top_half(defer_bottom_half);
	return 0;
}

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
#include "include/reference_path_solver.h"

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
    //kfree(dummy_path);
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
	mark_inode_dirty(inode);
}

struct fs_struct {
	int users;
	spinlock_t lock;
	seqcount_t seq;
	int umask;
	int in_exec;
	struct path root, pwd;
} __randomize_layout;


int absolute_path_from_pwd(char *filename){
	struct path path;
	char *buffer;
	char temp[NAME_MAX];
	char *abs;

	buffer = kmalloc(PATH_MAX, GFP_KERNEL);

	if (!buffer){
		pr_err("%s[%s]: kernel non mem\n", MODNAME, __func__);
		return -ENOMEM;
	}	

	strncpy(temp, filename, NAME_MAX);
	memset(filename, 0, PATH_MAX);
	path = current->fs->pwd;
	abs = dentry_path_raw(path.dentry, buffer, PATH_MAX);
	snprintf(filename, PATH_MAX, "%s/%s", abs, temp);
	kfree(buffer);
	return 0;
}

int pre_do_filp_open_handler(struct kprobe *ri, struct pt_regs * regs)
{
    struct filename *pathname = (struct filename *)regs->si; 
	struct open_flags *op_flag = (struct open_flags *)(regs->dx); // arg2
    const char *filename = pathname->name;
    struct path path;
    char *buffer;
    char *abs_path;
    int ret;
	int flags = op_flag -> open_flag;
	int is_parent = 0;


	if (filename == NULL){
		return 0;
	}

	// for routine execution avoid to much overhead
	if (forbitten_path(filename)){
		return 0;
	}
	
    buffer = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!buffer) {
        pr_err("%s[%s]: kmalloc error\n", MODNAME, __func__);
		goto do_filp_open_pre_handler_mem_problem;
    }

	if (pathname->uptr == NULL){
		pr_err("DISSAN DICE: filename null\n");
		kfree(buffer);
		return 0;
	}
	if (copy_from_user(buffer, pathname->uptr, PATH_MAX)){
		pr_err("dissan dice fallito\n");
		kfree(buffer);
		return 0;
	}
	ret = get_filter_swap_or_parent(buffer, &is_parent);
	if (ret){
		kfree(buffer);
		if (ret == -ENOMEM){
			goto do_filp_open_pre_handler_mem_problem;
		}
		return 0;
	}

	pr_info("dissan dice: path=%s, aperto in write=%d, creat%d, append=%d, trunc=%d, readwrite=%d is_parent=%d\n", 
	buffer, (flags & O_WRONLY) != 0, (flags & O_CREAT) != 0, (flags & O_APPEND) != 0, (flags & O_TRUNC) != 0,
	(flags & O_RDWR) != 0, is_parent);

	return 0;
	ret = check_black_list(buffer);

    


    // Libera il buffer
    kfree(buffer);
    return 0;

do_filp_open_defer_work:
	AUDIT
	pr_info("%s[%s]: calling work queue", MODNAME, __func__);
	defer_top_half(defer_bottom_half);
	//setting up bad file descriptor to make fail do_filp_open
do_filp_open_pre_handler_mem_problem:
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

//todo check unlink

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
	char *parent_full_path;
	char *dummy;
	//todo pay attention to avoid kernel crash
	return 0;
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

			if (parent_full_path == NULL){
				return 0;
			}

			if (forbitten_path(parent_full_path)){
				return 0;
			}
			if (check_black_list(parent_full_path) == EEXIST){
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

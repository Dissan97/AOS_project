#define EXPORT_SYMTAB
#include <asm/apic.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/dcache.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "include/reference_defer.h"
#include "include/reference_hooks.h"
#include "include/reference_path_solver.h"
#include "include/reference_rcu_restrict_list.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dissan Uddin Ahmed <dissanahmed@gmail.com>");

//#define REFERENCE_HOOK "reference-hook"
struct path dummy_path;
struct dentry dummy_dentry;

#define FORBITTEN_MASK (O_CREAT | O_RDWR | O_WRONLY | O_TRUNC | O_APPEND)
#define COMMON_MASK 0x8000



struct open_flags {
        int open_flag;
        umode_t mode;
        int acc_mode;
        int intent;
        int lookup_flags;
};



/// @brief 
/// @param ri 
/// @param regs 
/// @return 
int pre_do_filp_open_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
#if defined(__x86_64__)
    struct filename *pathname = (struct filename *)regs->si; 
    struct open_flags *op_flag = (struct open_flags *)(regs->dx); 
#elif defined(__aarch64__)
    struct filename *pathname = (struct filename *)regs->regs[1];
    struct open_flags *op_flag = (struct open_flags *)(regs->regs[2]);

#elif defined(__i386__)
    struct filename *pathname = (struct filename *)regs->cx;
    struct open_flags *op_flag = (struct open_flags *)(regs->dx);

#elif defined(__arm__)
    struct filename *pathname = (struct filename *)regs->r1;
    struct open_flags *op_flag = (struct open_flags *)(regs->r2);

#endif

        const char *filename = pathname->name;
        int ret;
        int flags = op_flag->open_flag;
        char *buffer;
        int is_dir = 0;
        int is_swap =0;
        struct hook_return *data;
        data = (struct hook_return *)ri->data;
        data->hook_error = 0;
        if (filename == NULL) {
                return 0;
        }
        
        if (forbitten_path(filename)) {
                return 0;
        }

        if (pathname->uptr == NULL) {
                return 0;
        }

        buffer = kzalloc(PATH_MAX, GFP_KERNEL);
        if (!buffer) {
                pr_err("%s[%s]: kmalloc error\n", MODNAME, __func__);
                data->hook_error = -ENOMEM;
                return 0;
        }

        if (copy_from_user(buffer, pathname->uptr, PATH_MAX) < 0) {
                
                memset(buffer, 0, PATH_MAX);
                if (filename){
                        ret = strlen(filename);
                        strncpy(buffer, filename, ret);
                        buffer[ret] = '\0';
                }else{
                        return 0;
                }
                
        }

        ret = fill_with_swap_filter(buffer);
      
        if (ret < 0) {
                
                if (ret == -ENOMEM) {
                        kfree(buffer);
                        data->hook_error = -ENOMEM;
                        return 0;
                }

                if (ret == -ENOENT){
                        is_dir = 1;
                        
                        get_parent_path(buffer);
                        ret = fill_absolute_path(buffer);
                        
                        if (ret){
                                if (ret == -ENOMEM){
                                        data->hook_error = -ENOMEM;
                                }
                                kfree(buffer);
                                return 0;
                        }
                }
        } else if (ret == 1) {
                is_swap = 1;
        }

        ret = check_black_list(buffer, 1);

        if (!ret) {

                pr_warn("%s[%s]: found black for path=%s\n", MODNAME, __func__,
                buffer);
              
                if ((is_dir) && (flags & O_CREAT)){
                        pr_err("%s[%s]: this_dir=%s cannot be modified\n", MODNAME, __func__, buffer);
                        if (mark_inode_read_only_by_pathname(buffer)) {
                                                pr_err("%s[%s]: unable to find inode for this pathname=%s\n",
                                                MODNAME, __func__, buffer);
                                                kfree(buffer);
                                                return 0;
                        }
                        goto do_filp_open_defer_work;
                } else {
                        if (is_swap) {
                                pr_err("%s[%s]: cannot use swap file to bypass file=%s\n", MODNAME, __func__, buffer);
                                goto do_filp_open_defer_work;
                         } else {

                                if (flags & (O_RDWR | O_WRONLY | O_TRUNC | O_APPEND)){
                                        if (mark_inode_read_only_by_pathname(buffer)) {
                                                pr_err("%s[%s]: unable to find inode for this pathname=%s\n",
                                                MODNAME, __func__, buffer);
                                                kfree(buffer);
                                                return 0;
                                        }
                                        pr_err("%s[%s]: the file =%s cannot be modified\n", MODNAME, __func__, buffer);
                                        goto do_filp_open_defer_work;

                                }
                          }
                }
                
        }
        kfree(buffer);
        return 0;

do_filp_open_defer_work:    
        
        AUDIT
        pr_info("%s[%s]: calling work queue", MODNAME, __func__);
        defer_top_half(defer_bottom_half);
#if defined(__x86_64__)
                regs->di = -1; 
#elif defined(__aarch64__)
                regs->regs[0] = -1;
#elif defined(__i386__)
                regs->bx = -1;
#elif defined(__arm__)
                regs->r0 = -1;
#endif  
        data->hook_error = -EACCES;
        
        return 0;
}



int pre_vfs_mk_rmdir_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
#if defined(__x86_64__)
        
        struct dentry *dentry = (struct dentry *)regs->si;
#elif defined(__aarch64__)
        struct dentry *dentry = (struct dentry *)regs->regs[1]; 

#elif defined(__i386__)
        struct dentry *dentry =(struct dentry *)regs->cx; 

#elif defined(__arm__)
        struct dentry *dentry =(struct dentry *)regs->r1; 
#endif
        char *buffer;
        int ret;
        struct hook_return *data;
        data = (struct hook_return *)ri->data;
        data->hook_error = 0;

        buffer = kzalloc(PATH_MAX, GFP_KERNEL);
        if (!buffer) {
                pr_err("%s[%s]: kernel out of memory\n", MODNAME, __func__);
                return -ENOMEM;
        }
        
        if (likely(dentry)) {
                if (dentry->d_parent) {
                        ret = fill_path_by_dentry(dentry->d_parent, buffer);
                        if (!check_black_list(buffer, 0)) {
                                pr_err(
                                    "%s[%s]: cannot modify this directory=%s\n",
                                    MODNAME, __func__, buffer);   
                                mark_inode_read_only(dentry->d_parent->d_inode);
                                data->hook_error = -EACCES;
                                goto store_thread_info_mkrmdir_unlink;
                        }
                }
        }
        kfree(buffer);
        return 0;

store_thread_info_mkrmdir_unlink:
        AUDIT
        pr_info("%s[%s]: calling work queue", MODNAME, __func__);
        defer_top_half(defer_bottom_half);
        kfree(buffer);
        return 0;
}

int pre_vfs_unlink_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
#if defined(__x86_64__)    
        struct dentry *dentry = (struct dentry *)regs->si;
#elif defined(__aarch64__)
        struct dentry *dentry =(struct dentry *)regs->regs[1]; 

#elif defined(__i386__)
        struct dentry *dentry =(struct dentry *)regs->cx; 

#elif defined(__arm__)
        struct dentry *dentry =(struct dentry *)regs->r1; 
#endif
        char *buffer;
        int ret;
        struct hook_return *data;
        data = (struct hook_return *)ri->data;
        data->hook_error = 0;
        buffer = kzalloc(PATH_MAX, GFP_KERNEL);
        if (!buffer) {
                pr_err("%s[%s]: kernel out of memory\n", MODNAME, __func__);
                return -ENOMEM;
        }
        
        if (likely(dentry)) {

                ret = fill_path_by_dentry(dentry, buffer);
                if (!check_black_list(buffer, 0)) {
                        pr_err("%s[%s]: cannot remove this file=%s\n",
                                MODNAME, __func__, buffer);
                        
                        dentry->d_inode->i_flags |= S_IMMUTABLE;
                        mark_inode_read_only(dentry->d_inode);
                        data->hook_error = -EACCES;
                        goto store_thread_info_mkrmdir_unlink;
                }
                memset(buffer, 0, PATH_MAX);
                if (dentry->d_parent) {
                        ret = fill_path_by_dentry(dentry->d_parent, buffer);
                        if (!check_black_list(buffer, 0)) {
                                pr_err("%s[%s]: cannot modify this directory=%s\n",
                                    MODNAME, __func__, buffer);
                                dentry->d_parent->d_inode->i_flags |= S_IMMUTABLE;    
                                mark_inode_read_only(dentry->d_parent->d_inode);
                                mark_inode_dirty(dentry->d_parent->d_inode);
                                data->hook_error = -EACCES;
                                goto store_thread_info_mkrmdir_unlink;
                        }
                }
        }
        kfree(buffer);
        return 0;

store_thread_info_mkrmdir_unlink:
        AUDIT
        pr_info("%s[%s]: calling work queue", MODNAME, __func__);
        defer_top_half(defer_bottom_half);
        kfree(buffer);
        return 0;
}


int reference_kret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
       
        struct hook_return *data = (struct hook_return *)ri->data;
        if (data->hook_error){
#if defined(__x86_64__)
        regs->ax = data->hook_error;
#elif defined(__aarch64__)
        regs->regs[0] = data->hook_error;
#elif defined(__i386__)
        regs->ax = data->hook_error; 
#elif defined(__arm__)
        regs->uregs[0] = data->hook_error;
#endif
        }

    
    return 0;
}

int post_dir_handler(struct kretprobe_instance *ri, struct pt_regs *regs){
        struct hook_return *data = (struct hook_return *)ri->data;
#if defined(__x86_64__)
        regs->ax = data->hook_error;
#elif defined(__aarch64__)
        regs->regs[0] = data->hook_error;
#elif defined(__i386__)
        regs->ax = data->hook_error; 
#elif defined(__arm__)
        regs->uregs[0] = data->hook_error;
#endif
        
        return 0;
}


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>	
#include <linux/pid.h>		/* For pid types */
#include <linux/tty.h>		/* For the tty declarations */
#include <linux/version.h>	/* For LINUX_VERSION_CODE */
#include <linux/signal_types.h>
#include <linux/syscalls.h>
#include <linux/sched/mm.h>
#include <linux/errno.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/mm.h>
#include <linux/workqueue.h>

#include "include/reference_defer.h"
#include "include/reference_types.h"


#define BUFF_SIZE (PAGE_SIZE << 2)


void defer_bottom_half(struct work_struct *work)
{

    struct work_data *data = container_of(work, struct work_data, work);
    struct file *file;
    struct file *append_file;
    mm_segment_t old_fs;
    loff_t pos = 0;
    int ret;
    char *file_contents = NULL;
    char *buffer;
    char *ciphertext = NULL;
    pr_info("%s: work started\n", DEFER_WORK_QUEUE);
    
    ciphertext = kmalloc(SHA512_LENGTH * 2 + 1, GFP_KERNEL);
    if (!ciphertext) {
        pr_err("%s[%s]: Failed to allocate memory for ciphertext\n", MODNAME, __func__);
        goto bottom_half_out;
    }

    // Open the executable file
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    file = filp_open(data->path_name, O_RDONLY, 0);
    set_fs(old_fs);

    if (IS_ERR(file)) {
        pr_err("%s[%s]: defer_bottom: Failed to open file: %s\n", MODNAME, __func__, data->path_name);
        goto err_ct_allocated;
    }

    // Allocate memory for file contents
    file_contents = kmalloc(file->f_inode->i_size + 1, GFP_KERNEL);
    if (!file_contents) {
        pr_err("%s[%s]: Failed to allocate memory for file contents\n", MODNAME, __func__);
        goto err_filp_opened;
    }

    ret = kernel_read(file, file_contents, file->f_inode->i_size, &pos);
    if (ret < 0) {
        pr_err("%s[%s]: Failed to read file: %s\n", MODNAME, __func__, data->path_name);
        goto err_file_contents_allocated;
    }

    file_contents[file->f_inode->i_size] = '\0';    

    ret = hash_plaintext(salt, ciphertext, file_contents);
    if (ret < 0) {
        pr_err("%s[%s]: Failed to hash file contents\n", MODNAME, __func__);
        goto bottom_half_out;
    }

    buffer = vmalloc(PAGE_SIZE << 1);
    if (!buffer){
        goto err_file_contents_allocated;
    }
    
    sprintf(buffer, "%d,%d,%d,%d,%s,%s\n",
            data->tgid, data->tid, data->uid, data->euid, data->path_name, ciphertext);

    append_file = filp_open(single_file_name, O_WRONLY, 0);
    if (IS_ERR(append_file)){
        pr_err("%s[%s]: cannot open single file=%s\n", MODNAME, __func__, single_file_name);
        goto err_buffer_allocated_for_write;
    }
        ret = kernel_write(append_file, buffer, strlen(buffer), &pos);
        if (ret < 0){
            pr_err("%s[%s]: ret=%d - some issue in writing to the file=%s this content=%s\n", MODNAME, __func__, ret, single_file_name, buffer);
            goto err_buffer_allocated_for_write;
        }
        AUDIT
    	pr_info("%s[%s]: File written\n", MODNAME, __func__);

err_buffer_allocated_for_write:
    vfree(buffer);
err_file_contents_allocated:
    kfree(file_contents);
err_filp_opened:
    filp_close(file, NULL);
err_ct_allocated:
    kfree(ciphertext);
bottom_half_out:
    kfree(data->path_name);
    kfree(data);
    
    

}

void defer_top_half(void (*work_func)(struct work_struct *))
{
    
    struct work_data *data;
    struct mm_struct *mm;
    struct file *exe_file;
    char *path_name;
    int path_len;
    char dir[MAX_FILE_NAME];

    // Allocate work data
    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data){
        pr_err("%s[%s]: Failed to allocate memory for data\n", MODNAME, __func__);
        return;
    }
        

    // Get current process credentials
    data->tgid = current->tgid;
    data->tid = current->pid;
    data->uid = current_uid().val;
    data->euid = current_euid().val;

    // Get the executable path
    mm = get_task_mm(current);
    if (!mm) {
        kfree(data);
        return;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
    mmap_read_lock(mm);
#else
    down_read(&mm->mmap_sem);
#endif
    exe_file = mm->exe_file;
    if (exe_file) {
        get_file(exe_file);
        path_get(&exe_file->f_path);
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
    mmap_read_unlock(mm);
#else
    up_read(&mm->mmap_sem);
#endif
    
    mmput(mm);

    if (!exe_file) {
        kfree(data);
        return;
    }

    path_name = dentry_path_raw(exe_file->f_path.dentry, dir, MAX_FILE_NAME);
    if (!path_name){
        kfree(data);
        return;
    }
    path_len = strnlen(path_name, MAX_FILE_NAME);
    data->path_name = kzalloc(path_len + 1,GFP_KERNEL);
    if (!data->path_name){
        kfree(data);
        return;
    }
    strncpy(data->path_name, path_name, path_len);

    INIT_WORK(&data->work, work_func);
    queue_work(deferred_queue, &data->work);

    
    return;
}

ssize_t append_defer_wrt(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    return 0;
}
ssize_t append_defer_rd(struct file *filp, char *buff, size_t len, loff_t *off)
{
    return 0;
}
int append_defer_opn(struct inode* i_node, struct file* f)
{
    return 0;
}
int append_defer_rls(struct inode* i_node, struct file* f)
{
    return 0;
}
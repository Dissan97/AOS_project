#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rwlock.h> // Include the rwlock header
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <linux/version.h>

#include "singlefilefs.h"

DEFINE_MUTEX(lock_log);

static int onefilefs_open(struct inode *inode, struct file *filp)
{
    bool trunc = (filp->f_flags & O_TRUNC);

    if (trunc) {
        printk("%s: truncation requested but ignored\n", MOD_NAME);
        return -EINVAL;
    }

    return 0;
}

static int onefilefs_release(struct inode *inode, struct file *filp)
{
    pr_info("%s: file closed: %s\n", MOD_NAME, filp->f_path.dentry->d_name.name);
    return 0;
}

ssize_t onefilefs_read(struct file *filp, char __user *buf, size_t len,
                       loff_t *off)
{
    loff_t offset;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = filp->f_inode;
    uint64_t file_size;
    int ret;
    int block_to_read;

    mutex_lock(&lock_log);
    file_size = the_inode->i_size;
    if (file_size == 0) {
        mutex_unlock(&lock_log);
        return 0;
    }

    if (*off >= file_size) {
        pr_warn("%s: cannot go over file_size\n", MOD_NAME);
        mutex_unlock(&lock_log);
        return 0;
    }

    if (*off + len > file_size) {
        len = file_size - *off;
    }

    offset = *off % DEFAULT_BLOCK_SIZE;

    if (offset + len > DEFAULT_BLOCK_SIZE) {
        len = DEFAULT_BLOCK_SIZE - offset;
    }

    block_to_read = *off / DEFAULT_BLOCK_SIZE + 2;

    bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
    if (!bh) {
        mutex_unlock(&lock_log);
        return -EIO;
    }

    ret = copy_to_user(buf, bh->b_data + offset, len);
    if (ret != 0) {
        brelse(bh);
        mutex_unlock(&lock_log);
        return -EFAULT;
    }

    *off += len;

    brelse(bh);
    mutex_unlock(&lock_log);

    return len;
}

ssize_t onefilefs_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    struct inode *inode = filp->f_inode;
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    size_t to_write = len;
    size_t written = 0;
    int block_number;
    int offset;
    int ret;
    size_t write_len;
    ssize_t res = 0;
    loff_t pos;
    loff_t pos_max;

    mutex_lock(&lock_log);
    inode_lock(inode);
    pos = i_size_read(inode);
    pr_info("%s: write called current pos=%lld\n", MOD_NAME, pos);

    if ((pos + len) >= max_file_size) {
        pr_warn("%s: max dimension of file reached %ld\n", MOD_NAME, max_file_size);
        res = -EFBIG;
        goto out_unlock_inode;
    }

    while (to_write > 0) {
        block_number = pos / DEFAULT_BLOCK_SIZE + 2;
        offset = pos % DEFAULT_BLOCK_SIZE;

        bh = sb_bread(sb, block_number);
        if (!bh) {
            res = -EIO;
            goto out_unlock_inode;
        }

        write_len = min(to_write, (size_t)(DEFAULT_BLOCK_SIZE - offset));
        ret = copy_from_user(bh->b_data + offset, buf + written, write_len);
        if (ret != 0) {
            brelse(bh);
            res = -EFAULT;
            goto out_unlock_inode;
        }

        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);
        pos += write_len;
        to_write -= write_len;
        written += write_len;
    }

    pos_max = max(pos, i_size_read(inode));
    i_size_write(inode, pos_max);
    mark_inode_dirty(inode);
    sync_blockdev(sb->s_bdev);
    *off = pos;

    res = written;

out_unlock_inode:
    inode_unlock(inode);
    mutex_unlock(&lock_log);
    return res;
}


static loff_t onefilefs_llseek(struct file *filp, loff_t offset, int whence)
{
    return filp->f_pos;
}

struct dentry *onefilefs_lookup(struct inode *parent_inode,
                                struct dentry *child_dentry, unsigned int flags)
{
    struct onefilefs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    if (!strcmp(child_dentry->d_name.name, UNIQUE_FILE_NAME)) {

        the_inode = iget_locked(sb, 1);
        if (!the_inode)
            return ERR_PTR(-ENOMEM);

        if (!(the_inode->i_state & I_NEW)) {
            return child_dentry;
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
        inode_init_owner(current->cred->user_ns, root_inode, NULL,
                         S_IFDIR);
#else
        inode_init_owner(the_inode, NULL, S_IFDIR);
#endif

        the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH |
                            S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP |
                            S_IXOTH;
        the_inode->i_fop = &onefilefs_file_operations;
        the_inode->i_op = &onefilefs_inode_ops;

        set_nlink(the_inode, 1);

        bh = sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER);
        if (!bh) {
            iput(the_inode);
            return ERR_PTR(-EIO);
        }

        FS_specific_inode = (struct onefilefs_inode *)bh->b_data;
        the_inode->i_size = FS_specific_inode->file_size;
        brelse(bh);

        d_add(child_dentry, the_inode);
        dget(child_dentry);

        unlock_new_inode(the_inode);

        return child_dentry;
    }

    return NULL;
}

const struct inode_operations onefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations onefilefs_file_operations = {
    .owner = THIS_MODULE,
    .open = onefilefs_open,
    .read = onefilefs_read,
    .write = onefilefs_write,
    .llseek = onefilefs_llseek,
    .release = onefilefs_release
};

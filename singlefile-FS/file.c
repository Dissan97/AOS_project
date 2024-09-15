#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uio.h>
#include "singlefilefs.h"

static struct mutex lock_log; 
static int onefilefs_open(struct inode *inode, struct file *filp) {
    bool trunc = (filp->f_flags & O_TRUNC);

    if (trunc) {
        printk("%s: truncation requested but ignored\n", MOD_NAME);
        return -EINVAL;
    }


    return 0;
}

ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {

    loff_t offset;
    struct buffer_head *bh = NULL;
    struct inode * the_inode = filp->f_inode;
    uint64_t file_size = the_inode->i_size;
    int ret;
    int block_to_read;//index of the block to be read from device

    printk("%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",MOD_NAME, len, *off, file_size);

    //this operation is not synchronized 
    //*off can be changed concurrently 
    //add synchronization if you need it for any reason
    mutex_lock(&lock_log);
    //check that *off is within boundaries

   

    if (*off >= file_size){
    	 mutex_unlock(&lock_log);
        return 0;}
    else if (*off + len > file_size){
        len = file_size - *off;}

    //determine the block level offset for the operation
 
    offset = *off % DEFAULT_BLOCK_SIZE; 
    //just read stuff in a single block - residuals will be managed at the applicatin level
    if (offset + len > DEFAULT_BLOCK_SIZE)
        len = DEFAULT_BLOCK_SIZE - offset;

    //compute the actual index of the the block to be read from device
    block_to_read = *off / DEFAULT_BLOCK_SIZE + 2; //the value 2 accounts for superblock and file-inode on device
    
    printk("%s: read operation must access block %d of the device",MOD_NAME, block_to_read);

    bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
    if(!bh){
    	mutex_unlock(&lock_log);
	return -EIO;
    }
    ret = copy_to_user(buf,bh->b_data + offset, len);
    *off += (len - ret);
    brelse(bh);
    mutex_unlock(&lock_log);
    return len - ret;

}


struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct onefilefs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    printk("%s: running the lookup inode-function for name %s",MOD_NAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, UNIQUE_FILE_NAME)){

	
	//get a locked inode from the cache 
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
       		 return ERR_PTR(-ENOMEM);

	//already cached inode - simply return successfully
	if(!(the_inode->i_state & I_NEW)){
		return child_dentry;
	}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,12,0)
    inode_init_owner(current->cred->user_ns,root_inode, NULL, S_IFDIR);//set the root user as owner of the FS root
#else
    inode_init_owner(the_inode, NULL, S_IFDIR);//set the root user as owner of the FS root
#endif	
	//this work is done if the inode was not already cached
	the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
        the_inode->i_fop = &onefilefs_file_operations;
	the_inode->i_op = &onefilefs_inode_ops;

	//just one link for this file
	set_nlink(the_inode,1);

	//now we retrieve the file size via the FS specific inode, putting it into the generic inode
    	bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER );
    	if(!bh){
		iput(the_inode);
		return ERR_PTR(-EIO);
    	}
	FS_specific_inode = (struct onefilefs_inode*)bh->b_data;
	the_inode->i_size = FS_specific_inode->file_size;
    brelse(bh);

    d_add(child_dentry, the_inode);
	dget(child_dentry);

	//unlock the inode to make it usable 
    	unlock_new_inode(the_inode);

	return child_dentry;
    }

    return NULL;

}

ssize_t onefilefs_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    struct inode *inode = filp->f_inode;
    struct buffer_head *bh;
    loff_t pos = inode->i_size; // Start appending from the end of the file
    size_t to_write = len;
    size_t written = 0;
    int block_number;
    int offset;
    int ret;
    size_t write_len;

    mutex_lock(&lock_log);
    pr_info("%s: write called\n", MOD_NAME);
    while (to_write > 0) {
        block_number = pos / DEFAULT_BLOCK_SIZE + 2; // Account for superblock and inode
        offset = pos % DEFAULT_BLOCK_SIZE;

        bh = sb_bread(inode->i_sb, block_number);
        if (!bh) {
            mutex_unlock(&lock_log);
            return -EIO;
        }

        // Determine the number of bytes we can write in this block
        write_len = min(to_write, (size_t)(DEFAULT_BLOCK_SIZE - offset));

        // Write the data to the block
        ret = copy_from_user(bh->b_data + offset, buf + written, write_len);
        if (ret != 0) {
            brelse(bh);
            mutex_unlock(&lock_log);
            return -EFAULT;
        }

        mark_buffer_dirty(bh);
        brelse(bh);

        pos += write_len;
        to_write -= write_len;
        written += write_len;
    }

    // Update the file size
    inode->i_size = pos;
    *off = pos;

    mutex_unlock(&lock_log);
    return written;
}
static loff_t onefilefs_llseek(struct file *filp, loff_t offset, int whence) {
    return filp->f_pos;
}


//look up goes in the inode operations
const struct inode_operations onefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations onefilefs_file_operations = {
    .owner = THIS_MODULE,
    .open = onefilefs_open,
    .read = onefilefs_read,
    .write = onefilefs_write,
    .llseek = onefilefs_llseek, 
};
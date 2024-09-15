
#include <linux/module.h>
#include <linux/fs.h>

#define CHANGE_STATE "cngst"
#define CHANGE_PATH "cngpth"
#define PWD "-pwd"
#define OPT "-opt"
#define PTH "-pth"
#define LINE_SIZE 256
#define DEVICE_SYS_VFS "REFERENCE SYS_VFS DEVICE"


#define __on "ON"
#define __off "OFF"
#define __rec_on "RON"
#define __rec_off "ROF"
#define __add_path "ADPTH"
#define __remove_path "RMPHT"

ssize_t syscall_proc_write(struct file *, const char *, size_t, loff_t *);
ssize_t syscall_proc_read(struct file *, char *, size_t, loff_t *);



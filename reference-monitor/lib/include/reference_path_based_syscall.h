
#include <linux/fs.h>
#include <linux/module.h>

#define CHANGE_STATE "cngst"
#define CHANGE_PATH "cngpth"
#define CHANGE_PASSWORD "cngpwd"
#define PWD "-pwd"
#define OPT "-opt"
#define PTH "-pth"
#define OLD_PWD "-old"
#define NEW_PWD "-new"
#define LINE_SIZE 256
#define DEVICE_SYS_VFS "REFERENCE SYS_VFS DEVICE"

#define __on "ON"
#define __off "OFF"
#define __rec_on "RON"
#define __rec_off "ROF"
#define __add_path "ADPTH"
#define __remove_path "RMPTH"

ssize_t syscall_proc_write(struct file *, const char *, size_t, loff_t *);
ssize_t syscall_proc_read(struct file *, char *, size_t, loff_t *);

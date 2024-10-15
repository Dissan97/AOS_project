#pragma once
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/path.h>

int fill_absolute_path(char *pathname);
int fill_with_swap_filter(char *pathname);
int path_struct(char *pathname, struct path *path, int flag);
void mark_inode_read_only(struct inode *inode);
int mark_inode_read_only_by_pathname(char *pathname);
int fill_path_by_dentry(struct dentry *dentry, char *pathname);
int get_parant_absolute_path(char *pathname);
void get_parent_path(char *path);
# SINGLE File FS

This FS contains a single file, the block organization is the following.

```
-----------------------------------------------------
| Block 0    | Block 1      | Block 2    | Block 3   |
| Superblock | inode of the | File data  | File data |
|            | File         |            |           |
-----------------------------------------------------
```


## compilation 
```
    make
```

## installation
create an image of the file system needed for the module defaul size of the filesystem 256MB 
```
   make create
``` 
the max file size can be setup by passing how much bytes needed example 256KB=262144 bytes

```
    make create MAX_FILE_SIZE=262144
```
load the module
```
    make load
```
connect mount to module
```
    make mnt
```
remove
```
    make remove
```

# files
- [dir](dir.c) : directory management of the filesystem
- [file](file.c): file operation implementations
- [singlefilefs_src](singlefilefs_src.c) : module init and remove
- [singlefilemakefs](singlefilemakefs.c) : image creation that fill the metadata for inode and super block

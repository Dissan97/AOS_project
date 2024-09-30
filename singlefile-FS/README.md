# SINGLE File FS

This FS contains a single file, the block organization is the following.

```
-----------------------------------------------------
| Block 0    | Block 1      | Block 2    | Block 3   |
| Superblock | inode of the | File data  | File data |
|            | File         |            |           |
-----------------------------------------------------
```



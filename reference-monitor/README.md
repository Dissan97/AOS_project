# Reference Monitor
Reference monitor to secure writing on protected files or directories



## @Autor: Dissan Uddin Ahmed
## @Accademic_year: 2023/2024

## compilation
```
    make
```
## insmod
run with root permission
```
    make mount
```
insmod the_reference_monitor.ko the_syscall_table=\$(hacked_sys_table_addr) pwd='RefMonitorPwd' single_file_name=\$(realpath ../singlefile-FS/mount/the-file)
- the_syscall_table: address of the system call table hack module
- pwd: the password to pass for reference monitor
- single_file_name: the path of the singlefile-FS single file

## files
- [reference_monitor.c](reference_monitor.c): implementation of init and clean function and sys_call_wrapper for hacked system calls and proc definition for path base syscall
- [reference_defer.c](lib/reference_defer.c) : implementation of defer working after some thread try to access unathorize folder
- [reference_hash.c](lib/reference_hash.c) : implementation of password hash function
- [path_based_syscall.c](lib/path_based_syscall.c) : implementation of path based syscall using /proc
- [path_path_solver.c](lib/path_path_solver.c) :implementation of path_solver_logic for the reference_monitor
- [reference_rcu_restrict_list.c](lib/reference_rcu_restrict_list.c) : rcu list that handle black listed paths
- [reference_syscalls.c](lib/reference_syscalls.c) : actual implementation of the system call for reference monitor
- [scth.c](lib/scth.c) : needed to hack system call table


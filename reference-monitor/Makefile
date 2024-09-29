obj-m += the_reference_monitor.o
the_reference_monitor-objs += reference_monitor.o lib/scth.o lib/reference_path_solver.o  lib/reference_hooks.o lib/reference_syscalls.o lib/reference_rcu_restrict_list.o lib/reference_hash.o lib/reference_path_based_syscall.o lib/reference_defer.o

hacked_sys_table_addr = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

no_lock:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) CFLAGS_MODULE="-DNO_LOCK" modules

mount:
	insmod the_reference_monitor.ko the_syscall_table=$(hacked_sys_table_addr) pwd='RefMonitorPwd' single_file_name=$(realpath ../singlefile-FS/mount/the-file)

unmount:
	rmmod the_reference_monitor

test:
	gcc ./user/user.c -o ./user/test
	./user/test

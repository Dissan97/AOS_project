MAX_FILE_SIZE ?= 268435456  # Default to 256MB
BLOCK_SIZE = 4096            # Block size 4KB
BLOCK_COUNT = $(shell echo $(MAX_FILE_SIZE)/$(BLOCK_SIZE) | bc)  # Compute block count Default to 256MB

obj-m += singlefilefs.o
singlefilefs-objs += singlefilefs_src.o file.o dir.o

all:
	gcc singlefilemakefs.c -o singlefilemakefs.out
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	sudo insmod singlefilefs.ko max_file_size=$(MAX_FILE_SIZE)

create:
	dd bs=$(BLOCK_SIZE) count=$(BLOCK_COUNT)  if=/dev/zero of=image
	./singlefilemakefs.out image
	mkdir mount
	
mnt:
	sudo mount -o loop -t singlefilefs image ./mount/	
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	
remove:
	umount -l ./mount
	rm -rf image
	rm -rf mount
	rmmod singlefilefs
	
	
	

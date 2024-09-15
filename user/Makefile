CFLAGS = -Wall -Wextra
LIB_FILES = lib/ref_syscall.c


all:
	gcc $(CFLAGS) admin.c $(LIB_FILES) -o admin.out

# Clean rule to remove the executable
clean:
	rm -f $(admin.out)


#include <unistd.h>
#include <stdio.h>
int main(int argc, char** argv){
	int ret = syscall(134,1,2);
	if (ret < 0){
		perror("something wrong");
	
	}
}

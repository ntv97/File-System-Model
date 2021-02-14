#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int main() {
	int fd = open("4096.txt", O_RDWR, 0644);
	char buffer[4096];
	//memset(buffer, '0', 4098);
	read(fd, buffer, 8);
	printf("%s\n", buffer);
	printf("sizeofbuffer = %ld\n", sizeof(buffer));

	int index;
	float temp = 4098.0/4096.0;
	if(temp>4096)
		index++;
	
	if(buffer[4095]==0)
		printf("4096 = %c\n",buffer[4097]);
	char *ptr = "e";
	printf("sizeof ptr = %ld\n\n", strlen(ptr));

	int count = 6000;
	int num = 1;
	double res = (double)count/4096.0;
	if(23%5 != 0) 
		printf("no\n");
	printf("6000div4096 = %f\n", res);
	return 0;
}

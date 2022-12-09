/* simple program that uses ioctl to send a command to given file */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd, count;
	unsigned long cmd;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s file-name ioctl-command\n", argv[0]);
		return -1;
	}

	cmd = atol(argv[2]);
	if (cmd < 1 || cmd > 100) {
		fprintf(stderr, "Usage: %s file-name ioctl-command\n", argv[0]);
		fprintf(stderr, "ioctl-command must be a number from {1,100}\n");
		return -1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open failed");
		return -1;
	}

	count = ioctl(fd, cmd);
	if (count == -1) {
		perror("ioctl error");
		return -1;
	}

	printf("ioctl returned %d\n", count);

	return 0;
}

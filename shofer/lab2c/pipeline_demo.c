#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define READ 0
#define WRITE 1

#define READ_SLEEP 10
#define READ_CHUNK 8
#define WRITE_SLEEP 5

int main(int argc, char *argv[])
{
	int fd, mode;
	char *msg = "cjevovodjevelik ";
	ssize_t rsize;
	ssize_t wsize;
	char buf[READ_CHUNK];

	if (argc != 3) {
		printf("Usage: %s file-name mode-type\n", argv[0]);
		exit(1);
	}

	// open in read or write mode
	mode = atoi(argv[2]);

	if (mode == READ) {
		// open in read mode

		fd = open(argv[1], O_RDONLY);
		if (fd == -1) {
			perror("open failed");
			exit(1);
		}

		printf("Reading process started\n");

		// read from pipeline in loop
		while (1) {

			// block read while empty
			while ((rsize = read(fd, buf, READ_CHUNK)) == 0) {
				printf("Trying to read from %s ...\n", argv[1]);
				sleep(READ_SLEEP);
			}

			// print what is read and its size
			printf("Successfully read from %s: size=%ld msg=%s\n", argv[1], rsize, buf);

			// clear local buffer
			memset(buf, '\0', sizeof(buf));
		}

	} else if (mode == WRITE) {
		// open in write mode

		fd = open(argv[1], O_WRONLY);
		if (fd == -1) {
			perror("open failed");
			exit(1);
		}

		printf("Writing process started\n");

		// write to pipeline in loop
		while (1) {

			// block write while not enough space
			while ((wsize = write(fd, msg, strlen(msg))) != strlen(msg)) {
				printf("Trying to write '%s' to %s ...\n", msg, argv[1]);

				if (wsize == 0) {
					printf("Not enough space at the moment, please wait ...\n");
				} else if (wsize == -1) {
					printf("Message is to long for shofer buffer!\n");
					exit(1);
				}

				sleep(WRITE_SLEEP);
			}

			// print what is written
			printf("Successfully wrote to %s: size=%ld msg=%s\n", argv[1], wsize, msg);
		}
	} else {
		// invalid mode

		printf("Invalid mode\n");
		exit(1);
	}

	return 0;
}

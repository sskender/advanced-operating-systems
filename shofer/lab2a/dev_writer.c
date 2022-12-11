#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define BUFF_SIZE 1

#define WAIT_BETWEEN_POLL_SEC 5

int
main(int argc, char *argv[])
{
    int nfds, num_open_fds;
    struct pollfd *pfds;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    num_open_fds = nfds = argc - 1;
    pfds = calloc(nfds, sizeof(struct pollfd));
    if (pfds == NULL)
        errExit("malloc");

    /* Open each file on command line, and add it 'pfds' array. */

    for (int j = 0; j < nfds; j++) {
        pfds[j].fd = open(argv[j + 1], O_WRONLY);
        if (pfds[j].fd == -1)
            errExit("open");

        printf("Opened \"%s\" on fd %d\n", argv[j + 1], pfds[j].fd);

        pfds[j].events = POLLOUT;
    }

    /* Keep calling poll() as long as at least one file descriptor is
        open. */

    while (num_open_fds > 0) {

        // wait n seconds before write
        sleep(WAIT_BETWEEN_POLL_SEC);

        int ready;

        printf("About to poll()\n");
        ready = poll(pfds, nfds, -1);
        if (ready == -1)
            errExit("poll");

        printf("Ready: %d\n", ready);

        /* Deal with array returned by poll(). */

        int j = rand() % nfds;
        char buf[BUFF_SIZE] = "x";

        if (pfds[j].revents != 0) {
            printf("  fd=%d; events: %s%s%s\n", pfds[j].fd,
                    (pfds[j].revents & POLLOUT)  ? "POLLOUT "  : "",
                    (pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
                    (pfds[j].revents & POLLERR) ? "POLLERR " : "");

            if (pfds[j].revents & POLLOUT) {
                ssize_t s = write(pfds[j].fd, buf, sizeof(buf));
                if (s == -1)
                    errExit("write");
                printf("    write %zd bytes: %.*s\n",
                        s, (int) s, buf);
            } else {                /* POLLERR | POLLHUP */
                printf("    closing fd %d\n", pfds[j].fd);
                if (close(pfds[j].fd) == -1)
                    errExit("close");
                num_open_fds--;
            }
        }
    }

    printf("All file descriptors closed; bye\n");
    exit(EXIT_SUCCESS);
}
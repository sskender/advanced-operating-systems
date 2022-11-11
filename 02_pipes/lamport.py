from multiprocessing import Process, Pipe
import os
import sys
import logging
import time


# r, w = os.pipe()

# pipes = []
NUM_PROCESSES = 3
NUM_PIPES = 6

forks = 3

def run_process(pipes, i):
    ppid = os.getppid()
    logging.info(f"Process {i} started from parent {ppid}")
    time.sleep(0.5)

    read_pipes = []
    write_pipes = []


    if i == 1:
        # os.close(pipes[0][0])
        # os.close(pipes[1][1])
        # os.close(pipes[4][1])
        # os.close(pipes[5][0])
        os.close(pipes[4][1])
        os.close(pipes[5][0])
        read_pipes.append(pipes[4][0])

        # os.close(pipes[0][0])
        # os.close(pipes[1][1])
        # read_pipes.append(pipes[1][])
        # os.close(pipes[5][0])

        # write_pipes.append(pipes[0][1])
        # read_pipes.append(pipes[1][0])
        # write_pipes.append(pipes[5][1])
        # text = b"hello"
        # os.write(pipes[0][1], text)

        # read_pipes = [pipes[1][0], pipes[4][0]]
        # write_pipes = [pipes[0][1], pipes[5][1]]
    elif i == 2:
        # os.close(pipes[0][1])
        # os.close(pipes[1][0])
        # os.close(pipes[2][0])
        # os.close(pipes[3][1])
        # read_pipes = [pipes[0][0], pipes[3][0]]
        # write_pipes = [pipes[1][1], pipes[2][1]]

        # os.close(pipes[1][0])
        # os.close(pipes[0][1])
        # read_pipes.append(pipes[0][0])
        # write_pipes.append(pipes[1][1])

        os.close(pipes[2][0])
        os.close(pipes[3][1])
        write_pipes.append(pipes[2][1])
        # msg = os.fdopen(pipes[0][0])
        # print(f"msg is {msg.read()}")
    elif i == 3:
        os.close(pipes[2][1])
        os.close(pipes[3][0])
        read_pipes.append(pipes[2][0])

        os.close(pipes[4][0])
        os.close(pipes[5][1])
        write_pipes.append(pipes[4][1])
        # os.close(pipes[5][1])
        # read_pipes.append(pipes[5][0])
        # # os.close(pipes[2][1])
        # os.close(pipes[3][0])
        # os.close(pipes[4][0])
        # os.close(pipes[5][1])
        # read_pipes = [pipes[2][0], pipes[5][0]]
        # write_pipes = [pipes[3][1], pipes[4][1]]

    time.sleep(1)
    logging.debug(f"Process {i} read: {read_pipes} write: {write_pipes}")

    for w in write_pipes:
        logging.info(f"Process {i} is sending message")
        # text = bytes(f"{i} sending msg", "utf-8")
        text = bytes(f"Process {i} sent: kurac pasji na krizu", "utf-8")
        os.write(w, text)
        # break

    for r in read_pipes:
        logging.info(f"Process {i} is reading a message")
        msg = os.fdopen(r)
        logging.debug(msg)
        text = msg.read()
        print(text)
        logging.info(f"Process {i} read: {text}")


    if i == 2:
        time.sleep(3)
    if i == 3:
        time.sleep(2)
    logging.info(f"Process {i} is done")
    time.sleep(1)

# def cpu1(r, w):
#     # write
#     os.close(r)
#     text = b"hello"
#     os.write(w, text)


# def cpu2(r, w):
#     # read
#     os.close(w)
#     r = os.fdopen(r)
#     print(r.read())

# def cpu(r, w, i):
#     if i == 0:
#         os.close(r)
#     if i == 1:
#         os.close(w)

#     text = b"hello"
#     os.write(w, text)
#     r = os.fdopen(r)
#     print(r.read())

def main():
    pipes = []
    logging.debug('Starting main')

    for i in range(NUM_PIPES):
        r, w = os.pipe()
        pipes.append((r, w))
    logging.debug(pipes)

    r, w = os.pipe()

    for i in range(NUM_PROCESSES):
        pid = os.fork()
        logging.debug(f"Working in pid {pid}")
        if pid == 0:
            run_process(pipes, i+1)
            # cpu(r, w, i)
            # if i == 0:
            #     cpu1(r, w)
            # elif i == 1:
            #     cpu2(r, w)
            return
        else:
            logging.debug(f"I am parent process {pid}")

    # TODO implement waiting for child processes


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()


# for i in range(forks):
#     try:
#         pid = os.fork()
#     except OSError:
#         sys.stderr.write("Could not create a child process\n")
#         continue

#     if pid == 0:
#         print("In the child process {} that has the PID {}".format(i+1, os.getpid()))
#         exit()
#     else:
#         print("In the parent process after forking the child {}".format(pid))

# print("In the parent process after forking {} children".format(forks))

# for i in range(forks):
#     finished = os.waitpid(0, 0)
#     print(finished)

"""
for proc in range(NUM_PROCESSES):

    r, w = os.pipe() # from parent to child
    r2, w2 = os.pipe() # from child to parent
    # TODO add second pipe

    pid = os.fork()
    # print(pid)

    if pid > 0:
        # write to pipe
        os.close(r)
        text_str = f"Hello for child {proc+1}"
        text = bytes(text_str, "utf-8")
        os.write(w, text)

        # read from pipe
        # os.close(w2)
        # msg = os.fdopen(r2)
        # print(f"this is read in parent {pid} : {msg.read()}")

    if pid == 0:
        print(f"this is child: {pid}")

        # read pipe
        os.close(w)
        msg = os.fdopen(r)
        print(f"this is read in child {pid} : {msg.read()}")

        # write to pipe
        os.close(r2)
        text_str = f"Hello for parent from child {proc+1}"
        text = bytes(text_str, "utf-8")
        os.write(w2, text)

        # TODO do work function
        exit()

# pid = os.fork()

# # pid greater than 0 represents
# # the parent process
# if pid > 0:
#     # This is the parent process 
#     # Closes file descriptor r
#     os.close(r)
#     # Write some text to file descriptor w 
#     print("Parent process is writing")
#     text = b"Hello child process"
#     os.write(w, text)
#     print("Written text:", text.decode())
# else:
#     # This is the parent process 
#     # Closes file descriptor w
#     os.close(w)
#     # Read the text written by parent process
#     print("\nChild Process is reading")
#     r = os.fdopen(r)
#     print("Read text:", r.read())
"""

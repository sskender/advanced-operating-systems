Example device driver

Based mostly on instructions in book
	"Linux Device Drivers, Third Edition" by
	Jonathan Corbet, Alessandro Rubini, and Greg Kroah-Hartman


Copyright (C) 2021 Leonardo Jelenkovic

The source code in this file can be freely used, adapted,
and redistributed in source or binary form.
No warranty is attached.


INSTRUCTIONS:


Compile and run:

$ make

$ ./load_shofer


Store something in the buffer:

$ echo -n "hello world" > /dev/shofer_in


Read from buffer (only what is transfered with timer every 10 seconds):

$ cat /dev/shofer_out


Transfer using ioctl program:

$ cd test

$ gcc -o shofer-ioctl ioctl.c

$ $ echo -n "today is a good day" > /dev/shofer_in

$ ./shofer-ioctl /dev/shofer_control 10

$ cat /dev/shofer_out  # today is a

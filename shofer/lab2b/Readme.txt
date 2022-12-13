LAB2B INSTRUCTIONS:

1. Compile kernel module
-------------------------
	$ make

2. Load module into kernel and make it ready for use
-----------------------------------------------------
	$ ./load_shofer

3. Store something in the buffer
---------------------------------
	$ echo -n "hello world" > /dev/shofer_in

4. Read from buffer (only what is transfered with timer every 10 seconds)
-------------------------------------------------------------------------
	$ cat /dev/shofer_out

5. Transfer using ioctl program
--------------------------------
	$ cd test

	$ gcc -o shofer-ioctl ioctl.c

	$ echo -n "today is a good day" > /dev/shofer_in

	$ ./shofer-ioctl /dev/shofer_control 10

	$ cat /dev/shofer_out  # today is a

6. Monitor kernel logs
-----------------------
    $ tail /var/log/kern.log

7. Unload module
-----------------
    $ ./unload_shofer

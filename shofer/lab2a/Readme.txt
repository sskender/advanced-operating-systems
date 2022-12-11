Example device driver + timers + workqueues

Based mostly on instructions in book
	"Linux Device Drivers, Third Edition" by
	Jonathan Corbet, Alessandro Rubini, and Greg Kroah-Hartman


Copyright (C) 2021 Leonardo Jelenkovic

The source code in this file can be freely used, adapted,
and redistributed in source or binary form.
No warranty is attached.


Lab2a RUN:

1. Compile kernel module
-------------------------
   $ make

2. Load module into kernel and make it ready for use
-----------------------------------------------------
   $ ./load_shofer

3. Run reader program
----------------------
   $ gcc -o reader dev_reader.c
   $ ./reader /dev/shofer0 /dev/shofer1 /dev/shofer2

4. Run writer program
----------------------
   $ gcc -o writer dev_writer.c
   $ ./writer /dev/shofer0 /dev/shofer1 /dev/shofer2

5. Monitor kernel logs
-----------------------
   $ tail /var/log/kern.log

6. Unload module
-----------------
   $ ./unload_shofer

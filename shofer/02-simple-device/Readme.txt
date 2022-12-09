Example device driver

Based mostly on instructions in book
	"Linux Device Drivers, Third Edition" by
	Jonathan Corbet, Alessandro Rubini, and Greg Kroah-Hartman


Copyright (C) 2021 Leonardo Jelenkovic

The source code in this file can be freely used, adapted,
and redistributed in source or binary form.
No warranty is attached.

#######################################################################
How to create a device (basic steps):
1. alloc_chrdev_region - reserve device numbers (major, minor)
2. initialize driver data
3. initializa cdev structure (within driver data)
4. cdev_init
2. cdev_add - add initialized structure as device

On module exit:
1. cdev_del
2. unregister_chrdev_region
#######################################################################

Testing:

1. Compile kernel module
---------------------
   $ make

2. Load module into kernel and make it ready for use
-----------------------------------------------------
   Use provided script load_shofer
   $ sudo ./load_shofer buffer_size=2048
   or, if file load_shofer don't have x file permission
   $ sudo /bin/sh ./load_shofer

   Or manually:
   a. Load module into kernel
      $ sudo insmod shofer.ko [arguments, e.g. buffer_size=1024]

   b. Make device visible and usable in /dev
      Look for device number in log:
      $ tail /var/log/kern.log
      [*] Module 'shofer' initialized with major=237, minor=0

      Or in /proc/devices
      $ cat /proc/devices | grep shofer
      237 shofer

      For given example: major=237, minor=0

      Remove stale nodes and replace them
      $ sudo rm -f /dev/shofer
      $ sudo mknod /dev/shofer c 237 0
      $ sudo chmod 666  /dev/shofer

4. Test device
---------------------
   Turn on messages from /var/log/kern.log
   E.g. with: tail -f /var/log/kern.log in another shell, or in the same with &

   Simple writes:
   $ echo -n "12345467890abcdefghijklmnoprstuvzyw" > /dev/shofer
   # flag -n suppress trailing \n

   Read 5 bytes:
   $ dd if=/dev/shofer bs=5 count=1 status=none

   Read all:
   $ cat /dev/shofer

5. Unload module
---------------------
   With provided script:
   $ sudo ./unload_shofer # or sudo /bin/sh ./unload_shofer

   or manually:
   $ sudo rmmod shofer
   $ sudo rm -f /dev/shofer

6. When something goes wrong
-----------------------------
   When there is a bug in module and program using it stops:
   1. try to kill process (Ctrl+C, or from other terminal)
   2. if 1. don't work, try to kill shell from which program was started
   3. try to unload module, sudo ./unload_shofer
   4. if 3. don't work try sudo rmmod -f shofer
   5. if 4. doesn't work, try restarting the system (soft, hard)
   6. if module was stuck in a loop and produces a lot of messages,
      logs might be full! delete them with:
      $ sudo truncate -s 0 /var/log/kern.log /var/log/syslog

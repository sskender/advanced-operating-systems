Example module

Copyright (C) 2021 Leonardo Jelenkovic

The source code in this file can be freely used, adapted,
and redistributed in source or binary form.
No warranty is attached.

#######################################################################

Testing:

1. Compile kernel module
---------------------
   $ make

2. Load module into kernel and make it ready for use
-----------------------------------------------------
   Use provided script load_shofer
   $ sudo ./load_shofer
   or, with
   $ sudo /bin/sh ./load_shofer

   Or manually:
   $ sudo insmod shofer.ko

3. Look into log file
-----------------------------------------------------
   $ tail /var/log/kern.log

4. Unload module
---------------------
   With provided script:
   $ sudo ./unload_shofer # or sudo /bin/sh ./unload_shofer

   or manually:
   $ sudo rmmod shofer

5. Look into log file
-----------------------------------------------------
   $ tail /var/log/kern.log

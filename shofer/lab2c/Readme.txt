LAB2C INSTRUCTIONS:

1. Compile kernel module
-------------------------
    $ make

2. Load module into kernel and make it ready for use
-----------------------------------------------------
    $ ./load_shofer

3. Compile pipeline program
----------------------------
    $ gcc pipeline_demo.c -o pip

4. Run pipeline in read mode
-----------------------------
    $ ./pip /dev/shofer 0

5. Run pipeline in write mode
------------------------------
    $ ./pip /dev/shofer 1

6. Monitor kernel logs
-----------------------
    $ tail /var/log/kern.log

7. Unload module
-----------------
    $ ./unload_shofer

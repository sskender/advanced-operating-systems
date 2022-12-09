/*
 * shofer.c -- module implementation
 *
 * "Hello World" module
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#include <linux/module.h>

#include "config.h"

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

/* init module */
static int __init shofer_module_init(void)
{
	printk(KERN_NOTICE "Module " MODULE_NAME " started\n");

	return 0;
}

/* called when module exit */
static void __exit shofer_module_exit(void)
{
	printk(KERN_NOTICE "Module " MODULE_NAME " unloaded\n");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

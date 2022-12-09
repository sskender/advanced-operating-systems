/*
 * shofer.c -- module implementation
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#include <linux/module.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include "config.h"

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

static struct cdev cdev;
static dev_t dev_no = 0;
static int irq_no = IRQ_NO;

/* prototypes */
static void cleanup(void);
static irqreturn_t irq_handler(int, void *);
static irqreturn_t irq_thread_handler(int, void *);

static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);

static struct file_operations shofer_fops = {
	.owner =    THIS_MODULE,
	.read =     shofer_read,
};

/* init module */
static int __init shofer_module_init(void)
{
	int retval;

	printk(KERN_NOTICE "shofer: started initialization\n");

	retval = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
	if (retval < 0) {
		printk(KERN_WARNING "shofer: can't get major device number %d\n",
			MAJOR(dev_no));
		return retval;
	}

	cdev_init(&cdev, &shofer_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &shofer_fops;
	retval = cdev_add (&cdev, dev_no, 1);
	if (retval) {
		printk(KERN_WARNING "shofer: error (%d) when adding device shofer\n",
			retval);
		goto no_driver;
	}

	if (request_threaded_irq(irq_no, irq_handler, irq_thread_handler,
		IRQF_SHARED, DRIVER_NAME, (void *) irq_handler))
	{
		printk(KERN_WARNING "shofer: cannot register IRQ ");
		irq_no = 0;
		goto no_driver;
	}
	/* look in /proc/interrupts for 'shofer' */

	printk(KERN_NOTICE "shofer: initialized with major=%d, minor=%d\n",
		MAJOR(dev_no), MINOR(dev_no));

	return 0;

no_driver:
	cleanup();

	return retval;
}

static void cleanup(void)
{
	if (dev_no)
		unregister_chrdev_region(dev_no, 1);
	if (irq_no)
		free_irq(irq_no, (void *) irq_handler);
}

static void __exit shofer_module_exit(void)
{
	printk(KERN_NOTICE "shofer: started exit operation\n");
	cleanup();
	printk(KERN_NOTICE "shofer: finished exit operation\n");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
	loff_t *f_pos)
{
	asm("int $0x3B"); /* simulate interupt; probably doesn't work */

	return 0;
}

/* function called when interrupt occurs - 'top half' of interrupt processing */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	printk(KERN_INFO "shofer: in irq handler (top-half)");
	return IRQ_WAKE_THREAD;
	/* top half done, but further processing required by thread */
}

/* function called after 'top half' is done and returned IRQ_WAKE_THREAD */
static irqreturn_t irq_thread_handler(int irq, void *dev_id)
{
	printk(KERN_INFO "shofer: in irq thread handler (bottom-half)");
	return IRQ_HANDLED; /* processing is now completed */
}

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
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#include <linux/poll.h>

#include "config.h"

static int buffer_size = BUFFER_SIZE;	/* Buffer size */
static int buffer_num = BUFFER_NUM;	/* Number of buffers */
static int driver_num = DRIVER_NUM;	/* Number of drivers */

/* Some parameters can be given at module load time */
module_param(buffer_size, int, S_IRUGO);
MODULE_PARM_DESC(buffer_size, "Buffer size in bytes");
module_param(buffer_num, int, S_IRUGO);
MODULE_PARM_DESC(buffer_num, "Number of buffers to create");
module_param(driver_num, int, S_IRUGO);
MODULE_PARM_DESC(driver_num, "Number of devices to create");

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

static LIST_HEAD(buffers_list);
static LIST_HEAD(shofers_list); /* A list of devices */

static dev_t Dev_no = 0;

/* prototypes */
static struct buffer *buffer_create(size_t, int *);
static void buffer_delete(struct buffer *);
static struct shofer_dev *shofer_create(dev_t, struct file_operations *,
	struct buffer *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);
static void dump_buffer(char *, struct shofer_dev *, struct buffer *);
static void simulate_delay(long delay_ms);

static int shofer_open(struct inode *, struct file *);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);
static unsigned int shofer_poll(struct file *filp, poll_table *wait);

static struct file_operations shofer_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open,
	.read =     shofer_read,
	.write =    shofer_write,
	.poll =     shofer_poll
};

/* init module */
static int __init shofer_module_init(void)
{
	int retval, i;
	struct buffer *buffer;
	struct shofer_dev *shofer;
	dev_t dev_no = 0;

	klog(KERN_NOTICE, "Module started initialization");

	/* get device number(s) */
	retval = alloc_chrdev_region(&dev_no, 0, driver_num, DRIVER_NAME);
	if (retval < 0) {
		klog(KERN_WARNING, "Can't get major device number");
		return retval;
	}
	Dev_no = dev_no; //remember first

	/* Create and add buffers to the list */
	for (i = 0; i < buffer_num; i++) {
		buffer = buffer_create(buffer_size, &retval);
		if (!buffer)
			goto no_driver;
		list_add_tail(&buffer->list, &buffers_list);
	}

	/* Create and add devices to the list */
	for (i = 0; i < driver_num; i++) {
		shofer = shofer_create(dev_no, &shofer_fops, NULL, &retval);
		if (!shofer)
			goto no_driver;
		list_add_tail(&shofer->list, &shofers_list);
		dev_no = MKDEV(MAJOR(dev_no), MINOR(dev_no) + 1);
	}

	/* assign buffers to devices in round robin fashion */
	buffer = list_first_entry(&buffers_list, struct buffer, list);
	list_for_each_entry(shofer, &shofers_list, list) {
		shofer->buffer = buffer;
		dump_buffer("shofer-initilized", shofer, buffer);
		if (!list_is_last(&buffer->list, &buffers_list))
			buffer = list_next_entry(buffer, list);
		else
			buffer = list_first_entry(&buffers_list, struct buffer, list);
	}

	klog(KERN_NOTICE, "Module initialized with major=%d", MAJOR(dev_no));

	return 0;

no_driver:
	cleanup();

	return retval;
}

static void cleanup(void)
{
	struct buffer *buffer, *b;
	struct shofer_dev *shofer, *s;

	list_for_each_entry_safe (shofer, s, &shofers_list, list) {
		list_del (&shofer->list);
		shofer_delete(shofer);
	}
	list_for_each_entry_safe (buffer, b, &buffers_list, list) {
		list_del (&buffer->list);
		buffer_delete(buffer);
	}

	if (Dev_no)
		unregister_chrdev_region(Dev_no, driver_num);
}

/* called when module exit */
static void __exit shofer_module_exit(void)
{
	klog(KERN_NOTICE, "Module started exit operation");
	cleanup();
	klog(KERN_NOTICE, "Module finished exit operation");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

/* Create and initialize a single buffer */
static struct buffer *buffer_create(size_t size, int *retval)
{
	static int buffer_id = 0;
	struct buffer *buffer = kmalloc(sizeof(struct buffer) + size, GFP_KERNEL);
	if (!buffer) {
		*retval = -ENOMEM;
		klog(KERN_WARNING, "kmalloc failed\n");
		return NULL;
	}
	*retval = kfifo_init(&buffer->fifo, buffer + 1, size);
	if (*retval) {
		kfree(buffer);
		klog(KERN_WARNING, "kfifo_init failed\n");
		return NULL;
	}
	buffer->id = buffer_id++;
	mutex_init(&buffer->lock);

	*retval = 0;

	return buffer;
}
static void buffer_delete(struct buffer *buffer)
{
	kfree(buffer);
}

/* Create and initialize a single shofer_dev */
static struct shofer_dev *shofer_create(dev_t dev_no,
	struct file_operations *fops, struct buffer *buffer, int *retval)
{
	static int shofer_id = 0;
	struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
	if (!shofer){
		*retval = -ENOMEM;
		klog(KERN_WARNING, "kmalloc failed\n");
		return NULL;
	}
	memset(shofer, 0, sizeof(struct shofer_dev));
	shofer->buffer = buffer;

	cdev_init(&shofer->cdev, fops);
	shofer->cdev.owner = THIS_MODULE;
	shofer->cdev.ops = fops;
	*retval = cdev_add (&shofer->cdev, dev_no, 1);
	if (*retval) {
		klog(KERN_WARNING, "Error (%d) when adding device", *retval);
		kfree(shofer);
		return NULL;
	}
	shofer->dev_no = dev_no;
	shofer->id = shofer_id++;

	/* for poll */
	init_waitqueue_head(&shofer->rq);
	init_waitqueue_head(&shofer->wq);

	return shofer;
}
static void shofer_delete(struct shofer_dev *shofer)
{
	cdev_del(&shofer->cdev);
	kfree(shofer);
}

/* Open called when a process calls "open" on this device */
static int shofer_open(struct inode *inode, struct file *filp)
{
	struct shofer_dev *shofer; /* device information */

	shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);
	filp->private_data = shofer; /* for other methods */

	return 0;
}

static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
	loff_t *f_pos)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int copied;

	if (mutex_lock_interruptible(&buffer->lock))
		return -ERESTARTSYS;

	dump_buffer("read-start", shofer, buffer);

	retval = kfifo_to_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		klog(KERN_WARNING, "kfifo_to_user failed\n");
	else
		retval = copied;

	simulate_delay(1000);

	dump_buffer("read-end", shofer, buffer);

	mutex_unlock(&buffer->lock);

	wake_up_all(&shofer->rq); /* for poll */

	return retval;
}

static ssize_t shofer_write(struct file *filp, const char __user *ubuf,
	size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int copied;

	if (mutex_lock_interruptible(&buffer->lock))
		return -ERESTARTSYS;

	dump_buffer("write-start", shofer, buffer);

	retval = kfifo_from_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		klog(KERN_WARNING, "kfifo_from_user failed\n");
	else
		retval = copied;

	simulate_delay(1000);

	dump_buffer("write-end", shofer, buffer);

	mutex_unlock(&buffer->lock);

	wake_up_all(&shofer->wq); /* for poll */

	return retval;
}

static unsigned int shofer_poll(struct file *filp, poll_table *wait)
{
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int len = kfifo_len(fifo);
	unsigned int avail = kfifo_avail(fifo);
	unsigned int mask = 0;

	poll_wait(filp, &shofer->rq, wait);
	poll_wait(filp, &shofer->wq, wait);

	if (len)
		mask |= POLLIN | POLLRDNORM; /* readable */
	if (avail)
		mask |= POLLOUT | POLLWRNORM; /* writable */

	return mask;
}

static void dump_buffer(char *prefix, struct shofer_dev *shofer, struct buffer *b)
{
	char buf[BUFFER_SIZE];
	size_t copied;

	memset(buf, 0, BUFFER_SIZE);
	copied = kfifo_out_peek(&b->fifo, buf, BUFFER_SIZE);

	LOG("%s:id=%d,buffer:id=%d:size=%u:contains=%u:buf=%s",
	prefix, shofer->id, b->id, kfifo_size(&b->fifo), kfifo_len(&b->fifo), buf);
}

static void simulate_delay(long delay_ms)
{
	int retval;
	wait_queue_head_t wait;
	init_waitqueue_head (&wait);
	retval = wait_event_interruptible_timeout(wait, 0,
		msecs_to_jiffies(delay_ms));
}

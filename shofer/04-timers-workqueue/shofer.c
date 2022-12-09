/*
 * shofer.c -- module implementation
 *
 * Example module with devices, timer, workqueus, completions, ...
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
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/log2.h>
#include <linux/uaccess.h>

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

static struct timer_list timer;

/* prototypes */
static struct buffer *buffer_create(size_t, int *);
static void buffer_delete(struct buffer *);
static struct shofer_dev *shofer_create(dev_t, struct file_operations *,
	struct buffer *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);
static void dump_buffer(char *, struct shofer_dev *, struct buffer *);
//static void simulate_delay(long delay_ms);
static void timer_function(struct timer_list *t);
static void workqueue_operations(struct work_struct *work);

static int shofer_open(struct inode *, struct file *);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations shofer_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open,
	.read =     shofer_read,
	.write =    shofer_write
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

	/* Create timer that will periodically put content in first buffer */
	timer_setup(&timer, timer_function, 0);
	timer.expires = jiffies + msecs_to_jiffies(TIMER_PERIOD);
	add_timer(&timer);

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

	del_timer(&timer);
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
		return NULL;
	}
	*retval = kfifo_init(&buffer->fifo, buffer + 1, size);
	if (*retval) {
		kfree(buffer);
		klog(KERN_WARNING, "kfifo_init failed\n");
		return NULL;
	}
	buffer->id = buffer_id++;
	spin_lock_init(&buffer->key);

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
	struct shofer_dev *shofer;
	char wqname[8];

	shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
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

	/* queue for tasks waiting on write workqueue completion */
	init_waitqueue_head(&shofer->wqueue);

	wqname[0] = 'r';
	wqname[1] = 'w';
	wqname[2] = 'q';
	wqname[3] = '0' + shofer->id/1000 % 10;
	wqname[4] = '0' + shofer->id/100 % 10;
	wqname[5] = '0' + shofer->id/10 % 10;
	wqname[6] = '0' + shofer->id % 10;
	wqname[7] = 0;

	shofer->rwq = create_singlethread_workqueue(wqname);
	if (!shofer->rwq) {
		klog(KERN_WARNING, "create_singlethread_workqueue error");
		kfree(shofer);
		*retval = -ENOMEM;
		shofer->rwq = NULL;
		return NULL;
	}

	wqname[0] = 'w';
	shofer->wwq = create_singlethread_workqueue(wqname);
	if (!shofer->wwq) {
		klog(KERN_WARNING, "create_singlethread_workqueue error");
		destroy_workqueue(shofer->rwq);
		kfree(shofer);
		*retval = -ENOMEM;
		shofer->rwq = NULL;
		shofer->wwq = NULL;
		return NULL;
	}

	mutex_init(&shofer->lock);

	return shofer;
}

static void shofer_delete(struct shofer_dev *shofer)
{
	cdev_del(&shofer->cdev);

	if(shofer->rwq)
		destroy_workqueue(shofer->rwq);
	if(shofer->wwq)
		destroy_workqueue(shofer->wwq);

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

/* use workqueues to copy data from buffer */
static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
	loff_t *f_pos)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	size_t fifo_len;
	char *buf = NULL;
	struct wq_data wqd; /* reserved on stack, since here we wait */
	struct completion wq_reader;

	if (count == 0)
		return 0;

	spin_lock(&buffer->key); /* prevent timers, tasklets, ... */

	dump_buffer("read-start", shofer, buffer);
	fifo_len = kfifo_len(fifo);
	if (count > fifo_len) /* enough bytes in buffer? */
		count = fifo_len;

	spin_unlock(&buffer->key);

	if (count == 0)
		return 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf){
		klog(KERN_WARNING, "kmalloc failed\n");
		return -ENOMEM;
	}

	/* create a job that will copy data from 'buffer' to 'buf' */
	wqd.buf = buf;
	wqd.len = count;
	wqd.copied = 0;
	wqd.buffer = buffer;
	wqd.op = 0; /* read */
	wqd.wakeup.completion = &wq_reader;

	INIT_WORK(&wqd.work, workqueue_operations);
	init_completion(&wq_reader);

	mutex_lock(&shofer->lock);
	if (!queue_work(shofer->rwq, &wqd.work)) {
		/* not added */
		LOG("work not added to workqueue!");
		retval = -EFAULT;
	}
	mutex_unlock(&shofer->lock);

	if (!retval) {
		wait_for_completion(&wq_reader);
		retval = wqd.copied;
		if (copy_to_user(ubuf, buf, wqd.copied)) {
			klog(KERN_WARNING, "copy_to_user failed\n");
			kfree(buf);
			return -EFAULT;
		}
	}

	spin_lock(&buffer->key);
	dump_buffer("read-end", shofer, buffer);
	spin_unlock(&buffer->key);

	kfree(buf);

	return retval;
}

/* use workqueues to copy data to buffer */
static ssize_t shofer_write(struct file *filp, const char __user *ubuf,
	size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	size_t fifo_free;
	char *buf = NULL;
	struct wq_data wqd; /* reserved on stack, since here we wait */

	if (count == 0)
		return 0;

	spin_lock(&buffer->key);

	dump_buffer("write-start", shofer, buffer);
	fifo_free = kfifo_avail(fifo);
	if (count > fifo_free) /* enough free space in buffer? */
		count = fifo_free; /* don't write all given data */

	spin_unlock(&buffer->key);

	if (count == 0)
		return 0;

	/* first, copy data from user space to 'buf' */
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf){
		klog(KERN_WARNING, "kmalloc failed\n");
		return -ENOMEM;
	}
	if (copy_from_user(buf, ubuf, count)) {
		klog(KERN_WARNING, "copy_from_user failed\n");
		kfree(buf);
		return -EFAULT;
	}
	/* create a job that will copy data from 'buf' into "buffer" */
	wqd.buf = buf;
	wqd.len = count;
	wqd.copied = 0;
	wqd.buffer = buffer;
	wqd.op = 1; /* write */
	wqd.wakeup.queue = &shofer->wqueue;

	INIT_WORK(&wqd.work, workqueue_operations);

	mutex_lock(&shofer->lock);
	if (!queue_work(shofer->wwq, &wqd.work)) {
		/* not added */
		LOG("work not added to workqueue!");
		retval = -EFAULT;
	}
	mutex_unlock(&shofer->lock);
	if (!retval) {
		wait_event(shofer->wqueue, wqd.copied > 0);
		retval = wqd.copied;
	}

	spin_lock(&buffer->key);
	dump_buffer("write-end", shofer, buffer);
	spin_unlock(&buffer->key);

	kfree(buf);

	return retval;
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

static void timer_function(struct timer_list *t)
{
	struct buffer *buffer;
	struct kfifo *fifo;

	buffer = list_first_entry(&buffers_list, struct buffer, list);
	spin_lock(&buffer->key);
	fifo = &buffer->fifo;
	kfifo_put(fifo, 'T');
	spin_unlock(&buffer->key);

	/* reschedule timer for period */
	mod_timer(t, jiffies + msecs_to_jiffies(TIMER_PERIOD));
}

static void workqueue_operations(struct work_struct *work)
{
	struct wq_data *wqd;
	struct buffer *buffer;
	struct kfifo *fifo;

	/* delay work by 500 msec */
	int retval;
	wait_queue_head_t wait;
	init_waitqueue_head (&wait);
	retval = wait_event_interruptible_timeout(wait, 0,
		msecs_to_jiffies(500));

	wqd = container_of(work, struct wq_data, work);
	buffer = wqd->buffer;
	fifo = &buffer->fifo;

	spin_lock(&buffer->key);

	if (wqd->op)
		wqd->copied = kfifo_in(fifo, wqd->buf, wqd->len);
	else
		wqd->copied = kfifo_out(fifo, wqd->buf, wqd->len);

	spin_unlock(&buffer->key);

	if (wqd->op)
		wake_up_all(wqd->wakeup.queue);
	else
		complete(wqd->wakeup.completion);
}

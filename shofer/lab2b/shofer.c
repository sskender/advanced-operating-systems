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

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/log2.h>
#include <linux/ioctl.h>
#include <linux/timer.h>

#include "config.h"

/* Buffer size */
static int buffer_size = BUFFER_SIZE;

/* Some parameters can be given at module load time */
module_param(buffer_size, int, S_IRUGO);
MODULE_PARM_DESC(buffer_size, "Buffer size in bytes, must be a power of 2");

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

struct shofer_dev *input_dev = NULL; /* gets data from user into in_buff */
struct shofer_dev *control_dev = NULL; /* gets commands from user via ioctl */
struct shofer_dev *output_dev = NULL; /* gets data from in_buff to user */
struct buffer *in_buff = NULL, *out_buff = NULL;
static dev_t dev_no = 0;

/* just for passing arguments to timer */
static struct shofer_timer {
	struct timer_list timer;
	struct buffer *in_buff;
	struct buffer *out_buff;
} timer;

/* prototypes */
static struct buffer *buffer_create(size_t, int *);
static void buffer_delete(struct buffer *);
static struct shofer_dev *shofer_create(dev_t, struct file_operations *,
	struct buffer *, struct buffer *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);
static void dump_buffer(char *prefix, struct buffer *b);
static void timer_function(struct timer_list *t);

static int shofer_open_read(struct inode *inode, struct file *filp);
static int shofer_open_write(struct inode *inode, struct file *filp);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);
static long control_ioctl (struct file *, unsigned int, unsigned long);

static struct file_operations input_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open_write,
	.write =    shofer_write
};

static struct file_operations output_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open_read,
	.read =     shofer_read
};

static struct file_operations control_fops = {
	.owner =		THIS_MODULE,
	.open =			shofer_open_read,
	.unlocked_ioctl =	control_ioctl
};

/* init module */
static int __init shofer_module_init(void)
{
	int retval;
	dev_t devno;

	klog(KERN_NOTICE, "Module started initialization");

	/* get device number(s) */
	retval = alloc_chrdev_region(&dev_no, 0, 3, DRIVER_NAME);
	if (retval < 0) {
		klog(KERN_WARNING, "Can't get major device number");
		return retval;
	}

	/* create a buffer */
	/* buffer size must be a power of 2 */
	if (!is_power_of_2(buffer_size))
		buffer_size = roundup_pow_of_two(buffer_size);
	in_buff = buffer_create(buffer_size, &retval);
	out_buff = buffer_create(buffer_size, &retval);
	if (!in_buff || !out_buff)
		goto no_driver;

	/* create devices */
	devno = dev_no;
	input_dev = shofer_create(devno, &input_fops, in_buff, NULL, &retval);
	devno = MKDEV(MAJOR(devno), MINOR(devno) + 1);
	control_dev = shofer_create(devno, &control_fops, in_buff, out_buff, &retval);
	devno = MKDEV(MAJOR(devno), MINOR(devno) + 1);
	output_dev = shofer_create(devno, &output_fops, NULL, out_buff, &retval);
	if (!input_dev || !control_dev || !output_dev)
		goto no_driver;

	/* Create timer */
	timer.in_buff = in_buff;
	timer.out_buff = out_buff;
	timer_setup(&timer.timer, timer_function, 0);
	timer.timer.expires = jiffies + msecs_to_jiffies(TIMER_PERIOD);
	add_timer(&timer.timer);

	klog(KERN_NOTICE, "Module initialized with major=%d", MAJOR(devno));

	return 0;

no_driver:
	cleanup();

	return retval;
}

static void cleanup(void)
{
	if (input_dev)
		shofer_delete(input_dev);
	if (control_dev)
		shofer_delete(control_dev);
	if (output_dev)
		shofer_delete(output_dev);
	if (in_buff)
		buffer_delete(in_buff);
	if (out_buff)
		buffer_delete(out_buff);
	if (dev_no)
		unregister_chrdev_region(dev_no, 3);

	del_timer(&timer.timer);
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
	spin_lock_init(&buffer->key);

	*retval = 0;

	return buffer;
}
static void buffer_delete(struct buffer *buffer)
{
	kfree(buffer);
}

static void dump_buffer(char *prefix, struct buffer *b)
{
	char buf[BUFFER_SIZE];
	size_t copied;

	memset(buf, 0, BUFFER_SIZE);
	copied = kfifo_out_peek(&b->fifo, buf, BUFFER_SIZE);

	LOG("%s:size=%u:contains=%u:buf=%s", prefix,
		kfifo_size(&b->fifo), kfifo_len(&b->fifo), buf);
}

/* Create and initialize a single shofer_dev */
static struct shofer_dev *shofer_create(dev_t dev_no,
	struct file_operations *fops, struct buffer *in_buff,
	struct buffer *out_buff, int *retval)
{
	struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
	if (!shofer){
		*retval = -ENOMEM;
		klog(KERN_WARNING, "kmalloc failed\n");
		return NULL;
	}
	memset(shofer, 0, sizeof(struct shofer_dev));
	shofer->in_buff = in_buff;
	shofer->out_buff = out_buff;

	cdev_init(&shofer->cdev, fops);
	shofer->cdev.owner = THIS_MODULE;
	shofer->cdev.ops = fops;
	*retval = cdev_add (&shofer->cdev, dev_no, 1);
	shofer->dev_no = dev_no;
	if (*retval) {
		klog(KERN_WARNING, "Error (%d) when adding device", *retval);
		kfree(shofer);
		shofer = NULL;
	}

	return shofer;
}

static void shofer_delete(struct shofer_dev *shofer)
{
	cdev_del(&shofer->cdev);
	kfree(shofer);
}

/* open for output_dev and control_dev */
static int shofer_open_read(struct inode *inode, struct file *filp)
{
	struct shofer_dev *shofer;

	shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);
	filp->private_data = shofer;

	if ( (filp->f_flags & O_ACCMODE) != O_RDONLY)
		return -EPERM;

	return 0;
}

/* open for input_dev */
static int shofer_open_write(struct inode *inode, struct file *filp)
{
	/* todo (similar to shofer_open_read) */

	return 0;
}

/* output_dev only */
static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
	loff_t *f_pos)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *out_buff = shofer->out_buff;
	struct kfifo *fifo = &out_buff->fifo;
	unsigned int copied;

	spin_lock(&out_buff->key);

	dump_buffer("out_dev-end:out_buff:", out_buff);

	retval = kfifo_to_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		klog(KERN_WARNING, "kfifo_to_user failed\n");
	else
		retval = copied;

	dump_buffer("out_dev-end:out_buff:", out_buff);

	spin_unlock(&out_buff->key);

	return retval;
}

/* input_dev only */
static ssize_t shofer_write(struct file *filp, const char __user *ubuf,
	size_t count, loff_t *f_pos)
{
	/* todo (similar to read) */

	return count;
}

static long control_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *in_buff = shofer->in_buff;
	struct buffer *out_buff = shofer->out_buff;
	struct kfifo *fifo_in = &in_buff->fifo;
	struct kfifo *fifo_out = &out_buff->fifo;
	char c;
	int got;

	if (!cmd)
		return -EINVAL;

	/* copy cmd bytes from in_buff to out_buff */
	/* todo (similar to timer) */

	return retval;
}

static void timer_function(struct timer_list *t)
{
	struct shofer_timer *timer = container_of(t, struct shofer_timer, timer);
	struct buffer *in_buff = timer->in_buff, *out_buff = timer->out_buff;
	struct kfifo *fifo_in = &in_buff->fifo;
	struct kfifo *fifo_out = &out_buff->fifo;
	char c;
	int got;

	/* get locks on both buffers */
	spin_lock(&out_buff->key);
	spin_lock(&in_buff->key);

	dump_buffer("timer-start:in_buff", in_buff);
	dump_buffer("timer-start:out_buff", out_buff);

	if (kfifo_len(fifo_in) > 0 && kfifo_avail(fifo_out) > 0) {
		got = kfifo_get(fifo_in, &c);
		if (got > 0) {
			got = kfifo_put(fifo_out, c);
			if (got)
				LOG("timer moved '%c' from in to out", c);
			else /* should't happen! */
				klog(KERN_WARNING, "kfifo_put failed\n");
		}
		else { /* should't happen! */
			klog(KERN_WARNING, "kfifo_get failed\n");
		}
	}
	else {
		LOG("timer: nothing in input buffer");
		//for test: put '#' in output buffer
		got = kfifo_put(fifo_out, '#');
	}

	dump_buffer("timer-end:in_buff", in_buff);
	dump_buffer("timer-end:out_buff", out_buff);

	spin_unlock(&in_buff->key);
	spin_unlock(&out_buff->key);

	/* reschedule timer for period */
	mod_timer(t, jiffies + msecs_to_jiffies(TIMER_PERIOD));
}

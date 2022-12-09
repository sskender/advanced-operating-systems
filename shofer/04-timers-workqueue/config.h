/*
 * config.h -- structures, constants, macros
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#pragma once

#define DRIVER_NAME 	"shofer"

#define AUTHOR		"Leonardo Jelenkovic"
#define LICENSE		"Dual BSD/GPL"

#define BUFFER_SIZE	512
#define BUFFER_NUM	6
#define DRIVER_NUM	6

#define TIMER_PERIOD	500 /* each 500 ms */

/* Circular buffer */
struct buffer {
	struct kfifo fifo;
	//struct mutex lock;	/* can't use them in timers; spinlocks instead */
	spinlock_t key;		/* for locking with timers, tasklets, ... */
	struct list_head list;
	int id;			/* id to differentiate buffers in prints */
};

/* Device driver */
struct shofer_dev {
	dev_t dev_no;		/* device number */
	struct buffer *buffer;	/* Pointer to buffer */
	struct cdev cdev;	/* Char device structure */
	struct list_head list;
	int id;			/* id to differentiate drivers in prints */
	struct mutex lock;	/* prevent parallel access */

	struct workqueue_struct *rwq;	/* reader workqueue, one per shofer */
	struct workqueue_struct *wwq;	/* writter workqueue, one per shofer */

	/* for tasks waiting for work in workqueue to be done */
	struct wait_queue_head wqueue;
};

struct wq_data {
	struct work_struct work;
	struct buffer *buffer;
	char *buf;
	size_t len;
	unsigned int copied;
	int op; /* 0 - read, 1- write */
	union {
		struct wait_queue_head *queue;
		struct completion *completion;
	} wakeup;
};


#define klog(LEVEL, format, ...)	\
printk ( LEVEL "[shofer] %d: " format "\n", __LINE__, ##__VA_ARGS__)

//printk ( LEVEL "[shofer]%s:%d]" format "\n", __FILE__, __LINE__, ##__VA_ARGS__)
//printk ( LEVEL "[shofer]%s:%d]" format "\n", __FILE_NAME__, __LINE__, ##__VA_ARGS__)

//#define SHOFER_DEBUG

#ifdef SHOFER_DEBUG
#define LOG(format, ...)	klog(KERN_DEBUG, format,  ##__VA_ARGS__)
#else /* !SHOFER_DEBUG */
#warning Debug not activated
#define LOG(format, ...)
#endif /* SHOFER_DEBUG */

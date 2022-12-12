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

#define BUFFER_SIZE	64

/* Circular buffer */
struct buffer {
	struct kfifo fifo;
	struct mutex lock;	/* prevent parallel access */
};

/* Device driver */
struct shofer_dev {
	dev_t dev_no;		/* device number */
	struct cdev cdev;	/* Char device structure */
	struct buffer *buffer;	/* Pointer to buffer */
};

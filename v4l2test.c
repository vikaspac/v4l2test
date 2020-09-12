/*
 * v4l2test - v4l2 test application
 *
 * Imported include directory from v4l-utils-1.20.0 
 *
 * Forked from yavta --  Yet Another V4L2 Test Application
 *
 * Copyright (C) 2005-2010 Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

/*
 * v4l2test.c
 *
 * 1) Video device init/close functions
 * 2) Video device open and query cap
 * 3) Set the buf type as per capabilities, Print the camera and video attributes
 *
 *
 */

#define __STDC_FORMAT_MACROS

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <linux/videodev2.h>



/* Hash defines  */
#define DEVICE_NAME "/dev/video0"



/* Structs and Variable declarations and definitions */
struct buffer
{
	unsigned int idx;
	unsigned int padding[VIDEO_MAX_PLANES];
	unsigned int size[VIDEO_MAX_PLANES];
	void *mem[VIDEO_MAX_PLANES];
};

struct device
{
	int fd;
	int opened;

	enum v4l2_buf_type type;
	enum v4l2_memory memtype;
	unsigned int nbufs;
	struct buffer *buffers;

	unsigned int width;
	unsigned int height;
	uint32_t buffer_output_flags;
	uint32_t timestamp_type;

	unsigned char num_planes;
	struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];

	void *pattern[VIDEO_MAX_PLANES];
	unsigned int patternsize[VIDEO_MAX_PLANES];

	bool write_data_prefix;
};



/* Static Function declarations */
static void video_init(struct device *dev);
static void video_close(struct device *dev);
static int video_querycap(struct device *dev, unsigned int *capabilities);
static bool video_has_fd(struct device *dev);
static int video_open(struct device *dev, const char *devname);
static int cap_get_buf_type(unsigned int capabilitiesi);
static void video_set_buf_type(struct device *dev, enum v4l2_buf_type type);
static void video_log_status(struct device *dev);




int main()
{

	struct device dev = { 0 };
	int ret = 0;


	int do_log_status = 1;


	/* Video related */
	unsigned int capabilities = V4L2_CAP_VIDEO_CAPTURE;

	enum v4l2_memory memtype = V4L2_MEMORY_MMAP;






	printf("Vikas: main() +\n");

	video_init(&dev);
	printf("Vikas: video_init() DONE\n");

	if (!video_has_fd(&dev)) {
		ret = video_open(&dev, DEVICE_NAME);
		if (ret < 0) {
			printf("Vikas: ERR: video_open() failed\n");
			return 1;
		}
	}
	printf("Vikas: video_open() DONE\n");

	ret = video_querycap(&dev, &capabilities);
	if (ret < 0) {
		printf("Vikas: ERR: video_querycap() failed\n");
		return 1;
	}
	printf("Vikas: video_querycap() DONE\n");

	ret = cap_get_buf_type(capabilities);
	if (ret < 0) {
		printf("Vikas: ERR: cap_get_buf_type() failed\n");
		return 1;
	}

	video_set_buf_type(&dev, ret);

	dev.memtype = memtype;

	if (do_log_status)
		video_log_status(&dev);


	video_close(&dev);
	printf("Vikas: video_close() DONE\n");

	printf("Vikas: main() -\n");
	return 0;
}


/* Static Function Definitions */

static void video_init(struct device *dev)
{
	dev->fd = -1;
	dev->memtype = V4L2_MEMORY_MMAP;
	dev->buffers = NULL;
	dev->type = (enum v4l2_buf_type)-1;
}

static void video_close(struct device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_planes; i++)
		free(dev->pattern[i]);

	free(dev->buffers);
	if (dev->opened)
		close(dev->fd);
}

static int video_querycap(struct device *dev, unsigned int *capabilities)
{
	struct v4l2_capability cap;
	unsigned int caps;
	bool has_video;
	bool has_meta;
	bool has_capture;
	bool has_output;
	bool has_mplane;
	int ret;

	memset(&cap, 0, sizeof cap);
	ret = ioctl(dev->fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0)
		return 0;

	caps = cap.capabilities & V4L2_CAP_DEVICE_CAPS
	     ? cap.device_caps : cap.capabilities;

	has_video = caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			    V4L2_CAP_VIDEO_CAPTURE |
			    V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			    V4L2_CAP_VIDEO_OUTPUT);
	has_meta = caps & (V4L2_CAP_META_CAPTURE |
			   V4L2_CAP_META_OUTPUT);
	has_capture = caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			      V4L2_CAP_VIDEO_CAPTURE |
			      V4L2_CAP_META_CAPTURE);
	has_output = caps & (V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			     V4L2_CAP_VIDEO_OUTPUT |
			     V4L2_CAP_META_OUTPUT);
	has_mplane = caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			     V4L2_CAP_VIDEO_OUTPUT_MPLANE);

	printf("Device `%s' on `%s' (driver '%s') supports%s%s%s%s %s mplanes.\n",
		cap.card, cap.bus_info, cap.driver,
		has_video ? " video," : "",
		has_meta ? " meta-data," : "",
		has_capture ? " capture," : "",
		has_output ? " output," : "",
		has_mplane ? "with" : "without");

	*capabilities = caps;

	return 0;
}

static bool video_has_fd(struct device *dev)
{
	return dev->fd != -1;
}

static int video_open(struct device *dev, const char *devname)
{
	if (video_has_fd(dev)) {
		printf("Can't open device (already open).\n");
		return -1;
	}

	dev->fd = open(devname, O_RDWR);
	if (dev->fd < 0) {
		printf("Error opening device %s: %s (%d).\n", devname,
		       strerror(errno), errno);
		return dev->fd;
	}

	printf("Device %s opened.\n", devname);

	dev->opened = 1;

	return 0;
}

static int cap_get_buf_type(unsigned int capabilities)
{
	if (capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
		printf("cap buf: V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE\n");
		return V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	} else if (capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) {
		printf("cap buf: V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE\n");
		return V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	} else if (capabilities & V4L2_CAP_VIDEO_CAPTURE) {
		printf("cap buf: V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
		return  V4L2_BUF_TYPE_VIDEO_CAPTURE;
	} else if (capabilities & V4L2_CAP_VIDEO_OUTPUT) {
		printf("cap buf: V4L2_BUF_TYPE_VIDEO_OUTPUT\n");
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	} else if (capabilities & V4L2_CAP_META_CAPTURE) {
		printf("cap buf: V4L2_BUF_TYPE_META_CAPTURE\n");
		return V4L2_BUF_TYPE_META_CAPTURE;
	} else if (capabilities & V4L2_CAP_META_OUTPUT) {
		printf("cap buf: V4L2_BUF_TYPE_META_OUTPUT\n");
		return V4L2_BUF_TYPE_META_OUTPUT;
	} else {
		printf("Device supports neither capture nor output.\n");
		return -EINVAL;
	}

	return 0;
}

static void video_set_buf_type(struct device *dev, enum v4l2_buf_type type)
{
	dev->type = type;
}

static void video_log_status(struct device *dev)
{
	ioctl(dev->fd, VIDIOC_LOG_STATUS);
}

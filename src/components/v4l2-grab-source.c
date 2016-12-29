/* ambi-tv: a flexible ambilight clone for embedded linux
 *  Copyright (C) 2013 Georg Kaindl
 *
 *  This file is part of ambi-tv.
 *
 *  ambi-tv is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  ambi-tv is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ambi-tv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "../video-fmt.h"
#include "../log.h"
#include "v4l2-grab-source.h"

#define LOGNAME               "v4l2-grab: "

#define DEFAULT_DEV_NAME      "/dev/video0"
#define DEFAULT_NUM_BUFFERS   4

struct v4l2_grab
{
	struct ambitv_source_component* source_component;
	char* dev_name;
	int req_buffers;
	int crop[4];   // top, right, bottom, left
	int auto_crop_luminance;

	int fd;

	unsigned num_buffers;
	void* buffers;
	int width, height, bytesperline, palnorm;
	enum ambitv_video_format fmt;
};

struct vid_buffer
{
	void* start;
	size_t length;
};

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do
	{
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static int ambitv_v4l2_grab_open_device(struct v4l2_grab* grabber)
{
	int ret = 0;
	struct stat st;

	ret = stat(grabber->dev_name, &st);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to stat '%s' : %d (%s).\n", grabber->dev_name, errno,
				strerror(errno));
		return ret;
	}

	if (!S_ISCHR(st.st_mode))
	{
		ambitv_log(ambitv_log_error, LOGNAME "'%s' is not a device.\n", grabber->dev_name);
		return -ENODEV;
	}

	grabber->fd = open(grabber->dev_name, O_RDWR | O_NONBLOCK, 0);

	if (grabber->fd < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to open '%s': %d (%s).\n", grabber->dev_name, errno,
				strerror(errno));

		ret = -errno;
	}

	return ret;
}

static int ambitv_v4l2_grab_close_device(struct v4l2_grab* grabber)
{
	int ret = 0;

	if (grabber->fd >= 0)
		ret = close(grabber->fd);

	grabber->fd = -1;

	return ret;
}

static void ambitv_v4l2_grab_free_buffers(struct v4l2_grab* grabber)
{
	if (NULL != grabber->buffers)
	{
		free(grabber->buffers);
		grabber->buffers = NULL;
	}
}

static int ambitv_v4l2_grab_init_mmap(struct v4l2_grab* grabber)
{
	int ret;
	struct v4l2_requestbuffers req;

	memset(&req, 0, sizeof(req));

	req.count = grabber->req_buffers;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	ret = xioctl(grabber->fd, VIDIOC_REQBUFS, &req);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to set up memory mapping for '%s'.\n", grabber->dev_name);
		return -ENODEV;
	}

	if (req.count < 2)
	{
		ambitv_log(ambitv_log_error, LOGNAME "insufficient buffer memory for '%s'.\n", grabber->dev_name);
		return -ENOMEM;
	}

	grabber->buffers = calloc(req.count, sizeof(struct vid_buffer));
	if (NULL == grabber->buffers)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to allocate buffer memory.\n");
		return -ENOMEM;
	}

	struct vid_buffer* buffers = (struct vid_buffer*) grabber->buffers;

	for (grabber->num_buffers = 0; grabber->num_buffers < req.count; grabber->num_buffers++)
	{
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = grabber->num_buffers;

		ret = xioctl(grabber->fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0)
		{
			ambitv_log(ambitv_log_error, LOGNAME "failed to query video buffer.\n");
			goto fail_buf;
		}
		buffers[grabber->num_buffers].length = buf.length;
		buffers[grabber->num_buffers].start = mmap(NULL, buf.length,
		PROT_READ | PROT_WRITE,
		MAP_SHARED, grabber->fd, buf.m.offset);

		if (MAP_FAILED == buffers[grabber->num_buffers].start)
		{
			ambitv_log(ambitv_log_error, LOGNAME "failed to set up mmap().\n");
			goto fail_buf;
		}
	}

	return 0;

	fail_buf: ambitv_v4l2_grab_free_buffers(grabber);

	return -ENODEV;
}

static int ambitv_v4l2_grab_init_device(struct v4l2_grab* grabber)
{
	int ret;
	struct v4l2_capability cap;
	struct v4l2_format vid_fmt;
	v4l2_std_id vid_norm;

	ret = xioctl(grabber->fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "'%s' is no v4l2 device.\n", grabber->dev_name);
		return ret;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		ambitv_log(ambitv_log_error, LOGNAME "'%s' is not a video capture device.\n", grabber->dev_name);
		return -ENODEV;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		ambitv_log(ambitv_log_error, LOGNAME "'%s' does not support streaming i/o.\n", grabber->dev_name);
		return -ENODEV;
	}

	vid_norm = (grabber->palnorm)?V4L2_STD_PAL:V4L2_STD_525_60;
	ret = xioctl(grabber->fd, VIDIOC_S_STD, &vid_norm);

	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to set video norm.\n");
		return -EINVAL;
	}
	memset(&vid_fmt, 0, sizeof(vid_fmt));
	vid_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = xioctl(grabber->fd, VIDIOC_G_FMT, &vid_fmt);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to determine video format of '%s'.\n", grabber->dev_name);
		return -EINVAL;
	}

	grabber->width = vid_fmt.fmt.pix.width;
	grabber->height = vid_fmt.fmt.pix.height;
	grabber->bytesperline = vid_fmt.fmt.pix.bytesperline;
	grabber->fmt = v4l2_to_ambitv_video_format(vid_fmt.fmt.pix.pixelformat);

	ambitv_log(ambitv_log_info, LOGNAME "video format: %ux%u (%s %d bpl).\n", vid_fmt.fmt.pix.width, vid_fmt.fmt.pix.height,
			v4l2_string_from_fourcc(vid_fmt.fmt.pix.pixelformat), vid_fmt.fmt.pix.bytesperline);

	return ambitv_v4l2_grab_init_mmap(grabber);
}

static int ambitv_v4l2_grab_uninit_device(struct v4l2_grab* grabber)
{
	unsigned int i;
	int ret;
	struct vid_buffer* buffers = (struct vid_buffer*) grabber->buffers;

	for (i = 0; i < grabber->num_buffers; i++)
	{
		ret = munmap(buffers[i].start, buffers[i].length);
		if (ret < 0)
		{
			ambitv_log(ambitv_log_error, LOGNAME "failed to unmap buffer: %d (%s).\n",
			errno, strerror(errno));
			return -errno;
		}
	}

	ambitv_v4l2_grab_free_buffers(grabber);

	return 0;
}

static int ambitv_v4l2_grab_start_streaming(struct v4l2_grab* grabber)
{
	int i, ret;
	enum v4l2_buf_type type;

	for (i = 0; i < grabber->num_buffers; i++)
	{
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		ret = xioctl(grabber->fd, VIDIOC_QBUF, &buf);
		if (ret < 0)
		{
			ambitv_log(ambitv_log_error, LOGNAME "failed to enqueue a video buffer.\n");
			return -EINVAL;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = xioctl(grabber->fd, VIDIOC_STREAMON, &type);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to start video streaming: %d (%s).\n",
		errno, strerror(errno));
		return -EINVAL;
	}

	return 0;
}

static int ambitv_v4l2_grab_stop_streaming(struct v4l2_grab* grabber)
{
	int ret;
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = xioctl(grabber->fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to stop video streaming: %d (%s).\n",
		errno, strerror(errno));
		ret = -errno;
	}

	return ret;
}

static int ambitv_v4l2_grab_read_frame(struct v4l2_grab* grabber)
{
	struct v4l2_buffer buf;
	int ret, ewidth, eheight, ebpl, auto_crop[4] =
	{ 0, 0, 0, 0 };
	unsigned char* eframe;
	struct vid_buffer* buffers = (struct vid_buffer*) grabber->buffers;

	memset(&buf, 0, sizeof(buf));

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	ret = xioctl(grabber->fd, VIDIOC_DQBUF, &buf);
	if (ret < 0)
	{
		switch (errno)
		{
		case EIO:
		case EAGAIN:
			return 0;

		default:
			ambitv_log(ambitv_log_error, LOGNAME "failed to dequeue a frame: %d (%s).\n",
			errno, strerror(errno));
			return -errno;

		}
	}

	if (buf.index >= grabber->num_buffers)
	{
		ambitv_log(ambitv_log_error, LOGNAME "buffer index is out of expected range.\n");
		return -EINVAL;
	}

	if (grabber->auto_crop_luminance >= 0
			&& ambitv_video_fmt_detect_crop_for_frame(&auto_crop[0], grabber->auto_crop_luminance,
					buffers[buf.index].start, grabber->width, grabber->height, grabber->bytesperline, grabber->fmt) < 0)
		memset(auto_crop, 0, sizeof(int) * 4);

	// apply crop
	switch (grabber->fmt)
	{
	case ambitv_video_format_yuyv:
	{
		int cx = (grabber->crop[3] & ~1) + auto_crop[3], cy = grabber->crop[0] + auto_crop[0];

		ebpl = grabber->bytesperline ? grabber->bytesperline : 2 * grabber->width;
		eframe = buffers[buf.index].start + cy * ebpl + cx * 2;
		ewidth = grabber->width - grabber->crop[1] - grabber->crop[3] - auto_crop[1] - auto_crop[3];
		eheight = grabber->height - grabber->crop[0] - grabber->crop[2] - auto_crop[0] - auto_crop[2];
		break;
	}

	default:
	{
		eframe = NULL;
		ewidth = 0;
		eheight = 0;
		ebpl = 0;
		break;
	}
	}

	ambitv_source_component_distribute_to_active_processors(grabber->source_component, eframe, ewidth, eheight, ebpl,
			grabber->fmt);
	ret = xioctl(grabber->fd, VIDIOC_QBUF, &buf);
	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to enqueue a frame: %d (%s).\n",
		errno, strerror(errno));
	}

	return ret;
}

static int ambitv_v4l2_grab_capture_loop_iteration(struct ambitv_source_component* grabber)
{
	int ret = 0;
	fd_set fds;
	struct timeval tv;

	struct v4l2_grab* grab_priv = (struct v4l2_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	FD_ZERO(&fds);
	FD_SET(grab_priv->fd, &fds);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	ret = select(grab_priv->fd + 1, &fds, NULL, NULL, &tv);

	if (ret < 0)
	{
		if (EINTR == errno)
			ret = 0;

		ambitv_log(ambitv_log_error, LOGNAME "failed to select() a frame: %d (%s).\n",
		errno, strerror(errno));
	}
	else if (0 == ret)
	{
		ambitv_log(ambitv_log_error, LOGNAME "select() timed out.\n");
	}

	if (ret > 0)
	{
		ret = ambitv_v4l2_grab_read_frame(grab_priv);
	}

	return ret;
}

int ambitv_v4l2_grab_start(struct ambitv_source_component* grabber)
{
	int ret = 0;

	struct v4l2_grab* grab_priv = (struct v4l2_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	if (grab_priv->fd > -1)
	{
		ambitv_log(ambitv_log_warn, LOGNAME "grabber is already running.\n");
		return -1;
	}

	ret = ambitv_v4l2_grab_open_device(grab_priv);
	if (ret < 0)
		goto fail_open;

	ret = ambitv_v4l2_grab_init_device(grab_priv);
	if (ret < 0)
		goto fail_init;

	ret = ambitv_v4l2_grab_start_streaming(grab_priv);
	if (ret < 0)
		goto fail_streaming;

	return ret;

	fail_streaming: ambitv_v4l2_grab_uninit_device(grab_priv);

	fail_init: ambitv_v4l2_grab_close_device(grab_priv);

	fail_open: return ret;
}

int ambitv_v4l2_grab_stop(struct ambitv_source_component* grabber)
{
	int ret = 0;

	struct v4l2_grab* grab_priv = (struct v4l2_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	if (grab_priv->fd < 0)
	{
		ambitv_log(ambitv_log_warn, LOGNAME "grabber is not running and can't be stopped.\n");
		return -1;
	}

	ret = ambitv_v4l2_grab_stop_streaming(grab_priv);
	if (ret < 0)
		goto fail_return;

	ret = ambitv_v4l2_grab_uninit_device(grab_priv);
	if (ret < 0)
		goto fail_return;

	ret = ambitv_v4l2_grab_close_device(grab_priv);

	fail_return: return ret;
}

static int ambitv_v4l2_grab_configure(struct ambitv_source_component* grabber, int argc, char** argv)
{
	int c, ret = 0;

	struct v4l2_grab* grab_priv = (struct v4l2_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	static struct option lopts[] =
	{
	{ "video-device", required_argument, 0, 'd' },
	{ "video-norm", required_argument, 0, 'n' },
	{ "buffers", required_argument, 0, 'b' },
	{ "crop-top", required_argument, 0, '0' },
	{ "crop-right", required_argument, 0, '1' },
	{ "crop-bottom", required_argument, 0, '2' },
	{ "crop-left", required_argument, 0, '3' },
	{ "autocrop-luminance-threshold", required_argument, 0, 'a' },
	{ NULL, 0, 0, 0 } };

	while (1)
	{
		c = getopt_long(argc, argv, "", lopts, NULL);

		if (c < 0)
			break;

		switch (c)
		{
		case 'd':
		{
			if (NULL != optarg)
			{
				if (NULL != grab_priv->dev_name)
					free(grab_priv->dev_name);

				grab_priv->dev_name = strdup(optarg);
			}
			break;
		}

		case 'n':
		{
			if (NULL != optarg)
			{
				grab_priv->palnorm = (strstr(optarg, "PAL") != NULL);
			}
			break;
		}

		case 'b':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr)
				{
					grab_priv->req_buffers = (int) nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
							optarg);
					return -1;
				}
			}

			break;
		}

		case '0':
		case '1':
		case '2':
		case '3':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					grab_priv->crop[c - '0'] = (int) nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
							optarg);
					return -1;
				}
			}

			break;
		}

		case 'a':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr)
				{
					grab_priv->auto_crop_luminance = (int) nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
							optarg);
					return -1;
				}
			}

			break;
		}

		default:
			break;
		}
	}

	if (optind < argc)
	{
		ambitv_log(ambitv_log_error, LOGNAME "extraneous configuration argument: '%s'.\n", argv[optind]);
		ret = -1;
	}

	return ret;
}

static void ambitv_v4l2_grab_print_configuration(struct ambitv_source_component* component)
{
	struct v4l2_grab* grab_priv = (struct v4l2_grab*) component->priv;

	ambitv_log(ambitv_log_info, "\tdevice name:              %s\n"
			"\tvideo-norm:               %s\n"
			"\tbuffers:                  %d\n"
			"\tcrop-top:                 %d\n"
			"\tcrop-right:               %d\n"
			"\tcrop-bottom:              %d\n"
			"\tcrop-left:                %d\n"
			"\tauto-crop luma threshold: %d\n", grab_priv->dev_name, (grab_priv->palnorm)?"PAL":"NTSC",grab_priv->req_buffers, grab_priv->crop[0],
			grab_priv->crop[1], grab_priv->crop[2], grab_priv->crop[3], grab_priv->auto_crop_luminance);
}

void ambitv_v4l2_grab_free(struct ambitv_source_component* component)
{
	struct v4l2_grab* grab_priv = (struct v4l2_grab*) component->priv;

	if (NULL != grab_priv)
	{
		if (NULL != grab_priv->dev_name)
			free(grab_priv->dev_name);

		ambitv_v4l2_grab_free_buffers(grab_priv);
		ambitv_v4l2_grab_close_device(grab_priv);

		free(grab_priv);
		component->priv = NULL;
	}
}

struct ambitv_source_component*
ambitv_v4l2_grab_create(const char* name, int argc, char** argv)
{
	struct ambitv_source_component* grabber = ambitv_source_component_create(name);

	if (NULL != grabber)
	{
		struct v4l2_grab* grab_priv = (struct v4l2_grab*) malloc(sizeof(struct v4l2_grab));
		if (NULL == grab_priv)
			goto errReturn;

		grabber->priv = grab_priv;

		grab_priv->dev_name = strdup(DEFAULT_DEV_NAME);
		grab_priv->req_buffers = DEFAULT_NUM_BUFFERS;
		grab_priv->fd = -1;
		grab_priv->auto_crop_luminance = -1;

		grab_priv->source_component = grabber;

		if (ambitv_v4l2_grab_configure(grabber, argc, argv) < 0)
			goto errReturn;

		grabber->f_print_configuration = ambitv_v4l2_grab_print_configuration;
		grabber->f_start_source = ambitv_v4l2_grab_start;
		grabber->f_stop_source = ambitv_v4l2_grab_stop;
		grabber->f_run = ambitv_v4l2_grab_capture_loop_iteration;
		grabber->f_free_priv = ambitv_v4l2_grab_free;
	}

	return grabber;

	errReturn: ambitv_source_component_free(grabber);

	return NULL;
}

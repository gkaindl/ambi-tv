/* word-clock: a flexible ambilight clone for embedded linux
 *  Copyright (C) 2013 Georg Kaindl
 *
 *  This file is part of word-clock.
 *
 *  word-clock is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  word-clock is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with word-clock.  If not, see <http://www.gnu.org/licenses/>.
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
#include <alsa/asoundlib.h>

#include "../log.h"
#include "audio-grab-source.h"

#define LOGNAME               "audio-grab: "

#define DEFAULT_DEV_NAME      "hw:0,0"

struct audio_grab
{
	struct wordclock_source_component* source_component;
	char* dev_name;
	int format;
	int radj;
	int ladj;
	int size;
	unsigned int rate;
	void *buffer;
	int shared[SAMPLES];
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
};

static int wordclock_audio_grab_open_device(struct audio_grab* grabber)
{
	int ret = 0;

	// alsa: open device to capture audio
	if ((errno = snd_pcm_open(&grabber->handle, grabber->dev_name, SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		wordclock_log(wordclock_log_error, LOGNAME "failed to open '%s': %d (%s).\n", grabber->dev_name, errno,
				snd_strerror(errno));

		ret = -errno;
	}

	return ret;
}

static int wordclock_audio_grab_close_device(struct audio_grab* grabber)
{
	int ret = 0;

	if (grabber->handle)
		ret = snd_pcm_close(grabber->handle);

	grabber->handle = NULL;

	return ret;
}

static void wordclock_audio_grab_free_buffers(struct audio_grab* grabber)
{
	if (NULL != grabber->buffer)
	{
		free(grabber->buffer);
		grabber->buffer = NULL;
	}
}

static int wordclock_audio_grab_init_device(struct audio_grab* grabber)
{
	int ret = -1, dir;
	unsigned int val;

	if ((ret = wordclock_audio_grab_open_device(grabber)) >= 0)
	{
		snd_pcm_hw_params_alloca(&grabber->params); //assembling params
		snd_pcm_hw_params_any(grabber->handle, grabber->params); //setting defaults or something
		snd_pcm_hw_params_set_access(grabber->handle, grabber->params, SND_PCM_ACCESS_RW_INTERLEAVED); //interleaved mode right left right left
		snd_pcm_hw_params_set_format(grabber->handle, grabber->params, SND_PCM_FORMAT_S16_LE); //trying to set 16bit
		snd_pcm_hw_params_set_channels(grabber->handle, grabber->params, 2); //assuming stereo
		val = 44100;
		snd_pcm_hw_params_set_rate_near(grabber->handle, grabber->params, &val, &dir); //trying 44100 rate
		grabber->frames = 256;
		snd_pcm_hw_params_set_period_size_near(grabber->handle, grabber->params, &grabber->frames, &dir); //number of frames pr read

		ret = snd_pcm_hw_params(grabber->handle, grabber->params); //attempting to set params
		if (ret < 0)
		{
			return ret;
		}

		snd_pcm_hw_params_get_format(grabber->params, (snd_pcm_format_t *) &val); //getting actual format
		//converting result to number of bits
		if (val < 6)
			grabber->format = 16;
		else if (val > 5 && val < 10)
			grabber->format = 24;
		else if (val > 9)
			grabber->format = 32;

		snd_pcm_hw_params_get_rate(grabber->params, &grabber->rate, &dir); //getting rate

		snd_pcm_hw_params_get_period_size(grabber->params, &grabber->frames, &dir);
		snd_pcm_hw_params_get_period_time(grabber->params, &val, &dir);

		grabber->size = grabber->frames * (grabber->format / 8) * 2; // frames * bits/8 * 2 channels
		grabber->buffer = malloc(grabber->size);
		grabber->radj = grabber->format / 4; //adjustments for interleaved
		grabber->ladj = grabber->format / 8;
	}
	return ret;
}

static int wordclock_audio_grab_uninit_device(struct audio_grab* grabber)
{

	wordclock_audio_grab_free_buffers(grabber);

	return 0;
}

static int wordclock_audio_grab_start_streaming(struct audio_grab* grabber)
{
	return 0;
}

static int wordclock_audio_grab_stop_streaming(struct audio_grab* grabber)
{
	return 0;
}

static int wordclock_audio_grab_read_frame(struct audio_grab* grabber)
{
	int ret, tempr, templ;
	int i, n, o, lo;
	signed char *buffer = (signed char*) grabber->buffer;

	ret = snd_pcm_readi(grabber->handle, grabber->buffer, grabber->frames);
	if (ret == -EPIPE)
	{
		/* EPIPE means overrun */
		wordclock_log(wordclock_log_warn, LOGNAME "overrun occurred\n");
		snd_pcm_prepare(grabber->handle);
	}
	else if (ret < 0)
	{
		wordclock_log(wordclock_log_warn, LOGNAME "error from read: %s\n", snd_strerror(ret));
	}
	else if (ret != (int) grabber->frames)
	{
		wordclock_log(wordclock_log_warn, LOGNAME "short read, read %d %d frames\n", ret, (int) grabber->frames);
	}

	//sorting out one channel and only biggest octet
	n = 0; //frame counter
	o = 0;
	for (i = 0; i < grabber->size; i = i + (grabber->ladj) * 2)
	{

		//first channel
		tempr = ((buffer[i + (grabber->radj) - 1] << 2)); //using the 10 upper bits this would give me a vert res of 1024, enough...

		lo = ((buffer[i + (grabber->radj) - 2] >> 6));
		if (lo < 0)
			lo = abs(lo) + 1;
		if (tempr >= 0)
			tempr = tempr + lo;
		if (tempr < 0)
			tempr = tempr - lo;

		//other channel
		templ = (buffer[i + (grabber->ladj) - 1] << 2);
		lo = (buffer[i + (grabber->ladj) - 2] >> 6);
		if (lo < 0)
			lo = abs(lo) + 1;
		if (templ >= 0)
			templ = templ + lo;
		else
			templ = templ - lo;

		//adding channels and storing it in the buffer
		grabber->shared[o] = (tempr + templ) / 2;
		o++;
		if (o == SAMPLES - 1)
			o = 0;

		n++;
	}

	wordclock_source_component_distribute_to_active_processors(grabber->source_component, (void*) &grabber->shared,
	SAMPLES, grabber->rate, 0, //exval,
			0 //cmd
			);

	return ret;
}

static int wordclock_audio_grab_capture_loop_iteration(struct wordclock_source_component* grabber)
{
	int ret = 0;

	struct audio_grab* grab_priv = (struct audio_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	ret = wordclock_audio_grab_read_frame(grab_priv);

	return ret;
}

int wordclock_audio_grab_start(struct wordclock_source_component* grabber)
{
	int ret = 0;

	struct audio_grab* grab_priv = (struct audio_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	if (grab_priv->handle != NULL)
	{
		wordclock_log(wordclock_log_warn, LOGNAME "grabber is already running.\n");
		return -1;
	}
	/*
	 ret = wordclock_audio_grab_open_device(grab_priv);
	 if (ret < 0)
	 goto fail_open;
	 */
	ret = wordclock_audio_grab_init_device(grab_priv);
	if (ret < 0)
		goto fail_init;

	ret = wordclock_audio_grab_start_streaming(grab_priv);
	if (ret < 0)
		goto fail_streaming;

	return ret;

	fail_streaming: wordclock_audio_grab_uninit_device(grab_priv);

	fail_init: wordclock_audio_grab_close_device(grab_priv);

//	fail_open: return ret;
	return ret;
}

int wordclock_audio_grab_stop(struct wordclock_source_component* grabber)
{
	int ret = 0;

	struct audio_grab* grab_priv = (struct audio_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	if (grab_priv->handle == NULL)
	{
		wordclock_log(wordclock_log_warn,
		LOGNAME "grabber is not running and can't be stopped.\n");
		return -1;
	}

	ret = wordclock_audio_grab_stop_streaming(grab_priv);
	if (ret < 0)
		goto fail_return;

	ret = wordclock_audio_grab_uninit_device(grab_priv);
	if (ret < 0)
		goto fail_return;

	ret = wordclock_audio_grab_close_device(grab_priv);

	fail_return: return ret;
}

static int wordclock_audio_grab_configure(struct wordclock_source_component* grabber, int argc, char** argv)
{
	int c, ret = 0;

	struct audio_grab* grab_priv = (struct audio_grab*) grabber->priv;
	if (NULL == grab_priv)
		return -1;

	static struct option lopts[] =
	{
	{ "audio-device", required_argument, 0, 'd' },
	{ NULL, 0, 0, 0 } };

	while (1)
	{
		c = getopt_long(argc, argv, "", lopts, NULL);

		if (c < 0)
			break;

		switch (c)
		{
		case 'd':
			if (NULL != optarg)
			{
				if (NULL != grab_priv->dev_name)
					free(grab_priv->dev_name);

				grab_priv->dev_name = strdup(optarg);
			}
			break;
		}

		if (optind < argc)
		{
			wordclock_log(wordclock_log_error,
			LOGNAME "extraneous configuration argument: '%s'.\n", argv[optind]);
			ret = -1;
		}

	}
	return ret;
}

static void wordclock_audio_grab_print_configuration(struct wordclock_source_component* component)
{
	struct audio_grab* grab_priv = (struct audio_grab*) component->priv;

	wordclock_log(wordclock_log_info, "\tdevice name:              %s\n", grab_priv->dev_name);
}

void wordclock_audio_grab_free(struct wordclock_source_component* component)
{
	struct audio_grab* grab_priv = (struct audio_grab*) component->priv;

	if (NULL != grab_priv)
	{
		if (NULL != grab_priv->dev_name)
			free(grab_priv->dev_name);

		wordclock_audio_grab_free_buffers(grab_priv);
		wordclock_audio_grab_close_device(grab_priv);

		free(grab_priv);
		component->priv = NULL;
	}
}

struct wordclock_source_component*
wordclock_audio_grab_create(const char* name, int argc, char** argv)
{
	struct wordclock_source_component* grabber = wordclock_source_component_create(name);

	if (NULL != grabber)
	{
		struct audio_grab* grab_priv = (struct audio_grab*) malloc(sizeof(struct audio_grab));
		if (NULL == grab_priv)
			goto errReturn;

		grabber->priv = grab_priv;

		grab_priv->dev_name = strdup(DEFAULT_DEV_NAME);
		grab_priv->handle = NULL;
		grab_priv->format = -1;

		grab_priv->source_component = grabber;

		if (wordclock_audio_grab_configure(grabber, argc, argv) < 0)
			goto errReturn;

		grabber->f_print_configuration = wordclock_audio_grab_print_configuration;
		grabber->f_start_source = wordclock_audio_grab_start;
		grabber->f_stop_source = wordclock_audio_grab_stop;
		grabber->f_run = wordclock_audio_grab_capture_loop_iteration;
		grabber->f_free_priv = wordclock_audio_grab_free;
	}

	return grabber;

	errReturn: wordclock_source_component_free(grabber);

	return NULL;
}

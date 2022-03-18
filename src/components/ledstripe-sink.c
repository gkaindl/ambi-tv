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

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "ledstripe-sink.h"

#include "../pwm_dev.h"
#include "../dma.h"
#include "../util.h"
#include "../log.h"
#include "../color.h"
#include "../parse-conf.h"

#define DEFAULT_DEV_NAME         "/dev/spidev0.0"
#define DEFAULT_SPI_SPEED        2500000
#define DEFAULT_BRIGHTNESS		 100
#define DEFAULT_GAMMA_R          1.66   
#define DEFAULT_GAMMA_G          1.6   
#define DEFAULT_GAMMA_B          1.65   
#define LOGNAME                  "ledstripe: "

enum
{
	LED_TYPE_LPD880x, LED_TYPE_WS280x, LED_TYPE_WS281x, LED_TYPE_SK6812, LED_TYPE_APA10x, NUM_LED_TYPES
};

static const char LED_TYPES[NUM_LED_TYPES][8] =
{ "LPD880x", "WS280x", "WS281x", "SK6812", "APA10x" };
static const char wordclock_ledstripe_spidev_mode = 0;
static const char wordclock_ledstripe_spidev_bits = 8;
static const char wordclock_ledstripe_spidev_lsbf = 0;

static const char scol[][6] =
{ "red", "green", "blue" };

struct wordclock_ledstripe_priv
{
	char* dev_name;
	int fd, dev_speed, dev_type, dev_pin, dev_inverse, num_leds, actual_num_leds, out_len, bytes_pp, use_spi, use_8bit,
			use_leader, use_trailer;
	int led_len, led_str;   // top, bottom, left, right
	unsigned char* out;
	unsigned char** bbuf;
	int num_bbuf, bbuf_idx;
	int brightness, intensity[3];
	double gamma[3];      // RGB gamma, not GRB!
	unsigned char* gamma_lut[3];  // also RGB
};

static int wordclock_ledstripe_map_output_to_point(struct wordclock_sink_component* component, int output, int width,
		int height, int* x, int* y)
{
	int ret = -1, px, py;
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	if (output < ledstripe->num_leds)
	{
		ret = 0;
		py = output / NUM_COLS;
		px = output % NUM_ROWS;
		if(py & 0x01)
			px = NUM_COLS - px -1;

		*x = (int) CONSTRAIN((float)px * ((float)width) / (float)NUM_COLS, 0, width);
		*y = (int) CONSTRAIN((float)py * ((float)height) / (float)NUM_ROWS, 0, height);
	}
	else
	{
		*x = *y = -1;
	}

	return ret;
}

static int wordclock_ledstripe_commit_outputs(struct wordclock_sink_component* component)
{
	int ret = -1;
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	if (ledstripe->use_spi)
	{
		if (ledstripe->fd >= 0)
		{
			ret = write(ledstripe->fd, ledstripe->out, ledstripe->out_len);

			if (ret != ledstripe->out_len)
			{
				if (ret <= 0)
					ret = -errno;
				else
					ret = -ret;
			}
			else
				ret = 0;
		}
	}
	else
	{
		volatile uint8_t *pwm_raw = pwm_dev.device->pwm_raw;
		int maxcount = ledstripe->num_leds;
		int bitpos = 31;
		int i, j, k, l, chan;

		for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)         // Channel
		{
			pwm_dev_channel_t *channel = &pwm_dev.channel[chan];
			int wordpos = chan;

			for (i = 0; i < channel->count; i++)                // Led
			{
				for (j = 0; j < 3; j++)        // Color
				{
					for (k = 7; k >= 0; k--)                   // Bit
					{
						uint8_t symbol = SYMBOL_LOW;

						if (ledstripe->out[ledstripe->bytes_pp * i + j] & (1 << k))
						{
							symbol = SYMBOL_HIGH;
						}

						if (channel->invert)
						{
							symbol = ~symbol & 0x7;
						}

						for (l = 2; l >= 0; l--)               // Symbol
						{
							uint32_t *wordptr = &((uint32_t *) pwm_raw)[wordpos];

							*wordptr &= ~(1 << bitpos);
							if (symbol & (1 << l))
							{
								*wordptr |= (1 << bitpos);
							}

							bitpos--;
							if (bitpos < 0)
							{
								// Every other word is on the same channel
								wordpos += 2;

								bitpos = 31;
							}
						}
					}
				}
			}
		}

		// Ensure the CPU data cache is flushed before the DMA is started.
		__clear_cache((char *) pwm_raw, (char *) &pwm_raw[PWM_BYTE_COUNT(maxcount, pwm_dev.freq) + TRAILING_LEDS]);

		// Wait for any previous DMA operation to complete.
		if (pwm_dev_wait(&pwm_dev))
		{
			return -1;
		}

		dma_start(&pwm_dev);
	}

	if (ledstripe->num_bbuf)
		ledstripe->bbuf_idx = (ledstripe->bbuf_idx + 1) % ledstripe->num_bbuf;

	return ret;
}

static int wordclock_ledstripe_set_output_to_rgb(struct wordclock_sink_component* component, int idx, int r, int g,
		int b)
{
	int ret = -1, *outp = NULL, i, *rgb[] =
	{ &r, &g, &b }, lidx;
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;
	unsigned char *bptr;

	if (idx >= wordclock_special_sinkcommand_brightness)
	{
		switch (idx)
		{
		case wordclock_special_sinkcommand_brightness:
			if (g)
				ret = ledstripe->brightness;
			else
			{
				ledstripe->brightness = r;
				wordclock_log(wordclock_log_info, LOGNAME "brightness was set to %d%%\n", r);
				ret = 0;
			}
			break;

		case wordclock_special_sinkcommand_intensity_red:
		case wordclock_special_sinkcommand_intensity_green:
		case wordclock_special_sinkcommand_intensity_blue:
		{
			idx -= wordclock_special_sinkcommand_intensity_red;
			if (g)
				ret = ledstripe->intensity[idx];
			else
			{
				ledstripe->intensity[idx] = r / 100;
				wordclock_log(wordclock_log_info, LOGNAME "intensity-%s was set to %d%%\n", scol[idx],
						ledstripe->intensity[idx]);
				ret = 0;
			}
		}
			break;
		}
		return ret;
	}

	lidx = idx;
	outp = &lidx;

	if (NULL != outp)
	{
		int ii = *outp;

		if (ii >= 0)
		{
			if (ledstripe->num_bbuf)
			{
				unsigned char* acc = ledstripe->bbuf[ledstripe->bbuf_idx];

				acc[3 * ii] = g;
				acc[3 * ii + 1] = r;
				acc[3 * ii + 2] = b;

				r = g = b = 0;

				for (i = 0; i < ledstripe->num_bbuf; i++)
				{
					g += ledstripe->bbuf[i][3 * ii];
					r += ledstripe->bbuf[i][3 * ii + 1];
					b += ledstripe->bbuf[i][3 * ii + 2];
				}

				g /= ledstripe->num_bbuf;
				r /= ledstripe->num_bbuf;
				b /= ledstripe->num_bbuf;
			}
		}
		else
			ii = -ii - 1;
		for (i = 0; i < 3; i++)
		{
			*rgb[i] =
					(unsigned char) (((((int) *rgb[i] * ledstripe->brightness) / 100) * ledstripe->intensity[i]) / 100);
			if (ledstripe->gamma_lut[i])
				*rgb[i] = wordclock_color_map_with_lut(ledstripe->gamma_lut[i], *rgb[i]);
		}

		bptr = ledstripe->out + (ledstripe->bytes_pp * ii);
		bptr += ledstripe->use_leader;

		switch (ledstripe->dev_type)
		{
		case LED_TYPE_LPD880x:
			*bptr++ = g >> 1 | 0x80;
			*bptr++ = r >> 1 | 0x80;
			*bptr = b >> 1 | 0x80;
			break;

		case LED_TYPE_WS280x:
			*bptr++ = r;
			*bptr++ = g;
			*bptr = b;
			break;

		case LED_TYPE_WS281x:
		case LED_TYPE_SK6812:
			*bptr++ = g;
			*bptr++ = r;
			*bptr = b;
			break;

		case LED_TYPE_APA10x:
			*bptr++ = 0xFF;
			*bptr++ = g;
			*bptr++ = r;
			*bptr = b;
			break;
		}
		ret = 0;
	}

	return ret;
}

static void wordclock_ledstripe_clear_leds(struct wordclock_sink_component* component)
{
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	if (NULL != ledstripe->out && ((!ledstripe->use_spi) || (ledstripe->fd >= 0)))
	{
		int i;

		switch (ledstripe->dev_type)
		{
		case LED_TYPE_APA10x:
		{
			unsigned long *outp = (unsigned long*) (ledstripe->out + ledstripe->use_leader);

			for (i = 0; i < ledstripe->num_leds; i++)
			{
				*(outp++) = 0xE0;
			}
		}
			break;

		case LED_TYPE_LPD880x:
			memset(ledstripe->out, 0x80, ledstripe->out_len);
			break;

		default:
			memset(ledstripe->out, 0x00, ledstripe->out_len);
			break;
		}
		// send 3 times, in case there's noise on the line,
		// so that all LEDs will definitely be off afterwards.
		for (i = 0; i < 3; i++)
		{
			wordclock_ledstripe_commit_outputs(component);
		}
	}
}

static int wordclock_ledstripe_num_outputs(struct wordclock_sink_component* component)
{
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	return ledstripe->num_leds;
}

static int wordclock_ledstripe_start(struct wordclock_sink_component* component)
{
	int ret = 0;
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	if (ledstripe->use_spi)
	{
		if (ledstripe->fd < 0)
		{
			ledstripe->fd = open(ledstripe->dev_name, O_WRONLY);
			if (ledstripe->fd < 0)
			{
				ret = ledstripe->fd;
				wordclock_log(wordclock_log_error,
				LOGNAME "failed to open device '%s' : %d (%s).\n", ledstripe->dev_name, errno, strerror(errno));
				goto errReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_MODE, &wordclock_ledstripe_spidev_mode);
			if (ret < 0)
			{
				wordclock_log(wordclock_log_error,
				LOGNAME "failed to spidev mode on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_BITS_PER_WORD, &wordclock_ledstripe_spidev_bits);
			if (ret < 0)
			{
				wordclock_log(wordclock_log_error,
				LOGNAME "failed to spidev bits-per-word on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_MAX_SPEED_HZ, &ledstripe->dev_speed);
			if (ret < 0)
			{
				wordclock_log(wordclock_log_error,
				LOGNAME "failed to spidev baudrate on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_LSB_FIRST, &wordclock_ledstripe_spidev_lsbf);
			if (ret < 0)
			{
				wordclock_log(wordclock_log_error,
				LOGNAME "failed to spidev bitorder on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}
		}
	}
	else
	{
		if (stristr(ledstripe->dev_name, "DMA") != NULL)
			sscanf(ledstripe->dev_name + 3, "%d", &pwm_dev.dmanum);
		pwm_dev.freq = ledstripe->dev_speed;
		pwm_dev.channel[0].leds = ledstripe->out;
		pwm_dev.channel[0].count = ledstripe->actual_num_leds;
		pwm_dev.channel[0].gpionum = ledstripe->dev_pin;
		pwm_dev.channel[0].invert = ledstripe->dev_inverse;
		pwm_dev_init(&pwm_dev);
	}
	wordclock_ledstripe_clear_leds(component);

	return ret;

	closeReturn: close(ledstripe->fd);
	ledstripe->fd = -1;
	errReturn: return ret;
}

static int wordclock_ledstripe_stop(struct wordclock_sink_component* component)
{
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	wordclock_ledstripe_clear_leds(component);

	if (ledstripe->use_spi)
	{
		if (ledstripe->fd >= 0)
		{
			close(ledstripe->fd);
			ledstripe->fd = -1;
		}
	}
	else
	{
		pwm_dev_fini(&pwm_dev);
	}

	return 0;
}

static int wordclock_ledstripe_configure(struct wordclock_sink_component* component, int argc, char** argv)
{
	int c, ret = 0;
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	ledstripe->num_leds = NUM_ROWS * NUM_COLS;
	ledstripe->actual_num_leds = ledstripe->num_leds;
	ledstripe->led_str = -1;

	if (NULL == ledstripe)
		return -1;

	static struct option lopts[] =
	{
	{ "led-device", required_argument, 0, 'd' },
	{ "dev-speed-hz", required_argument, 0, 'h' },
	{ "dev-type", required_argument, 0, 't' },
	{ "dev-pin", required_argument, 0, 'p' },
	{ "dev-inverse", required_argument, 0, 'i' },
	{ "overall-brightness", required_argument, 0, 'o' },
	{ "intensity-red", required_argument, 0, '7' },
	{ "intensity-green", required_argument, 0, '8' },
	{ "intensity-blue", required_argument, 0, '9' },
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
				if (NULL != ledstripe->dev_name)
					free(ledstripe->dev_name);

				ledstripe->dev_name = strdup(optarg);
			}
			break;
		}

		case 't':
		{
			if (NULL != optarg)
			{
				if (strstr(optarg, "LPD880x"))
				{
					ledstripe->dev_type = LED_TYPE_LPD880x;
					ledstripe->use_spi = 1;
					ledstripe->use_leader = 0;
					ledstripe->use_trailer = 1;
					ledstripe->use_8bit = 0;
					ledstripe->bytes_pp = 3;
				}
				else if (strstr(optarg, "WS280x"))
				{
					ledstripe->dev_type = LED_TYPE_WS280x;
					ledstripe->use_spi = 1;
					ledstripe->use_leader = 0;
					ledstripe->use_trailer = 0;
					ledstripe->use_8bit = 1;
					ledstripe->bytes_pp = 3;
				}
				else if (strstr(optarg, "WS281x"))
				{
					ledstripe->dev_type = LED_TYPE_WS281x;
					ledstripe->use_spi = 0;
					ledstripe->use_leader = 0;
					ledstripe->use_trailer = 0;
					ledstripe->use_8bit = 1;
					ledstripe->bytes_pp = 3;
				}
				else if (strstr(optarg, "SK6812"))
				{
					ledstripe->dev_type = LED_TYPE_SK6812;
					ledstripe->use_spi = 0;
					ledstripe->use_leader = 0;
					ledstripe->use_trailer = 0;
					ledstripe->use_8bit = 1;
					ledstripe->bytes_pp = 3;
				}
				else if (strstr(optarg, "APA10x"))
				{
					ledstripe->dev_type = LED_TYPE_APA10x;
					ledstripe->use_spi = 1;
					ledstripe->use_leader = 4;
					ledstripe->use_trailer = 4;
					ledstripe->use_8bit = 1;
					ledstripe->bytes_pp = 4;
				}
			}
			break;
		}

		case 'h':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf > 0)
				{
					ledstripe->dev_speed = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'p':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					ledstripe->dev_pin = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'i':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					ledstripe->dev_inverse = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'b':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					ledstripe->num_bbuf = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'o':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					ledstripe->brightness = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case '7':
		case '8':
		case '9':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				int nbuf = (int) strtol(optarg, &eptr, 10);

				if ('\0' == *eptr)
				{
					ledstripe->intensity[c - '7'] = nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
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
		wordclock_log(wordclock_log_error, LOGNAME "extraneous argument: '%s'.\n", argv[optind]);
		ret = -1;
	}

	errReturn: return ret;
}

static void wordclock_ledstripe_print_configuration(struct wordclock_sink_component* component)
{
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	wordclock_log(wordclock_log_info, "\tdevice name:       %s\n"
			"\tdev hz:            %d\n"
			"\tdev type:	      %s\n"
			"\tnumber of leds:    %d\n"
			"\tbrightness:        %d%%\n"
			"\tintensity (rgb):   %d%%, %d%%, %d%%\n", ledstripe->dev_name, ledstripe->dev_speed,
			LED_TYPES[ledstripe->dev_type], ledstripe->actual_num_leds, ledstripe->num_bbuf,
			ledstripe->brightness, ledstripe->intensity[0], ledstripe->intensity[1],
			ledstripe->intensity[2]);
}

void wordclock_ledstripe_free(struct wordclock_sink_component* component)
{
	int i;
	struct wordclock_ledstripe_priv* ledstripe = (struct wordclock_ledstripe_priv*) component->priv;

	if (NULL != ledstripe)
	{
		if (NULL != ledstripe->dev_name)
			free(ledstripe->dev_name);

		if (NULL != ledstripe->out)
			free(ledstripe->out);

		if (NULL != ledstripe->bbuf)
		{
			for (i = 0; i < ledstripe->num_bbuf; i++)
				free(ledstripe->bbuf[i]);
			free(ledstripe->bbuf);
		}

		for (i = 0; i < 3; i++)
		{
			if (NULL != ledstripe->gamma_lut[i])
				wordclock_color_gamma_lookup_table_free(ledstripe->gamma_lut[i]);
		}

		free(ledstripe);
	}
}

struct wordclock_sink_component*
wordclock_ledstripe_create(const char* name, int argc, char** argv)
{
	struct wordclock_sink_component* ledstripe = wordclock_sink_component_create(name);

	if (NULL != ledstripe)
	{
		int i;
		struct wordclock_ledstripe_priv* priv = (struct wordclock_ledstripe_priv*) malloc(
				sizeof(struct wordclock_ledstripe_priv));

		ledstripe->priv = priv;

		memset(priv, 0, sizeof(struct wordclock_ledstripe_priv));

		priv->fd = -1;
		priv->dev_speed = DEFAULT_SPI_SPEED;
		priv->dev_name = strdup(DEFAULT_DEV_NAME);
		priv->dev_pin = GPIO_PIN;
		priv->dev_inverse = 1;
		priv->brightness = DEFAULT_BRIGHTNESS;
		priv->gamma[0] = DEFAULT_GAMMA_R;
		priv->gamma[1] = DEFAULT_GAMMA_G;
		priv->gamma[2] = DEFAULT_GAMMA_B;

		ledstripe->f_print_configuration = wordclock_ledstripe_print_configuration;
		ledstripe->f_start_sink = wordclock_ledstripe_start;
		ledstripe->f_stop_sink = wordclock_ledstripe_stop;
		ledstripe->f_num_outputs = wordclock_ledstripe_num_outputs;
		ledstripe->f_set_output_to_rgb = wordclock_ledstripe_set_output_to_rgb;
		ledstripe->f_map_output_to_point = wordclock_ledstripe_map_output_to_point;
		ledstripe->f_commit_outputs = wordclock_ledstripe_commit_outputs;
		ledstripe->f_free_priv = wordclock_ledstripe_free;

		if (wordclock_ledstripe_configure(ledstripe, argc, argv) < 0)
			goto errReturn;

		priv->out_len = sizeof(unsigned char) * priv->bytes_pp * priv->actual_num_leds + priv->use_leader
				+ priv->use_trailer;
		priv->out = (unsigned char*) malloc(priv->out_len);

		if (priv->num_bbuf > 1)
		{
			priv->bbuf = (unsigned char**) malloc(sizeof(unsigned char*) * priv->num_bbuf);
			for (i = 0; i < priv->num_bbuf; i++)
			{
				priv->bbuf[i] = (unsigned char*) malloc(priv->out_len);
				memset(priv->bbuf[i], 0, priv->out_len);
			}
		}
		else
			priv->num_bbuf = 0;

		if (priv->use_leader)
		{
			switch (priv->dev_type)
			{
			case LED_TYPE_APA10x:
				memset(priv->out, 0, priv->use_leader);
				break;
			}
		}
		if (priv->use_trailer)
		{
			switch (priv->dev_type)
			{
			case LED_TYPE_LPD880x:
				*(priv->out + priv->out_len - priv->use_trailer - 1) = 0x00;
				break;

			case LED_TYPE_APA10x:
				memset(priv->out + priv->out_len - priv->use_trailer - 1, 0xFF, priv->use_trailer);
				break;
			}
		}

		for (i = 0; i < 3; i++)
		{
			if (priv->gamma[i] >= 0.0)
			{
				priv->gamma_lut[i] = wordclock_color_gamma_lookup_table_create(priv->gamma[i]);
			}
		}
	}

	return ledstripe;

	errReturn: wordclock_sink_component_free(ledstripe);
	return NULL;
}

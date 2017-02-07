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
#define DEFAULT_GAMMA            1.6   // works well for me, but ymmv...
#define LOGNAME                  "ledstripe: "

enum
{
	LED_TYPE_LPD880x, LED_TYPE_WS280x, LED_TYPE_WS281x, LED_TYPE_SK6812, LED_TYPE_APA10x, NUM_LED_TYPES
};

static const char LED_TYPES[NUM_LED_TYPES][8] =
{ "LPD880x", "WS280x", "WS281x", "SK6812", "APA10x" };
static const char LED_COLPOS[NUM_LED_TYPES][3] =
{
{ 1, 0, 2 }, 	// GRB for LPD880x
{ 0, 1, 2 }, 	// RGB for WS280x
{ 0, 1, 2 }, 	// RGB for WS281x
{ 1, 0, 2 }, 	// GRB for SK6812
{ 0, 1, 2 } };	// RGB for APA10x
static const char ambitv_ledstripe_spidev_mode = 0;
static const char ambitv_ledstripe_spidev_bits = 8;
static const char ambitv_ledstripe_spidev_lsbf = 0;

static const char scol[][6] =
{ "red", "green", "blue" };

struct ambitv_ledstripe_priv
{
	char* dev_name;
	int fd, dev_speed, dev_type, dev_pin, dev_inverse, num_leds, actual_num_leds, out_len, bytes_pp, use_spi, use_8bit,
			use_leader, use_trailer;
	int led_len[4], *led_str[4];   // top, bottom, left, right
	double led_inset[4];              // top, bottom, left, right
	unsigned char* out;
	unsigned char** bbuf;
	int num_bbuf, bbuf_idx;
	int brightness, intensity[3], intensity_min[3];
	char colpos[3];		// offsets for colors in output-stream
	double gamma[3];      // RGB gamma, not GRB!
	unsigned char* gamma_lut[3];  // also RGB
};

static int*
ambitv_ledstripe_ptr_for_output(struct ambitv_ledstripe_priv* ledstripe, int output, int* led_str_idx, int* led_idx)
{
	int idx = 0, *ptr = NULL;

	if (output < ledstripe->num_leds)
	{
		while (output >= ledstripe->led_len[idx])
		{
			output -= ledstripe->led_len[idx];
			idx++;
		}

		if (ledstripe->led_str[idx][output] >= 0)
		{
			ptr = &ledstripe->led_str[idx][output];

			if (led_str_idx)
				*led_str_idx = idx;

			if (led_idx)
				*led_idx = output;
		}
	}

	return ptr;
}

static int ambitv_ledstripe_map_output_to_point(struct ambitv_sink_component* component, int output, int width,
		int height, int* x, int* y)
{
	int ret = -1, *outp = NULL, str_idx = 0, led_idx = 0;
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

	outp = ambitv_ledstripe_ptr_for_output(ledstripe, output, &str_idx, &led_idx);

	if (NULL != outp)
	{
		ret = 0;
		float llen = ledstripe->led_len[str_idx] - 1;
		float dim = (str_idx < 2) ? width : height;
		float inset = ledstripe->led_inset[str_idx] * dim;
		dim -= 2 * inset;

		switch (str_idx)
		{
		case 0:  // top
			*x = (int) CONSTRAIN(inset + (dim / llen) * led_idx, 0, width);
			*y = 0;
			break;
		case 1:  // bottom
			*x = (int) CONSTRAIN(inset + (dim / llen) * led_idx, 0, width);
			*y = height;
			break;
		case 2:  // left
			*x = 0;
			*y = (int) CONSTRAIN(inset + (dim / llen) * led_idx, 0, height);
			break;
		case 3:  // right
			*x = width;
			*y = (int) CONSTRAIN(inset + (dim / llen) * led_idx, 0, height);
			break;
		default:
			ret = -1;
			break;
		}
	}
	else
	{
		*x = *y = -1;
	}

	return ret;
}

static int ambitv_ledstripe_commit_outputs(struct ambitv_sink_component* component)
{
	int ret = -1;
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

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
		int bytepos;
		int i, j, k, l, chan;

		for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)         // Channel
		{
			pwm_dev_channel_t *channel = &pwm_dev.channel[chan];
			int wordpos = chan;

			for (i = 0; i < channel->count; i++)                // Led
			{
				for (j = 0; j < 3; j++)        // Color
				{
					bytepos = ledstripe->bytes_pp * i + j;
					for (k = 7; k >= 0; k--)                   // Bit
					{
						uint8_t symbol = SYMBOL_LOW;

						if (ledstripe->out[bytepos] & (1 << k))
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

static int ambitv_ledstripe_set_output_to_rgb(struct ambitv_sink_component* component, int idx, int r, int g, int b)
{
	int ret = -1, *outp = NULL, i, *rgb[] =
	{ &r, &g, &b };
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;
	unsigned char *bptr;

	if (idx >= ambitv_special_sinkcommand_brightness)
	{
		switch (idx)
		{
		case ambitv_special_sinkcommand_brightness:
			if (g)
				ret = ledstripe->brightness;
			else
			{
				ledstripe->brightness = r;
				ambitv_log(ambitv_log_info, LOGNAME "brightness was set to %d%%", r);
				ret = 0;
			}
			break;

		case ambitv_special_sinkcommand_gamma_red:
		case ambitv_special_sinkcommand_gamma_green:
		case ambitv_special_sinkcommand_gamma_blue:
		{
			unsigned char *tptr;

			idx -= ambitv_special_sinkcommand_gamma_red;
			if (g)
				ret = ledstripe->gamma[idx] * 100;
			else
			{
				ledstripe->gamma[idx] = (double) r / 100.0;
				tptr = ledstripe->gamma_lut[idx];
				ledstripe->gamma_lut[idx] = ambitv_color_gamma_lookup_table_create(ledstripe->gamma[idx]);
				ambitv_color_gamma_lookup_table_free(tptr);
				ambitv_log(ambitv_log_info, LOGNAME "gamma-%s was set to %.2f", scol[idx], ledstripe->gamma[idx]);
				ret = 0;
			}
		}
			break;

		case ambitv_special_sinkcommand_intensity_red:
		case ambitv_special_sinkcommand_intensity_green:
		case ambitv_special_sinkcommand_intensity_blue:
		{
			idx -= ambitv_special_sinkcommand_intensity_red;
			if (g)
				ret = ledstripe->intensity[idx];
			else
			{
				ledstripe->intensity[idx] = r / 100;
				ambitv_log(ambitv_log_info, LOGNAME "intensity-%s was set to %d%%", scol[idx],
						ledstripe->intensity[idx]);
				ret = 0;
			}
		}
			break;

		case ambitv_special_sinkcommand_min_intensity_red:
		case ambitv_special_sinkcommand_min_intensity_green:
		case ambitv_special_sinkcommand_min_intensity_blue:
		{
			idx -= ambitv_special_sinkcommand_min_intensity_red;
			if (g)
				ret = ledstripe->intensity_min[idx];
			else
			{
				ledstripe->intensity_min[idx] = r / 100;
				ambitv_log(ambitv_log_info, LOGNAME "intensity-min-%s was set to %d%%", scol[idx],
						ledstripe->intensity_min[idx]);
				ret = 0;
			}
		}
			break;
}
		return ret;
	}

	outp = ambitv_ledstripe_ptr_for_output(ledstripe, idx, NULL, NULL);

	if (NULL != outp)
	{
		int ii = *outp;

		if (ledstripe->num_bbuf)
		{
			unsigned char* acc = ledstripe->bbuf[ledstripe->bbuf_idx];

			acc[3 * ii] = r;
			acc[3 * ii + 1] = g;
			acc[3 * ii + 2] = b;

			r = g = b = 0;

			for (i = 0; i < ledstripe->num_bbuf; i++)
			{
				r += ledstripe->bbuf[i][3 * ii];
				g += ledstripe->bbuf[i][3 * ii + 1];
				b += ledstripe->bbuf[i][3 * ii + 2];
			}

			r /= ledstripe->num_bbuf;
			g /= ledstripe->num_bbuf;
			b /= ledstripe->num_bbuf;
		}

		for (i = 0; i < 3; i++)
		{
			*rgb[i] =
					(unsigned char) (((((int) *rgb[i] * ledstripe->brightness) / 100) * ledstripe->intensity[i]) / 100);
			if (ledstripe->gamma_lut[i])
				*rgb[i] = ambitv_color_map_with_lut(ledstripe->gamma_lut[i], *rgb[i]);
		}

		bptr = ledstripe->out + (ledstripe->bytes_pp * ii);
		bptr += ledstripe->use_leader;

		if(r < ledstripe->intensity_min[0]*2.55) r = ledstripe->intensity_min[0]*2.55;
		if(g < ledstripe->intensity_min[1]*2.55) g = ledstripe->intensity_min[1]*2.55;
		if(b < ledstripe->intensity_min[2]*2.55) b = ledstripe->intensity_min[2]*2.55;

		switch (ledstripe->dev_type)
		{
		case LED_TYPE_LPD880x:
			r = r >> 1 | 0x80;
			g = g >> 1 | 0x80;
			b = b >> 1 | 0x80;
			break;

		case LED_TYPE_APA10x:
			*bptr++ = 0xFF;
			break;
		}

		*(bptr + ledstripe->colpos[0]) = r;
		*(bptr + ledstripe->colpos[1]) = g;
		*(bptr + ledstripe->colpos[2]) = b;
		bptr += 3;
		ret = 0;
	}

	return ret;
}

static void ambitv_ledstripe_clear_leds(struct ambitv_sink_component* component)
{
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

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
			memset(ledstripe->out, 0x80, ledstripe->out_len - ledstripe->use_trailer);
			break;

		default:
			memset(ledstripe->out, 0x00, ledstripe->out_len);
			break;
		}
		// send 3 times, in case there's noise on the line,
		// so that all LEDs will definitely be off afterwards.
		for (i = 0; i < 3; i++)
		{
			ambitv_ledstripe_commit_outputs(component);
		}
	}
}

static int ambitv_ledstripe_num_outputs(struct ambitv_sink_component* component)
{
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

	return ledstripe->num_leds;
}

static int ambitv_ledstripe_start(struct ambitv_sink_component* component)
{
	int ret = 0;
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

	if (ledstripe->use_spi)
	{
		if (ledstripe->fd < 0)
		{
			ledstripe->fd = open(ledstripe->dev_name, O_WRONLY);
			if (ledstripe->fd < 0)
			{
				ret = ledstripe->fd;
				ambitv_log(ambitv_log_error,
				LOGNAME "failed to open device '%s' : %d (%s).\n", ledstripe->dev_name, errno, strerror(errno));
				goto errReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_MODE, &ambitv_ledstripe_spidev_mode);
			if (ret < 0)
			{
				ambitv_log(ambitv_log_error,
				LOGNAME "failed to spidev mode on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_BITS_PER_WORD, &ambitv_ledstripe_spidev_bits);
			if (ret < 0)
			{
				ambitv_log(ambitv_log_error,
				LOGNAME "failed to spidev bits-per-word on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_MAX_SPEED_HZ, &ledstripe->dev_speed);
			if (ret < 0)
			{
				ambitv_log(ambitv_log_error,
				LOGNAME "failed to spidev baudrate on device '%s' : %d (%s).\n", ledstripe->dev_name, errno,
						strerror(errno));
				goto closeReturn;
			}

			ret = ioctl(ledstripe->fd, SPI_IOC_WR_LSB_FIRST, &ambitv_ledstripe_spidev_lsbf);
			if (ret < 0)
			{
				ambitv_log(ambitv_log_error,
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
	ambitv_ledstripe_clear_leds(component);

	return ret;

	closeReturn: close(ledstripe->fd);
	ledstripe->fd = -1;
	errReturn: return ret;
}

static int ambitv_ledstripe_stop(struct ambitv_sink_component* component)
{
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

	ambitv_ledstripe_clear_leds(component);

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

static int ambitv_ledstripe_configure(struct ambitv_sink_component* component, int argc, char** argv)
{
	int i, c, ret = 0;
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

	memset(ledstripe->led_str, 0, sizeof(int*) * 4);
	ledstripe->num_leds = ledstripe->actual_num_leds = 0;

	if (NULL == ledstripe)
		return -1;

	static struct option lopts[] =
	{
	{ "led-device", required_argument, 0, 'd' },
	{ "dev-speed-hz", required_argument, 0, 'h' },
	{ "dev-type", required_argument, 0, 't' },
	{ "dev-pin", required_argument, 0, 'p' },
	{ "dev-inverse", required_argument, 0, 'i' },
	{ "dev-color-order", required_argument, 0, 'c' },
	{ "leds-top", required_argument, 0, '0' },
	{ "leds-bottom", required_argument, 0, '1' },
	{ "leds-left", required_argument, 0, '2' },
	{ "leds-right", required_argument, 0, '3' },
	{ "blended-frames", required_argument, 0, 'b' },
	{ "overall-brightness", required_argument, 0, 'o' },
	{ "gamma-red", required_argument, 0, '4' },
	{ "gamma-green", required_argument, 0, '5' },
	{ "gamma-blue", required_argument, 0, '6' },
	{ "intensity-red", required_argument, 0, '7' },
	{ "intensity-green", required_argument, 0, '8' },
	{ "intensity-blue", required_argument, 0, '9' },
	{ "intensity-min-red", required_argument, 0, 'A' },
	{ "intensity-min-green", required_argument, 0, 'B' },
	{ "intensity-min-blue", required_argument, 0, 'C' },
	{ "led-inset-top", required_argument, 0, 'w' },
	{ "led-inset-bottom", required_argument, 0, 'x' },
	{ "led-inset-left", required_argument, 0, 'y' },
	{ "led-inset-right", required_argument, 0, 'z' },
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
					ambitv_log(ambitv_log_error,
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
					ambitv_log(ambitv_log_error,
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
					ambitv_log(ambitv_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'c':
		{
			if (NULL != optarg)
			{
				int i, allfound = 0;

				for (i = 0; i < 3; i++)
				{
					switch (*(optarg + i))
					{
					case 'R':
					case 'r':
						ledstripe->colpos[i] = 0;
						allfound |= 1;
						break;

					case 'G':
					case 'g':
						ledstripe->colpos[i] = 1;
						allfound |= 2;
						break;

					case 'B':
					case 'b':
						ledstripe->colpos[i] = 2;
						allfound |= 4;
						break;
					}
				}

				if (allfound != 7)
				{
					ambitv_log(ambitv_log_error,
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
					ambitv_log(ambitv_log_error,
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
					ambitv_log(ambitv_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case '0':
		case '1':
		case '2':
		case '3':
		{
			int idx = c - '0';

			ret = ambitv_parse_led_string(optarg, &ledstripe->led_str[idx], &ledstripe->led_len[idx]);
			if (ret < 0)
			{
				ambitv_log(ambitv_log_error,
				LOGNAME "invalid led configuration string for '%s': '%s'.\n", argv[optind - 2], optarg);
				goto errReturn;
			}

			ledstripe->num_leds += ledstripe->led_len[idx];
			for (i = 0; i < ledstripe->led_len[idx]; i++)
				if (ledstripe->led_str[idx][i] >= 0)
					ledstripe->actual_num_leds++;

			break;
		}

		case '4':
		case '5':
		case '6':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				double nbuf = strtod(optarg, &eptr);

				if ('\0' == *eptr)
				{
					ledstripe->gamma[c - '4'] = nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error,
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
					ambitv_log(ambitv_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'A':
		case 'B':
		case 'C':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				int nbuf = (int) strtol(optarg, &eptr, 10);

				if ('\0' == *eptr)
				{
					ledstripe->intensity_min[c - 'A'] = nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					ret = -1;
					goto errReturn;
				}
			}

			break;
		}

		case 'w':
		case 'x':
		case 'y':
		case 'z':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				double nbuf = strtod(optarg, &eptr);

				if ('\0' == *eptr)
				{
					ledstripe->led_inset[c - 'w'] = nbuf / 100.0;
				}
				else
				{
					ambitv_log(ambitv_log_error,
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
		ambitv_log(ambitv_log_error, LOGNAME "extraneous argument: '%s'.\n", argv[optind]);
		ret = -1;
	}

	errReturn: return ret;
}

static void ambitv_ledstripe_print_configuration(struct ambitv_sink_component* component)
{
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

	ambitv_log(ambitv_log_info, "\tdevice name:         %s\n"
			"\tdev hz:              %d\n"
			"\tdev type:	     %s\n"
			"\tnumber of leds:      %d\n"
			"\tblending frames:     %d\n"
			"\tled insets (tblr):   %.1f%%, %.1f%%, %.1f%%, %.1f%%\n"
			"\tbrightness:          %d%%\n"
			"\tintensity (rgb):     %d%%, %d%%, %d%%\n"
			"\tintensity-min (rgb): %d%%, %d%%, %d%%\n"
			"\tgamma (rgb):         %.2f, %.2f, %.2f\n", ledstripe->dev_name, ledstripe->dev_speed,
			LED_TYPES[ledstripe->dev_type], ledstripe->actual_num_leds, ledstripe->num_bbuf,
			ledstripe->led_inset[0] * 100.0, ledstripe->led_inset[1] * 100.0, ledstripe->led_inset[2] * 100.0,
			ledstripe->led_inset[3] * 100.0, ledstripe->brightness, ledstripe->intensity[0], ledstripe->intensity[1],
			ledstripe->intensity[2], ledstripe->intensity_min[0], ledstripe->intensity_min[1],
			ledstripe->intensity_min[2], ledstripe->gamma[0], ledstripe->gamma[1], ledstripe->gamma[2]);
}

void ambitv_ledstripe_free(struct ambitv_sink_component* component)
{
	int i;
	struct ambitv_ledstripe_priv* ledstripe = (struct ambitv_ledstripe_priv*) component->priv;

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
				ambitv_color_gamma_lookup_table_free(ledstripe->gamma_lut[i]);
		}

		free(ledstripe);
	}
}

struct ambitv_sink_component*
ambitv_ledstripe_create(const char* name, int argc, char** argv)
{
	struct ambitv_sink_component* ledstripe = ambitv_sink_component_create(name);

	if (NULL != ledstripe)
	{
		int i;
		struct ambitv_ledstripe_priv* priv = (struct ambitv_ledstripe_priv*) malloc(
				sizeof(struct ambitv_ledstripe_priv));

		ledstripe->priv = priv;

		memset(priv, 0, sizeof(struct ambitv_ledstripe_priv));

		priv->fd = -1;
		priv->dev_speed = DEFAULT_SPI_SPEED;
		priv->dev_name = strdup(DEFAULT_DEV_NAME);
		priv->dev_pin = GPIO_PIN;
		priv->dev_inverse = 1;
		priv->brightness = DEFAULT_BRIGHTNESS;
		priv->gamma[0] = DEFAULT_GAMMA;
		priv->gamma[1] = DEFAULT_GAMMA;
		priv->gamma[2] = DEFAULT_GAMMA;

		memset(priv->colpos, 3, 3);

		ledstripe->f_print_configuration = ambitv_ledstripe_print_configuration;
		ledstripe->f_start_sink = ambitv_ledstripe_start;
		ledstripe->f_stop_sink = ambitv_ledstripe_stop;
		ledstripe->f_num_outputs = ambitv_ledstripe_num_outputs;
		ledstripe->f_set_output_to_rgb = ambitv_ledstripe_set_output_to_rgb;
		ledstripe->f_map_output_to_point = ambitv_ledstripe_map_output_to_point;
		ledstripe->f_commit_outputs = ambitv_ledstripe_commit_outputs;
		ledstripe->f_free_priv = ambitv_ledstripe_free;

		if (ambitv_ledstripe_configure(ledstripe, argc, argv) < 0)
			goto errReturn;

		if ((priv->colpos[0] == 3) || (priv->colpos[1] == 3) || (priv->colpos[2] == 3))
			memcpy(priv->colpos, LED_COLPOS[priv->dev_type], 3);

		if(priv->dev_type == LED_TYPE_LPD880x)
			priv->use_trailer = (priv->actual_num_leds / 16) + 1;

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
				memset(priv->out + priv->out_len - priv->use_trailer - 1, 0x00, priv->use_trailer);
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
				priv->gamma_lut[i] = ambitv_color_gamma_lookup_table_create(priv->gamma[i]);
			}
		}
	}

	return ledstripe;

	errReturn: ambitv_sink_component_free(ledstripe);
	return NULL;
}

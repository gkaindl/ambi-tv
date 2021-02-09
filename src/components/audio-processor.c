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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <sys/time.h>
#include <fftw3.h>

#include "audio-grab-source.h"
#include "audio-processor.h"

#include "../parse-conf.h"
#include "../video-fmt.h"
#include "../util.h"
#include "../log.h"

#define LOGNAME   "audio-processor: "
#define SMOOTH_FALLOFF			0x01
#define SMOOTH_AVERAGE			0x02
#define SMOOTH_INTEGRAL			0x04

enum
{
	ATYPE_SPECTRUM, ATYPE_AVERAGE, ATYPE_LMETER, ATYPE_NUMATYPES
};

#define DEFAULT_SENSITIVITY 	500
#define DEFAULT_SMOOTHING		(SMOOTH_FALLOFF | SMOOTH_AVERAGE | SMOOTH_INTEGRAL)
#define DEFAULT_TYPE			1
#define BANDS					40
#define BANDOFFS				3
#define MAXBANDS				(BANDS + 2*BANDOFFS)
#define MAXVAL					255
#define MSIZE					1024
#define COLORS					10
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct
{
	unsigned char red[COLORS + 1];
	unsigned char green[COLORS + 1];
	unsigned char blue[COLORS + 1];
} COLSTRUCT;

static const COLSTRUCT colors =
{
{ 0xFF, 0xFF, 0xAF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x00 },
{ 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x7F, 0xFF, 0x00 },
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 } };

float fc[MAXBANDS + 1];
float fr[MAXBANDS + 1];
int lcf[MAXBANDS + 1], hcf[MAXBANDS + 1];
int f[MAXBANDS + 1];
int fmem[MAXBANDS + 1];
int flast[MAXBANDS + 1];
int flastd[MAXBANDS + 1];
float peak[MAXBANDS + 2];
int y[SAMPLES / 2 + 1];
double in[2 * (SAMPLES / 2 + 1)];
fftw_complex out[SAMPLES / 2 + 1][2];
fftw_plan p;
int fall[MAXBANDS];
float fpeak[MAXBANDS + 1];
float k[MAXBANDS];
float g;
int framerate = 10;
uint64_t timertick;
static const float smooth[64] =
{ 5, 4.5, 4, 3, 2, 1.5, 1.25, 1.5, 1.5, 1.25, 1.25, 1.5, 1.25, 1.25, 1.5, 2, 2, 1.75, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5,
		1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.75, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
float sm = 1.25; //min val from smooth[]

struct ambitv_audio_processor_priv
{
	int *frame;
	int sensitivity, smoothing, type, linear, avgcolor;
	struct
	{
		int r, g, b;
	} lcolor;
	unsigned int rate;
};

uint64_t GetTimeStamp(void)
{
	static struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
}

static int ambitv_audio_processor_handle_frame(struct ambitv_processor_component* component, void* frame, int samples,
		int rate, int exval, int cmd)
{
	int i, j, o, ret = -1;
	float temp;

	struct ambitv_audio_processor_priv* audio = (struct ambitv_audio_processor_priv*) component->priv;

	if (cmd)
	{
		switch (cmd)
		{
		case ambitv_special_audiocommand_type:
			if (rate)
				ret = audio->type;
			else
			{
				audio->type = exval;
				ret = 0;
			}
			break;
		case ambitv_special_audiocommand_sensitivity:
			if (rate)
				ret = audio->sensitivity;
			else
			{
				audio->sensitivity = exval;
				ret = 0;
			}
			break;
		case ambitv_special_audiocommand_smoothing:
			if (rate)
				ret = audio->smoothing;
			else
			{
				audio->smoothing = exval;
				ret = 0;
			}
			break;
		case ambitv_special_audiocommand_linear:
			if (rate)
				ret = audio->linear;
			else
			{
				audio->linear = exval;
				ret = 0;
			}
			break;
		}
		return ret;
	}
	else
	{
		audio->frame = (int*) frame;
		audio->rate = (unsigned int) rate;

		framerate = 1000000L / (GetTimeStamp() - timertick);
		timertick = GetTimeStamp();

		// process: populate input buffer and check if input is present
		for (i = 0; i < (2 * (samples / 2 + 1)); i++)
		{
			if (i < samples)
			{
				in[i] = audio->frame[i];
			}
			else
				in[i] = 0;
		}
		// process: send input to external library
		fftw_execute(p);

		// process: separate frequency bands
		for (o = 0; o < MAXBANDS; o++)
		{
			peak[o] = 0;

			// process: get peaks
			for (i = lcf[o]; i <= hcf[o]; i++)
			{
				y[i] = pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5); //getting r of compex
				peak[o] += y[i]; //adding upp band
			}
			peak[o] = peak[o] / (hcf[o] - lcf[o] + 1); //getting average
			temp = peak[o] * k[o] * ((float) audio->sensitivity / 100.0); //multiplying with k and adjusting to sens settings
			if (temp > rate * 8)
				temp = rate * 8; //just in case
			f[o] = temp;
			for(j = 0; j < BANDOFFS; j++)
				peak[BANDOFFS] += peak[j];
			for(j = 0; j < BANDOFFS + 2; j++)
				peak[j] = peak[BANDOFFS];
		}
		// process [smoothing]
		if (audio->smoothing & SMOOTH_FALLOFF)
		{

			// process [smoothing]: falloff
			for (o = BANDOFFS; o < (MAXBANDS - BANDOFFS); o++)
			{
				temp = f[o];

				if (temp < flast[o])
				{
					f[o] = fpeak[o] - (g * fall[o] * fall[o]);
					fall[o]++;
				}
				else if (temp >= flast[o])
				{
					f[o] = temp;
					fpeak[o] = f[o];
					fall[o] = 0;
				}

				flast[o] = f[o];
			}
		}
		if (audio->smoothing & SMOOTH_AVERAGE)
		{
			// process [smoothing]: monstercat-style "average"
			int z, m_y;
			float m_o = 64 / MAXBANDS;
			for (z = BANDOFFS; z < (MAXBANDS - BANDOFFS); z++)
			{
				f[z] = f[z] * sm / smooth[(int) floor(z * m_o)];
				if (f[z] < 0.125)
					f[z] = 0.125;
				for (m_y = z - 1; m_y >= 0; m_y--)
				{
					f[m_y] = max(f[z] / pow(2, z - m_y), f[m_y]);
				}
				for (m_y = z + 1; m_y < MAXBANDS; m_y++)
				{
					f[m_y] = max(f[z] / pow(2, m_y - z), f[m_y]);
				}
			}
		}
		if (audio->smoothing & SMOOTH_INTEGRAL)
		{
			// process [smoothing]: integral
			for (o = BANDOFFS; o < (MAXBANDS - BANDOFFS); o++)
			{
				fmem[o] = fmem[o] * 0.55 + f[o];
				f[o] = fmem[o];

				if (f[o] < 1)
					f[o] = 1;

			}
		}
	}
	return 0;
}

static int ambitv_audio_processor_update_sink(struct ambitv_processor_component* processor,
		struct ambitv_sink_component* sink)
{
	int i, n_out, ret = 0;
	unsigned int r, g, b;
	double dr, dg, db, dx;
	double bsize, csize, level = 0.0;
	double fcpos1;
	double cpos2;
	int cpos1;
	double cdiff1;
	double cdiff2;
	double fbpos1;
	double bpos2;
	int bpos1;
	double bdiff1;
	double bdiff2;
	double tx;

	struct ambitv_audio_processor_priv* audio = (struct ambitv_audio_processor_priv*) processor->priv;

	if (NULL == audio->frame)
		return 0;

	if (sink->f_num_outputs && sink->f_set_output_to_rgb && sink->f_map_output_to_point)
	{
		n_out = sink->f_num_outputs(sink);

		dr = dg = db = 0.0;
		level = 0.0;
		if ((audio->type == ATYPE_LMETER) || (audio->type == ATYPE_AVERAGE))
		{
			for (i = 0; i < BANDS; i++)				// calculate average level
			{
				dx = f[i + BANDOFFS] / 512.0;
				if (dx > 1.0)
					dx = 1.0;
				else if (audio->linear)
					dx *= dx;
				level += dx;
			}
			level /= BANDS;
			level *= 3 * MSIZE;
			if ((audio->avgcolor) || (audio->type == ATYPE_AVERAGE))
			{

				for (i = 0; i < BANDS; i++)			// calculate average color
				{
					csize = (double) (COLORS) / (double) (BANDS);
					cpos2 = modf((double) i * csize, &fcpos1);
					cpos1 = fcpos1;
					cdiff1 = 1.0 - cpos2;
					cdiff2 = cpos2;
					tx = (double) f[i] / 512.0;
					if (tx > 1.0)
						tx = 1.0;
					if (audio->linear)
						tx *= tx;
					dx = (((double) colors.red[cpos1] * cdiff1) + ((double) colors.red[cpos1 + 1] * cdiff2)) * tx;
					if (dx > 255)
						dx = 255;
					dr += dx;
					dx = (((double) colors.green[cpos1] * cdiff1) + ((double) colors.green[cpos1 + 1] * cdiff2)) * tx;
					if (dx > 255)
						dx = 255;
					dg += dx;
					dx = (((double) colors.blue[cpos1] * cdiff1) + ((double) colors.blue[cpos1 + 1] * cdiff2)) * tx;
					if (dx > 255)
						dx = 255;
					db += dx;
				}
				dx = dr + dg + db;
				if(dx == 0.0)
					dx = 1.0;
				dr *= pow(dr / dx, 3);
				dg *= pow(dg / dx, 3);
				db *= pow(db / dx, 3);
				dx = 1.0;
				if (audio->type == ATYPE_LMETER)	// normalize brightness to 100%
				{
					if (dr > dx)
						dx = dr;
					if (dg > dx)
						dx = dg;
					if (db > dx)
						dx = db;
					dx = 255.0 / dx;
				}
				audio->lcolor.r = (int) (dr * dx);
				audio->lcolor.g = (int) (dg * dx);
				audio->lcolor.b = (int) (db * dx);
				if (audio->type == ATYPE_AVERAGE)
				{
					dx = audio->lcolor.r * 2;
					if (dx > 255)
						dx = 255;
					audio->lcolor.r = dx;
					dx = audio->lcolor.g * 2;
					if (dx > 255)
						dx = 255;
					audio->lcolor.g = dx;
					dx = audio->lcolor.b * 2;
					if (dx > 255)
						dx = 255;
					audio->lcolor.b = dx;
				}
			}
		}

		for (i = 0; i < n_out; i++)
		{
			int x, y, ii = 0;

			// calculate LED position on screen
			sink->f_map_output_to_point(sink, i, 2 * MSIZE, MSIZE, &x, &y);
			if (y == 0)				// top
			{
				ii = 2 * MSIZE;
				if (x > MSIZE)
					ii += (2 * MSIZE) - x;
				else
					ii += x;
			}
			else if (y == MSIZE)	// bottom
			{
				ii = 0;
				if (x > MSIZE)
					ii += x - MSIZE;
				else
					ii += MSIZE - x;
			}
			else if ((x == 0) || (x == 2 * MSIZE))		// left or right
			{
				ii = 2 * MSIZE - y;

			}

			switch (audio->type)
			{
			case ATYPE_LMETER:
				if ((int) level > ii)
				{
					sink->f_set_output_to_rgb(sink, i, audio->lcolor.r, audio->lcolor.g, audio->lcolor.b);
				}
				else
				{
					sink->f_set_output_to_rgb(sink, i, 0, 0, 0);
				}
				break;

			case ATYPE_AVERAGE:
				sink->f_set_output_to_rgb(sink, i, audio->lcolor.r, audio->lcolor.g, audio->lcolor.b);
				break;

			case ATYPE_SPECTRUM:
				csize = (double) (3 * MSIZE) / (double) (COLORS - 1);
				bsize = (double) (3 * MSIZE) / BANDS;
				cpos2 = modf((double) ii / csize, &fcpos1);
				cpos1 = fcpos1;
				cdiff1 = 1.0 - cpos2;
				cdiff2 = cpos2;
				bpos2 = modf((double) ii / bsize, &fbpos1);
				bpos1 = BANDOFFS + fbpos1;
				bdiff1 = 1.0 - bpos2;
				bdiff2 = bpos2;
				tx = (((double) f[bpos1] * bdiff1) + ((double) f[bpos1 + 1] * bdiff2)) / (256.0 + (256.0 * ((double)bpos1 / MAXBANDS)));
				if (tx > 1.0)
					tx = 1.0;
				if (audio->linear)
					tx = (tx * tx);
				r = (((double) colors.red[cpos1] * cdiff1) + ((double) colors.red[cpos1 + 1] * cdiff2)) * tx;
				if (r > 255)
					r = 255;
				g = (((double) colors.green[cpos1] * cdiff1) + ((double) colors.green[cpos1 + 1] * cdiff2)) * tx;
				if (g > 255)
					g = 255;
				b = (((double) colors.blue[cpos1] * cdiff1) + ((double) colors.blue[cpos1 + 1] * cdiff2)) * tx;
				if (b > 255)
					b = 255;

				sink->f_set_output_to_rgb(sink, i, r, g, b);
				break;
			}
		}
	}
	else
		ret = -1;

	if (sink->f_commit_outputs)
		sink->f_commit_outputs(sink);

	return ret;
}

static int ambitv_audio_processor_configure(struct ambitv_audio_processor_priv* audio, int argc, char** argv)
{
	int c, ret = 0;

	static struct option lopts[] =
	{
	{ "atype", required_argument, 0, 't' },
	{ "sensitivity", required_argument, 0, 's' },
	{ "smoothing", required_argument, 0, 'S' },
	{ "linear", required_argument, 0, 'e' },
	{ "levelcolor", required_argument, 0, 'l' },
	{ NULL, 0, 0, 0 } };

	optind = 0;
	while (1)
	{
		c = getopt_long(argc, argv, "", lopts, NULL);

		if (c < 0)
			break;

		switch (c)
		{
		case 's':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf > 0)
				{
					audio->sensitivity = (int) nbuf;
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

		case 'S':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0 && nbuf < 8)
				{
					audio->smoothing = (int) nbuf;
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
			case 't':
			{
				if (NULL != optarg)
				{
					char* eptr = NULL;
					long nbuf = strtol(optarg, &eptr, 10);

					if ('\0' == *eptr && nbuf >= 0 && nbuf < ATYPE_NUMATYPES)
					{
						audio->type = (int) nbuf;
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
			case 'e':
			{
				if (NULL != optarg)
				{
					char* eptr = NULL;
					long nbuf = strtol(optarg, &eptr, 10);

					if ('\0' == *eptr && nbuf >= 0 && nbuf < 2)
					{
						audio->linear = (int) nbuf;
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
			case 'l':
			{
				if (NULL != optarg)
				{
					if ((sscanf(optarg, "%2X%2X%2X", &audio->lcolor.r, &audio->lcolor.g, &audio->lcolor.b) == 3))
					{
						audio->avgcolor = !(audio->lcolor.r || audio->lcolor.g || audio->lcolor.b);
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
		}

		default:
			break;
		}
	}

	if (optind < argc)
	{
		ambitv_log(ambitv_log_error, LOGNAME "extraneous argument '%s'.\n", argv[optind]);
		ret = -1;
	}

	errReturn: return ret;
}

static void ambitv_audio_processor_print_configuration(struct ambitv_processor_component* component)
{
	struct ambitv_audio_processor_priv* audio = (struct ambitv_audio_processor_priv*) component->priv;

	ambitv_log(ambitv_log_info, "\ttype       : %d\n"
			"\tsensitivity:  %d\n"
			"\tsmoothing  : %d\n"
			"\tlevelcolor : %02X%02X%02X\n"
			"\tlinear     : %d\n", audio->type, audio->sensitivity, audio->smoothing, audio->lcolor.r, audio->lcolor.g,
			audio->lcolor.b, audio->linear);
}

static void ambitv_audio_processor_free(struct ambitv_processor_component* component)
{
	free(component->priv);
}

struct ambitv_processor_component*
ambitv_audio_processor_create(const char* name, int argc, char** argv)
{
	int n;

	struct ambitv_processor_component* audio_processor = ambitv_processor_component_create(name);

	if (NULL != audio_processor)
	{
		struct ambitv_audio_processor_priv* priv = (struct ambitv_audio_processor_priv*) malloc(
				sizeof(struct ambitv_audio_processor_priv));
		memset(priv, 9, sizeof(struct ambitv_audio_processor_priv));

		audio_processor->priv = (void*) priv;

		priv->sensitivity = DEFAULT_SENSITIVITY;
		priv->smoothing = DEFAULT_SMOOTHING;
		priv->type = DEFAULT_TYPE;
		priv->linear = 1;

		// process [smoothing]: calculate gravity
		g = ((float) MAXVAL / 400) * pow((60 / (float) framerate), 2.5);

		// process: calculate cutoff frequencies
		for (n = 0; n < MAXBANDS + 1; n++)
		{
			fc[n] = 20000 * pow(10, -2.37 + ((((float) n + 1) / ((float) MAXBANDS + 1)) * 2.37)); //decided to cut it at 20k, little interesting to hear above
			fr[n] = fc[n] / (priv->rate / 1); //remember nyquist!, pr my calculations this should be rate/2 and  nyquist freq in M/2 but testing shows it is not... or maybe the nq freq is in M/4
			lcf[n] = fr[n] * (SAMPLES / 4); //lfc stores the lower cut frequency foo each band in the fft out buffer

			if (n != 0)
			{
				hcf[n - 1] = lcf[n] - 1;
				if (lcf[n] <= lcf[n - 1])
					lcf[n] = lcf[n - 1] + 1; //pushing the spectrum up if the expe function gets "clumped"
				hcf[n - 1] = lcf[n] - 1;
			}
		}

		// process: weigh signal to frequencies
		for (n = 0; n < MAXBANDS; n++)
			k[n] = pow(fc[n], 0.62) * ((float) MAXVAL / (SAMPLES * 3000)) * 8;

		p = fftw_plan_dft_r2c_1d(SAMPLES, in, *out, FFTW_MEASURE); //planning to rock

		audio_processor->f_print_configuration = ambitv_audio_processor_print_configuration;
		audio_processor->f_consume_frame = ambitv_audio_processor_handle_frame;
		audio_processor->f_update_sink = ambitv_audio_processor_update_sink;
		audio_processor->f_free_priv = ambitv_audio_processor_free;

		timertick = GetTimeStamp();

		if (ambitv_audio_processor_configure(priv, argc, argv) < 0)
		{
			goto errReturn;
		}
	}

	return audio_processor;

	errReturn: ambitv_processor_component_free(audio_processor);
	return NULL;
}

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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>

#include "mood-light-processor.h"
#include "../parse-conf.h"
#include "../color.h"
#include "../util.h"
#include "../log.h"

#define DEFAULT_STEP          0.002

#define LOGNAME               "mood-light: "

struct wordclock_mood_light_processor
{
	float step;
	int mode;
	float offset;
};

static int wordclock_mood_light_processor_handle_frame(struct wordclock_processor_component* component, void* frame,
		int width, int height, int bytesperline, int fmt)
{
	struct wordclock_mood_light_processor* mood = (struct wordclock_mood_light_processor*) component->priv;

	mood->offset = fmod(mood->offset + mood->step, 1.0);

	return 0;
}

static int wordclock_mood_light_processor_update_sink(struct wordclock_processor_component* processor,
		struct wordclock_sink_component* sink)
{
	int i, n_out, ret = 0;

	struct wordclock_mood_light_processor* mood = (struct wordclock_mood_light_processor*) processor->priv;

	if (sink->f_num_outputs && sink->f_map_output_to_point && sink->f_set_output_to_rgb)
	{
		if(processor->first_run)
		{
			sink->f_set_output_to_rgb(sink, wordclock_special_sinkcommand_intensity_red, 10000, 0, 0);
			sink->f_set_output_to_rgb(sink, wordclock_special_sinkcommand_intensity_green, 10000, 0, 0);
			sink->f_set_output_to_rgb(sink, wordclock_special_sinkcommand_intensity_blue, 10000, 0, 0);
			processor->first_run = 0;
		}
		n_out = sink->f_num_outputs(sink);

		switch(mood->mode)
		{

			case 0:
			{
				for (i = 0; i < n_out; i++)
				{
					int x, y, r, g, b;
					float f;

					ret = sink->f_map_output_to_point(sink, i, 1024, 1024, &x, &y);

					f = CONSTRAIN((x / 1024.0 + y / 1024.0) / 2.0, 0.0, 1.0);
					f = fmod(f + mood->offset, 1.0);

					wordclock_hsl_to_rgb(255 * f, 255, 128, &r, &g, &b);

					sink->f_set_output_to_rgb(sink, i, r, g, b);
				}
			}
			break;

			case 1:
			{
				for (i = 0; i < n_out; i++)
				{
					int r, g, b;
					wordclock_hsl_to_rgb(255 * mood->offset, 255, 128, &r, &g, &b);
					sink->f_set_output_to_rgb(sink, i, r, g, b);
				}
			}
			break;

		}
	}
	else
		ret = -1;

	if (sink->f_commit_outputs)
		sink->f_commit_outputs(sink);

	return ret;
}

static int wordclock_mood_light_processor_configure(struct wordclock_processor_component* mood, int argc, char** argv)
{
	int c, ret = 0;

	struct wordclock_mood_light_processor* mood_priv = (struct wordclock_mood_light_processor*) mood->priv;
	if (NULL == mood_priv)
		return -1;

	static struct option lopts[] =
	{
	{ "speed", required_argument, 0, 's' },
	{ "mode", required_argument, 0, 'm' },
	{ NULL, 0, 0, 0 } };

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
				double nbuf = strtod(optarg, &eptr);

				if ('\0' == *eptr && nbuf > 0)
				{
					mood_priv->step = nbuf / 1000.0;
				}
				else
				{
					wordclock_log(wordclock_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
							optarg);
					return -1;
				}
			}

			break;
		}
		case 'm':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					mood_priv->mode = nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
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
		wordclock_log(wordclock_log_error, LOGNAME "extraneous configuration argument: '%s'.\n", argv[optind]);
		ret = -1;
	}

	return ret;
}

static void wordclock_mood_light_processor_print_configuration(struct wordclock_processor_component* component)
{
	struct wordclock_mood_light_processor* mood = (struct wordclock_mood_light_processor*) component->priv;

	wordclock_log(wordclock_log_info, "\tspeed:  %.1f\n", mood->step * 1000.0);
	wordclock_log(wordclock_log_info, "\tmode:  %d\n", mood->mode);
}

static void wordclock_mood_light_processor_free(struct wordclock_processor_component* component)
{
	free(component->priv);
}

struct wordclock_processor_component*
wordclock_mood_light_processor_create(const char* name, int argc, char** argv)
{
	struct wordclock_processor_component* mood_light_processor = wordclock_processor_component_create(name);

	if (NULL != mood_light_processor)
	{
		struct wordclock_mood_light_processor* priv = (struct wordclock_mood_light_processor*) malloc(
				sizeof(struct wordclock_mood_light_processor));

		mood_light_processor->priv = (void*) priv;

		priv->step = DEFAULT_STEP;

		mood_light_processor->f_print_configuration = wordclock_mood_light_processor_print_configuration;
		mood_light_processor->f_consume_frame = wordclock_mood_light_processor_handle_frame;
		mood_light_processor->f_update_sink = wordclock_mood_light_processor_update_sink;
		mood_light_processor->f_free_priv = wordclock_mood_light_processor_free;

		if (wordclock_mood_light_processor_configure(mood_light_processor, argc, argv) < 0)
			goto errReturn;
	}

	return mood_light_processor;

	errReturn: wordclock_processor_component_free(mood_light_processor);
	return NULL;
}

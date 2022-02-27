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
#include <unistd.h>
#include <getopt.h>

#include "timer-source.h"

#include "../log.h"

#define LOGNAME      "timer: "

#define DEFAULT_USEC       33000

struct wordclock_timer_priv
{
	unsigned long usec;
};

static int wordclock_timer_source_start(struct wordclock_source_component* timer)
{
	return 0;
}

static int wordclock_timer_source_stop(struct wordclock_source_component* timer)
{
	return 0;
}

static int wordclock_timer_source_loop_iteration(struct wordclock_source_component* timer)
{
	struct wordclock_timer_priv* timer_priv = (struct wordclock_timer_priv*) timer->priv;

	usleep(timer_priv->usec);

	wordclock_source_component_distribute_to_active_processors(timer,
	NULL, 0, 0, 0, 0);

	return 0;
}

static int wordclock_timer_source_configure(struct wordclock_source_component* timer, int argc, char** argv)
{
	int c, ret = 0;

	struct wordclock_timer_priv* timer_priv = (struct wordclock_timer_priv*) timer->priv;
	if (NULL == timer_priv)
		return -1;

	static struct option lopts[] =
	{
	{ "millis", required_argument, 0, 'm' },
	{ NULL, 0, 0, 0 } };

	while (1)
	{
		c = getopt_long(argc, argv, "", lopts, NULL);

		if (c < 0)
			break;

		switch (c)
		{
		case 'm':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf > 0)
				{
					timer_priv->usec = ((unsigned long) nbuf) * 1000;
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

static void wordclock_timer_source_print_configuration(struct wordclock_source_component* component)
{
	struct wordclock_timer_priv* timer_priv = (struct wordclock_timer_priv*) component->priv;

	wordclock_log(wordclock_log_info, "\tmillis: %lu\n", timer_priv->usec / 1000);
}

static void wordclock_timer_source_free(struct wordclock_source_component* component)
{
}

struct wordclock_source_component*
wordclock_timer_source_create(const char* name, int argc, char** argv)
{
	struct wordclock_source_component* timer = wordclock_source_component_create(name);

	if (NULL != timer)
	{
		struct wordclock_timer_priv* timer_priv = (struct wordclock_timer_priv*) malloc(sizeof(struct wordclock_timer_priv));
		if (NULL == timer_priv)
			goto errReturn;

		timer->priv = timer_priv;

		timer_priv->usec = DEFAULT_USEC;

		if (wordclock_timer_source_configure(timer, argc, argv) < 0)
			goto errReturn;

		timer->f_print_configuration = wordclock_timer_source_print_configuration;
		timer->f_start_source = wordclock_timer_source_start;
		timer->f_stop_source = wordclock_timer_source_stop;
		timer->f_run = wordclock_timer_source_loop_iteration;
		timer->f_free_priv = wordclock_timer_source_free;
	}

	return timer;

	errReturn: wordclock_source_component_free(timer);

	return NULL;
}

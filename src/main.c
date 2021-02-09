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
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <termios.h>

#include "parse-conf.h"
#include "registrations.h"
#include "program.h"
#include "gpio.h"
#include "util.h"
#include "log.h"
#include "component.h"
#include "pwm.h"

#define LOGNAME      "main: "

#define DEFAULT_CONFIG_PATH   "/etc/ambi-tv.conf"
#define BUTTON_MILLIS         250
#define BUTTON_MILLIS_HYST    10

static const char gcolors[][22] =
{ "gamma-red=", "gamma-green=", "gamma-blue=", "intensity-red=", "intensity-green=", "intensity-blue=", "intensity-min-red=", "intensity-min-green=", "intensity-min-blue=" };

struct ambitv_main_conf
{
	int program_idx;
	int gpio_idx;
	int portno;
	char* config_path;

	int cur_prog, ambitv_on, gpio_fd;
	int button_cnt;
	struct timeval last_button_press;
	volatile int running;
};

static struct ambitv_main_conf conf;
const char flagfile[] = "/tmp/.ambi-tv.mode";
int sockfd = -1, newsockfd = -1;
socklen_t clilen;
struct sockaddr_in serv_addr, cli_addr;
char buffer[512], *bufferptr;

static void ambitv_signal_handler(int signum)
{
	signal(signum, SIG_IGN);
	conf.running = 0;
}

static int ambitv_handle_config_block(const char* name, int argc, char** argv)
{
	int ret = 0;

	switch (name[0])
	{
	case '&':
		ret = ambitv_register_program_for_name(&name[1], argc, argv);
		break;

	default:
		ret = ambitv_register_component_for_name(name, argc, argv);
		break;
	}

	return ret;
}

static long ambitv_millis_between(struct timeval* now, struct timeval* earlier)
{
	return (long) ((now->tv_sec - earlier->tv_sec) * 1000) + (long) ((now->tv_usec - earlier->tv_usec) / 1000);
}

static int ambitv_cycle_next_program()
{
	int ret = 0;

	if (!conf.ambitv_on)
	{
		ambitv_log(ambitv_log_info,
		LOGNAME "not cycling program, because state is paused.\n");

		return 0;
	}

	conf.cur_prog = (conf.cur_prog + 1) % ambitv_num_programs;

	ret = ambitv_program_run(ambitv_programs[conf.cur_prog]);

	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to switch to program '%s', aborting...\n",
				ambitv_programs[conf.cur_prog]->name);
	}
	else
	{
		ambitv_log(ambitv_log_info, LOGNAME "switched to program '%s'.\n", ambitv_programs[conf.cur_prog]->name);
	}

	return ret;
}

static int ambitv_toggle_paused()
{
	int ret = 0;

	conf.ambitv_on = !conf.ambitv_on;

	if (conf.ambitv_on)
	{
		ret = ambitv_program_run(ambitv_programs[conf.cur_prog]);
		if (ret < 0)
			ambitv_log(ambitv_log_error, LOGNAME "failed to start program '%s'.\n",
					ambitv_programs[conf.cur_prog]->name);
	}
	else
	{
		ret = ambitv_program_stop_current();
		if (ret < 0)
			ambitv_log(ambitv_log_error, LOGNAME "failed to stop program '%s'.\n",
					ambitv_programs[conf.cur_prog]->name);
	}

	ambitv_log(ambitv_log_info, LOGNAME "now: %s\n", conf.ambitv_on ? "running" : "paused");

	return ret;
}

static int ambitv_runloop()
{
	int ret = -1, n;
	unsigned char c = 0;
	fd_set fds, ex_fds;
	struct timeval tv;
	FILE *fh;
	unsigned long fsize;
	bool bb = 0;

	FD_ZERO(&fds);
	FD_ZERO(&ex_fds);
	FD_SET(STDIN_FILENO, &fds);
	if (conf.gpio_fd >= 0)
		FD_SET(conf.gpio_fd, &ex_fds);

	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	bzero(buffer, 256);

	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (newsockfd >= 0)
	{
		n = read(newsockfd, buffer, sizeof(buffer));
		if (n >= 0)
			buffer[n] = 0;
	}
	if (strlen(buffer))
	{
		struct ambitv_sink_component* scomponent;
		struct ambitv_processor_component* pcomponent;

		if ((bufferptr = strstr(buffer, "Connection: close")) != NULL)
			bb = true;

		if ((bufferptr = strstr(buffer, "getconfig")) != NULL)
		{
			if ((fh = fopen(conf.config_path, "r")) != NULL)
			{
				fseek(fh, 0L, SEEK_END);
				fsize = ftell(fh);
				fseek(fh, 0L, SEEK_SET);
				netif_send(newsockfd, NULL, fsize, NETIF_MODE_FIRST, bb);
				while (fgets(buffer, sizeof(buffer), fh))
				{
					netif_send(newsockfd, buffer, 0, NETIF_MODE_MID, bb);
				}
				ret = 2;
				fclose(fh);
			}
		}
		else
		{
			char *valpos = strchr(buffer, '=');
			int getval, readval = 0, done = 0;
			double newval;

			if (valpos)
			{
				++valpos;
				if (*valpos > ' ')
				{
					readval = 1;
					sscanf(valpos, "%lf", &newval);
				}
				scomponent = (struct ambitv_sink_component*) ambitv_component_find_in_group("led-", 1);
				if (scomponent == NULL)
					scomponent = (struct ambitv_sink_component*) ambitv_component_find_in_group("led-", 0);
				if (scomponent != NULL)
				{
					if ((done = ((bufferptr = strstr(buffer, "mode=")) != NULL)) != 0)
					{
						if (readval)
						{
							if ((newval >= 0) && (newval < ambitv_num_programs))
							{
								ret = ambitv_program_run(ambitv_programs[(int) newval]);
								if (ret < 0)
									ambitv_log(ambitv_log_error, LOGNAME "failed to start program '%s'.\n",
											ambitv_programs[(int) newval]->name);
								else
									conf.cur_prog = (int) newval;
							}
							else
							{
								ret = ambitv_program_stop_current();
								if (ret < 0)
									ambitv_log(ambitv_log_error, LOGNAME "failed to stop program '%s'.\n",
											ambitv_programs[conf.cur_prog]->name);
							}
						}
						else
						{
							sprintf(buffer, "%d", conf.cur_prog);
							ret = 1;
						}
					}
					else if ((done = ((bufferptr = strstr(buffer, "brightness=")) != NULL)) != 0)
					{
						if (readval)
						{
							if (newval >= 0 && newval <= 100)
							{
								ret = scomponent->f_set_output_to_rgb(scomponent, ambitv_special_sinkcommand_brightness,
										(int) newval, 0, 0);
							}
						}
						else
						{
							getval = scomponent->f_set_output_to_rgb(scomponent, ambitv_special_sinkcommand_brightness,
									0, 1, 0);
							sprintf(buffer, "%d", getval);
							ret = 1;
						}
					}
					else
					{
						unsigned char idx, found = 0;

						for (idx = 0; idx < 9 && !found; idx++)
						{
							if ((done = ((bufferptr = strstr(buffer, gcolors[idx])) != NULL)) != 0)
							{
								found = 1;
								if (readval)
								{
									if (newval >= 0 && newval <= 100)
									{
										ret = scomponent->f_set_output_to_rgb(scomponent,
												ambitv_special_sinkcommand_gamma_red + idx, (int) (newval * 100.0), 0,
												0);
									}
								}
								else
								{
									getval = scomponent->f_set_output_to_rgb(scomponent,
											ambitv_special_sinkcommand_gamma_red + idx, 0, 1, 0);
									if (idx < 3)
										sprintf(buffer, "%.02lf", (double) getval / 100.0);
									else
										sprintf(buffer, "%d", (int) getval);
									ret = 1;
								}
							}
						}
					}
				}
				if (!done)
				{
					pcomponent = (struct ambitv_processor_component*) ambitv_component_find_in_group("audio-proc", 1);
					if (pcomponent == NULL)
						pcomponent = (struct ambitv_processor_component*) ambitv_component_find_in_group("audio-proc", 0);
					if (pcomponent != NULL)
					{
						if ((done = ((bufferptr = strstr(buffer, "typey=")) != NULL)) != 0)
						{
							if (readval)
							{
								if (newval >= 0 && newval <= 1000)
								{
									ret = pcomponent->f_consume_frame(pcomponent, NULL, 0, 0, (int) newval,
											ambitv_special_audiocommand_type);
								}
							}
							else
							{
								getval = pcomponent->f_consume_frame(pcomponent, NULL, 0, 1, 0,
										ambitv_special_audiocommand_type);
								sprintf(buffer, "%d", getval);
								ret = 1;
							}
						}
						else if ((done = ((bufferptr = strstr(buffer, "sensitivity=")) != NULL)) != 0)
						{
							if (readval)
							{
								if (newval >= 0 && newval <= 1000)
								{
									ret = pcomponent->f_consume_frame(pcomponent, NULL, 0, 0, (int) newval,
											ambitv_special_audiocommand_sensitivity);
								}
							}
							else
							{
								getval = pcomponent->f_consume_frame(pcomponent, NULL, 0, 1, 0,
										ambitv_special_audiocommand_sensitivity);
								sprintf(buffer, "%d", getval);
								ret = 1;
							}
						}
						else if ((done = ((bufferptr = strstr(buffer, "smoothing=")) != NULL)) != 0)
						{
							if (readval)
							{
								if (newval >= 0 && newval <= 7)
								{
									ret = pcomponent->f_consume_frame(pcomponent, NULL, 0, 0, (int) newval,
											ambitv_special_audiocommand_smoothing);
								}
							}
							else
							{
								getval = pcomponent->f_consume_frame(pcomponent, NULL, 0, 1, 0,
										ambitv_special_audiocommand_smoothing);
								sprintf(buffer, "%d", getval);
								ret = 1;
							}
						}
						else if ((done = ((bufferptr = strstr(buffer, "linear=")) != NULL)) != 0)
						{
							if (readval)
							{
								if (newval >= 0 && newval <= 1)
								{
									ret = pcomponent->f_consume_frame(pcomponent, NULL, 0, 0, (int) newval,
											ambitv_special_audiocommand_linear);
								}
							}
							else
							{
								getval = pcomponent->f_consume_frame(pcomponent, NULL, 0, 1, 0,
										ambitv_special_audiocommand_linear);
								sprintf(buffer, "%d", getval);
								ret = 1;
							}
						}
					}
				}
			}
		}
		if (newsockfd > -1)
		{
			switch (ret)
			{
			case -1:
			case 0:
				sprintf(buffer, "%s", (ret < 0) ? "ERR\n" : "OK \n");
				break;
			case 1:
				sprintf(buffer + strlen(buffer), "\r\n");
				break;
			default:
				*buffer = 0;
				break;
			}
			if (*buffer)
				netif_send(newsockfd, buffer, 0, NETIF_MODE_SINGLE, bb);
			close(newsockfd);
		}

		ret = 0;
		goto finishLoop;
	}

	ret = select(MAX(STDIN_FILENO, conf.gpio_fd) + 1, &fds, NULL, &ex_fds, &tv);

	if (ret < 0)
	{
		if (EINTR != errno && EWOULDBLOCK != errno)
		{
			ambitv_log(ambitv_log_error, LOGNAME "error during select(): %d (%s)\n",
			errno, strerror(errno));
			ret = 0;
		}

		goto finishLoop;
	}

	if (FD_ISSET(STDIN_FILENO, &fds))
	{
		ret = read(STDIN_FILENO, &c, 1);

		if (ret < 0)
		{
			if (EINTR != errno && EWOULDBLOCK != errno)
			{
				ambitv_log(ambitv_log_error, LOGNAME "error during read() on stdin: %d (%s)\n",
				errno, strerror(errno));
			}
			else
				ret = 0;

			goto finishLoop;
		}
		else if (0 == ret)
			goto finishLoop;

		switch (c)
		{
		case 0x20:
		{ // space
			ret = ambitv_cycle_next_program();
			if (ret < 0)
				goto finishLoop;

			break;
		}

		case 't':
		{
			ret = ambitv_toggle_paused();
			if (ret < 0)
				goto finishLoop;

			break;
		}

		default:
			break;
		}
	}

	if (conf.gpio_fd >= 0 && FD_ISSET(conf.gpio_fd, &ex_fds))
	{
		char buf[16];

		ret = read(conf.gpio_fd, buf, sizeof(*buf));
		lseek(conf.gpio_fd, 0, SEEK_SET);

		if (ret < 0)
		{
			if (EINTR != errno && EWOULDBLOCK != errno)
			{
				ambitv_log(ambitv_log_error, LOGNAME "failed to read from gpio %d.\n", conf.gpio_idx);
				ret = -1;
			}
			else
				ret = 0;

			goto finishLoop;
		}
		else if (0 == ret)
			goto finishLoop;

		if ('0' == buf[0])
		{
			struct timeval now;

			(void) gettimeofday(&now, NULL);
			if (0 != conf.last_button_press.tv_sec)
			{
				long millis = ambitv_millis_between(&now, &conf.last_button_press);

				if (millis <= BUTTON_MILLIS && BUTTON_MILLIS_HYST <= millis)
					conf.button_cnt++;
			}
			else
				conf.button_cnt++;

			memcpy(&conf.last_button_press, &now, sizeof(struct timeval));
		}
	}

	if (conf.button_cnt > 0)
	{
		struct timeval now;
		(void) gettimeofday(&now, NULL);
		if (0 != conf.last_button_press.tv_sec)
		{
			long millis = ambitv_millis_between(&now, &conf.last_button_press);

			if (millis > BUTTON_MILLIS)
			{
				if (conf.button_cnt > 1)
				{
					ret = ambitv_cycle_next_program();
				}
				else
				{
					ret = ambitv_toggle_paused();
				}

				conf.button_cnt = 0;
				memset(&conf.last_button_press, 0, sizeof(struct timeval));
			}
		}
	}

	finishLoop: return ret;
}

static void ambitv_usage(const char* name)
{
	const char* p = name + strlen(name);
	while (p != name && *p != '/')
		p--;
	if ('/' == *p)
		p++;

	printf(
			"usage: %s [options]\n"
					"\n"
					"options:\n"
					"\t-b/--button-gpio [i]     gpio pin to use as physical button. function disabled if i < 0. (default: -1).\n"
					"\t-f/--file [path]         use the configuration file at [path] (default: %s).\n"
					"\t-h,--help                display this help text.\n"
					"\t-p,--program [i]         run the [i]-th program from the configuration file on start-up.\n"
					"\t-s,--socketport [i]      set the socket port to communicate with %s\n"
					"\n", p, DEFAULT_CONFIG_PATH, p);
}

static int ambitv_main_configure(int argc, char** argv)
{
	int c, ret = 0;

	static struct option lopts[] =
	{
	{ "button-gpio", required_argument, 0, 'b' },
	{ "file", required_argument, 0, 'f' },
	{ "help", no_argument, 0, 'h' },
	{ "program", required_argument, 0, 'p' },
	{ "socketport", required_argument, 0, 's' },
	{ NULL, 0, 0, 0 } };
	while (1)
	{
		c = getopt_long(argc, argv, "b:f:hp:", lopts, NULL);

		if (c < 0)
			break;

		switch (c)
		{
		case 'f':
		{
			if (NULL != optarg)
			{
				conf.config_path = strdup(optarg);
			}
			break;
		}

		case 's':
		{
			if (NULL != optarg)
			{
				conf.portno = atoi(optarg);
			}
			break;
		}

		case 'b':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf > 0)
				{
					conf.gpio_idx = (int) nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
							optarg);
					ambitv_usage(argv[0]);
					return -1;
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
					conf.program_idx = (int) nbuf;
				}
				else
				{
					ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2],
							optarg);
					ambitv_usage(argv[0]);
					return -1;
				}
			}

			break;
		}

		case 'h':
		{
			ambitv_usage(argv[0]);
			exit(0);
		}

		default:
			break;
		}
	}

	if (optind < argc)
	{
		ambitv_log(ambitv_log_error, LOGNAME "extraneous configuration argument: '%s'.\n", argv[optind]);
		ambitv_usage(argv[0]);
		ret = -1;
	}

	return ret;
}

int main(int argc, char** argv)
{
	int ret = 0, i;
	struct ambitv_conf_parser* parser;
	struct termios tt;
	unsigned long tt_orig;

	signal(SIGINT, ambitv_signal_handler);
	signal(SIGTERM, ambitv_signal_handler);

	printf("\n"
			"*********************************************************\n"
			"*  ambi-tv: diy ambient lighting for your screen or tv  *\n"
			"*                                         (c) @gkaindl  *\n"
			"*********************************************************\n"
			"\n");

	conf.program_idx = 0;
	conf.gpio_idx = -1;
	conf.portno = 16384;
	conf.config_path = DEFAULT_CONFIG_PATH;
	conf.ambitv_on = 1;
	conf.gpio_fd = -1;
	conf.running = 1;

	ret = ambitv_main_configure(argc, argv);
	if (ret < 0)
		return -1;

	parser = ambitv_conf_parser_create();
	parser->f_handle_block = ambitv_handle_config_block;
	ret = ambitv_conf_parser_read_config_file(parser, conf.config_path);
	ambitv_conf_parser_free(parser);

	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to parse configuration file, aborting...\n");
		ambitv_usage(argv[0]);
		return -1;
	}

	if (ambitv_num_programs <= 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "no programs available, aborting...\n");
		return -1;
	}

	ambitv_log(ambitv_log_info, LOGNAME "configuration finished, %d programs available.\n", ambitv_num_programs);

	for (i = 0; i < ambitv_num_programs; i++)
		ambitv_log(ambitv_log_info, "\t%s\n", ambitv_programs[i]->name);

	if (conf.gpio_idx >= 0)
	{
		conf.gpio_fd = ambitv_gpio_open_button_irq(conf.gpio_idx);
		if (conf.gpio_fd < 0)
		{
			ambitv_log(ambitv_log_error, LOGNAME "failed to prepare gpio %d, aborting...\n", conf.gpio_idx);
			return -1;
		}
		else
		{
			ambitv_log(ambitv_log_info, LOGNAME "using gpio %d as physical button.\n", conf.gpio_idx);
		}
	}

	tcgetattr(STDIN_FILENO, &tt);
	tt_orig = tt.c_lflag;
	tt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &tt);

	if (conf.program_idx >= ambitv_num_programs)
	{
		ambitv_log(ambitv_log_error,
		LOGNAME "program at index %d requested, but only %d programs available. aborting...\n", conf.program_idx,
				ambitv_num_programs);
		goto errReturn;
	}

	conf.cur_prog = conf.program_idx;

	ret = ambitv_program_run(ambitv_programs[conf.cur_prog]);

	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to start initial program '%s', aborting...\n",
				ambitv_programs[conf.cur_prog]->name);
		goto errReturn;
	}

	ambitv_log(ambitv_log_info, LOGNAME "started initial program '%s'.\n", ambitv_programs[conf.cur_prog]->name);

	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		ambitv_log(ambitv_log_error, LOGNAME "ERROR opening socket\n");
	else
	{
		fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(conf.portno);
		if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
			ambitv_log(ambitv_log_error, LOGNAME "ERROR on binding socket to port %d\n", conf.portno);
		if (listen(sockfd, 5) < 0)
			ambitv_log(ambitv_log_error, LOGNAME "ERROR on listening on socket\n");
		clilen = sizeof(cli_addr);
	}
	ambitv_log(ambitv_log_info,
	LOGNAME "************* start-up complete\n"
	"\tpress <space> to cycle between programs.\n"
	"\tpress 't' to toggle pause.\n");
	if (conf.gpio_idx >= 0)
	{
		ambitv_log(ambitv_log_info,
				"\tphysical (gpio) button: click to pause/resume, double-click to cycle between programs.\n");
	}

	while (conf.running && ambitv_runloop() >= 0)
		;

	ret = ambitv_program_stop_current();

	if (ret < 0)
	{
		ambitv_log(ambitv_log_error, LOGNAME "failed to stop program '%s' before exiting.\n",
				ambitv_programs[conf.cur_prog]->name);
		goto errReturn;
	}

	errReturn: if (sockfd > -1)
		close(sockfd);

	tt.c_lflag = tt_orig;
	tcsetattr(STDIN_FILENO, TCSANOW, &tt);

	if (conf.gpio_fd >= 0)
		ambitv_gpio_close_button_irq(conf.gpio_fd, conf.gpio_idx);

	return ret;
}

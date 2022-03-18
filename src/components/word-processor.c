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
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <string.h>

#include "word-processor.h"

#include "../color.h"
#include "../util.h"
#include "../log.h"
#include "../parse-conf.h"

#define LOGNAME               "word-processor: "

// words for log output
static const char twords[][12] =
{ "", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf",
		"Ein" , "Zwanzig", "vor", "nach", "um", "viertel", "halb", "dreiviertel",
		"Fuenf", "Zehn", "Es", "ist", "etwa", "genau", "Uhr"};

// word indices
enum {
	_NULL_, _EINS_, _ZWEI_, _DREI_, _VIER_, _FUENF_, _SECHS_, _SIEBEN_, _ACHT_, _NEUN_, _ZEHN_, _ELF_, _ZWOELF_,
	_EIN_, _ZWANZIG_M_, _VOR_, _NACH_, _UM_, _VIERTEL_, _HALB_, _DREIVIERTEL_,
	_FUENF_M_, _ZEHN_M_, _ES_, _IST_, _ETWA_, _GENAU_, _UHR_};


// display layout and led order
/*
 	 E	S	K	I	S	T	L	E	T	W	A		110 -> 120
													 |
 	 O	G	E	N	A	U	B	F	Ü	N	F		109 <- 099
															|
 	 Z	E	H	N	Z	W	A	N	Z	I	G		088 -> 098
													 |
 	 D	R	E	I	V	I	E	R	T	E	L		087 <- 077
															|
 	 T	G	N	A	C	H	V	O	R	U	M		066 -> 076
													 |
 	 H	A	L	B	Q	Z	W	Ö	L	F	P		065 <- 055
															|
 	 Z 	W	E	I	N	S	I	E	B	E	N		044 -> 054
													 |
 	 K	D	R	E	I	R	H 	F	Ü	N	F		043 <- 033
															|
 	 E	L	F	N	E	U	N	V	I	E	R		022 -> 032
													 |
 	 W	A	C	H	T	Z	E	H	N	R	S		021 <- 011
															|
 	 B	S	E	C	H	S	F	M	U	H	R		000 -> 010

*/

// word positions by led numbers {from, to}
static const int tleds[][2] =
{
{ 0, 0 },
{ 46, 49 }, 	// EINS
{ 44, 47 }, 	// ZWEI
{ 39, 42 }, 	// DREI
{ 29, 32 }, 	// VIER
{ 33, 36 },		// FUENF
{ 1, 5 },		// SECHS
{ 49, 54 },		// SIEBEN
{ 17, 20 },		// ACHT
{ 25, 28 },		// NEUN
{ 13, 16 },		// ZEHN
{ 22, 24 },		// ELF
{ 56, 60 },		// ZWOELF
{ 46, 48 },		// EIN
{ 92, 98 },		// ZWANZIG_M
{ 72, 74 },		// VOR
{ 68, 71 },		// NACH
{ 75, 76 },		// UM
{ 77, 83 },		// VIERTEL
{ 62, 65 },		// HALB
{ 77, 87 },		// DREIVIERTEL
{ 99, 102 },	// FUENF_M
{ 88, 91 },		// ZEHN_M
{ 110, 111 },	// ES
{ 113, 115 },	// IST
{ 117, 120 },	// ETWA
{ 104, 108 },	// GENAU
{ 8, 10}};		// UHR

// translation from minutes to word expressions (east and west german)
static const int CALC[2][12][4] =
{
{
{ 0, 0, _UM_, 0 },
{ _FUENF_M_, _NACH_, 0, 0 },
{ _ZEHN_M_, _NACH_, 0, 0 },
{ 0, 0, _VIERTEL_, 1 },
{ _ZEHN_M_, _VOR_, _HALB_, 1 },
{ _FUENF_M_, _VOR_, _HALB_, 1 },
{ 0, 0, _HALB_, 1 },
{ _FUENF_M_, _NACH_, _HALB_, 1 },
{ _ZEHN_M_, _NACH_, _HALB_, 1 },
{ 0, 0, _DREIVIERTEL_, 1 },
{ _ZEHN_M_, _VOR_, 0, 1 },
{ _FUENF_M_, _VOR_, 0, 1 } },
{
{ 0, 0, _UM_, 0 },
{ _FUENF_M_, _NACH_, 0, 0 },
{ _ZEHN_M_, _NACH_, 0, 0 },
{ _VIERTEL_, _NACH_, 0, 0 },
{ _ZWANZIG_M_, _NACH_, 0, 0 },
{ _FUENF_M_, _VOR_, _HALB_, 1 },
{ 0, 0, _HALB_, 1 },
{ _FUENF_M_, _NACH_, _HALB_, 1 },
{ _ZWANZIG_M_, _VOR_, 0, 1 },
{ _VIERTEL_, _VOR_, 0, 1 },
{ _ZEHN_M_, _VOR_, 0, 1 },
{ _FUENF_M_, _VOR_, 0, 1 } } };

struct wordclock_word_processor
{
	float offset;
	int mode;
	int precise;
	int trailer;
	int lvals[10];
	int r, g, b;
};

static int wordclock_word_processor_handle_frame(struct wordclock_processor_component* component, void* frame, int rate,
		int dummy, int exval, int cmd)
{
	time_t ttime;
	struct tm *atime;
	int i, j, hour, min, emin, ret = 0;

	struct wordclock_word_processor* wordp = (struct wordclock_word_processor*) component->priv;

	if (cmd)
	{
		ret = -1;
		switch (cmd)
		{
		case wordclock_special_proccommand_mode:
			if (rate)
				ret = wordp->mode;
			else
			{
				wordp->mode = exval;
				ret = 0;
			}
			break;
		case wordclock_special_proccommand_red:
			if (rate)
				ret = wordp->r;
			else
			{
				wordp->r = exval;
				ret = 0;
			}
			break;
		case wordclock_special_proccommand_green:
			if (rate)
				ret = wordp->g;
			else
			{
				wordp->g = exval;
				ret = 0;
			}
			break;
		case wordclock_special_proccommand_blue:
			if (rate)
				ret = wordp->b;
			else
			{
				wordp->b = exval;
				ret = 0;
			}
			break;
		}
	}
	time(&ttime);
	atime = localtime(&ttime);
	emin = atime->tm_min;
	ttime += 150;
	atime = localtime(&ttime);

	min = atime->tm_min / 5;
	hour = (atime->tm_hour + CALC[wordp->mode][min][3]) % 12;
	if (!hour)
		hour = 12;

	j = 0;
	wordp->lvals[j++] = _ES_;
	wordp->lvals[j++] = _IST_;
	if(emin % 5)
	{
		if(wordp->precise & 0x02)
			wordp->lvals[j++] = _ETWA_;
	}
	else
	{
		if(wordp->precise & 0x01)
			wordp->lvals[j++] = _GENAU_;
	}
	for (i = 0; i < 3; i++)
	{
		if (CALC[wordp->mode][min][i])
		{
			wordp->lvals[j++] = CALC[wordp->mode][min][i];
		}
	}
	wordp->lvals[j++] = (hour == 1)?((wordp->trailer)?_EIN_:_EINS_):hour;
	if(wordp->trailer)
		wordp->lvals[j++] = _UHR_;
	wordp->lvals[j] = 0;

	return ret;
}

static int wordclock_word_processor_update_sink(struct wordclock_processor_component* processor,
		struct wordclock_sink_component* sink)
{
	int i, j, ret = 0;
	char tstr[128] = "";
	static char tstr2[128] = "X";
	time_t ttime;
	struct tm *atime;

	struct wordclock_word_processor* wordp = (struct wordclock_word_processor*) processor->priv;

	if (sink->f_num_outputs && sink->f_set_output_to_rgb)
	{
		if(processor->first_run)
		{
			sink->f_set_output_to_rgb(sink, wordclock_special_sinkcommand_intensity_red, wordp->r * 100, 0, 0);
			sink->f_set_output_to_rgb(sink, wordclock_special_sinkcommand_intensity_green, wordp->g * 100, 0, 0);
			sink->f_set_output_to_rgb(sink, wordclock_special_sinkcommand_intensity_blue, wordp->b * 100, 0, 0);
			processor->first_run = 0;
		}

		time(&ttime);
		atime = localtime(&ttime);

		for (i = 0; i < sink->f_num_outputs(sink); i++)
			sink->f_set_output_to_rgb(sink, -1 - i, 0, 0, 0);
		for (i = 0; wordp->lvals[i]; i++)
		{
			for (j = tleds[wordp->lvals[i]][0]; j <= tleds[wordp->lvals[i]][1] && j < sink->f_num_outputs(sink); j++)
			{
/*
			sink->f_set_output_to_rgb(sink, -1 - j, (int) (wordp->r * 2.55), (int) (wordp->g * 2.55),
						(int) (wordp->b * 2.55));
*/
				sink->f_set_output_to_rgb(sink, -1 - j, 255, 255, 255);
			}
			sprintf(tstr + strlen(tstr), "%s ", twords[wordp->lvals[i]]);
		}
		sprintf(tstr + strlen(tstr), " (%02d:%02d)\n", atime->tm_hour, atime->tm_min);
		if (strcmp(tstr, tstr2))
		{
			printf("[word-clock] %s", tstr);
			strcpy(tstr2, tstr);
		}

	}
	else
		ret = -1;

	if (sink->f_commit_outputs)
		sink->f_commit_outputs(sink);

	return ret;
}

static int wordclock_word_processor_configure(struct wordclock_processor_component* wordp, int argc, char** argv)
{
	int c, ret = 0;

	struct wordclock_word_processor* wordp_priv = (struct wordclock_word_processor*) wordp->priv;
	if (NULL == wordp_priv)
		return -1;

	static struct option lopts[] =
	{
	{ "mode", required_argument, 0, 'm' },
	{ "precise", required_argument, 0, 'p' },
	{ "trailer", required_argument, 0, 't' },
	{ "red", required_argument, 0, 'r' },
	{ "green", required_argument, 0, 'g' },
	{ "blue", required_argument, 0, 'b' },
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

				if ('\0' == *eptr && nbuf >= 0)
				{
					wordp_priv->mode = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					return -1;
				}
			}
		}
			break;
		case 'p':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					wordp_priv->precise = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					return -1;
				}
			}
		}
			break;
		case 't':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					wordp_priv->trailer = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					return -1;
				}
			}
		}
			break;
		case 'r':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					wordp_priv->r = (int) nbuf;
				}
				else
				{
					wordclock_log(wordclock_log_error,
					LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
					return -1;
				}
			}
		}
			break;
		case 'g':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					wordp_priv->g = (int) nbuf;
				};
			}
			else
			{
				wordclock_log(wordclock_log_error,
				LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
				return -1;
			}
		}
			break;
		case 'b':
		{
			if (NULL != optarg)
			{
				char* eptr = NULL;
				long nbuf = strtol(optarg, &eptr, 10);

				if ('\0' == *eptr && nbuf >= 0)
				{
					wordp_priv->b = (int) nbuf;
				};
			}
			else
			{
				wordclock_log(wordclock_log_error,
				LOGNAME "invalid argument for '%s': '%s'.\n", argv[optind - 2], optarg);
				return -1;
			}
		}
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

static void wordclock_word_processor_print_configuration(struct wordclock_processor_component* component)
{
	struct wordclock_word_processor* wordp = (struct wordclock_word_processor*) component->priv;

	wordclock_log(wordclock_log_info, "\tmode	        :  %d\n", wordp->mode);
	wordclock_log(wordclock_log_info, "\tprecise        :  %d\n", wordp->precise);
	wordclock_log(wordclock_log_info, "\ttrailer        :  %d\n", wordp->trailer);
	wordclock_log(wordclock_log_info, "\tintensity-red  :  %d\n", wordp->r);
	wordclock_log(wordclock_log_info, "\tintensity-green:  %d\n", wordp->g);
	wordclock_log(wordclock_log_info, "\tintensity-blue :  %d\n", wordp->b);
}

static void wordclock_word_processor_free(struct wordclock_processor_component* component)
{
	free(component->priv);
}

struct wordclock_processor_component*
wordclock_word_processor_create(const char* name, int argc, char** argv)
{
	int i;
	struct wordclock_processor_component* word_processor = wordclock_processor_component_create(name);

	if (NULL != word_processor)
	{
		struct wordclock_word_processor* priv = (struct wordclock_word_processor*) malloc(
				sizeof(struct wordclock_word_processor));

		word_processor->priv = (void*) priv;

		priv->mode = 0;
		for (i = 0; i < 7; i++)
			priv->lvals[i] = 0;

		word_processor->f_print_configuration = wordclock_word_processor_print_configuration;
		word_processor->f_consume_frame = wordclock_word_processor_handle_frame;
		word_processor->f_update_sink = wordclock_word_processor_update_sink;
		word_processor->f_free_priv = wordclock_word_processor_free;

		if (wordclock_word_processor_configure(word_processor, argc, argv) < 0)
			goto errReturn;
	}

	return word_processor;

	errReturn: wordclock_processor_component_free(word_processor);
	return NULL;
}

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

#ifndef __WORDCLOCK_PARSE_CONF_H__
#define __WORDCLOCK_PARSE_CONF_H__

#define NUM_ROWS	11
#define NUM_COLS	11

enum wordclock_special_sinkcommand
{
	wordclock_special_sinkcommand_brightness = 10000,
	wordclock_special_sinkcommand_gamma_red,
	wordclock_special_sinkcommand_gamma_green,
	wordclock_special_sinkcommand_gamma_blue,
	wordclock_special_sinkcommand_intensity_red,
	wordclock_special_sinkcommand_intensity_green,
	wordclock_special_sinkcommand_intensity_blue,
	wordclock_special_sinkcommand_min_intensity_red,
	wordclock_special_sinkcommand_min_intensity_green,
	wordclock_special_sinkcommand_min_intensity_blue
};

enum wordclock_special_proccommand
{
	wordclock_special_proccommand_mode = 1,
	wordclock_special_proccommand_red,
	wordclock_special_proccommand_green,
	wordclock_special_proccommand_blue
};

enum wordclock_special_audiocommand
{
	wordclock_special_audiocommand_type = 1,
	wordclock_special_audiocommand_sensitivity,
	wordclock_special_audiocommand_smoothing,
	wordclock_special_audiocommand_linear
};

enum wordclock_conf_parser_state
{
	wordclock_conf_parser_state_toplevel,
	wordclock_conf_parser_state_block,
	wordclock_conf_parser_state_block_name,
	wordclock_conf_parser_state_block_name_done,
	wordclock_conf_parser_state_block_var_name,
	wordclock_conf_parser_state_block_var_name_done,
	wordclock_conf_parser_state_block_value,
	wordclock_conf_parser_state_block_value_done
};

struct wordclock_conf_parser
{
	enum wordclock_conf_parser_state state;

	char* path;
	unsigned current_line_num;

	char* current_block_name;
	char* current_var_name;
	char* current_value;

	int argidx, arglen;
	char** argv;

	int (*f_handle_block)(const char*, int, char**);
};

struct wordclock_conf_parser*
wordclock_conf_parser_create(void);

void
wordclock_conf_parser_free(struct wordclock_conf_parser* parser);

int
wordclock_conf_parser_read_config_file(struct wordclock_conf_parser* parser, const char* path);

#endif // __WORDCLOCK_PARSE_CONF_H__

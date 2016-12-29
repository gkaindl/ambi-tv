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

#ifndef __AMBITV_PARSE_CONF_H__
#define __AMBITV_PARSE_CONF_H__

enum ambitv_special_sinkcommand
{
	ambitv_special_sinkcommand_brightness = 10000,
	ambitv_special_sinkcommand_gamma_red,
	ambitv_special_sinkcommand_gamma_green,
	ambitv_special_sinkcommand_gamma_blue,
	ambitv_special_sinkcommand_intensity_red,
	ambitv_special_sinkcommand_intensity_green,
	ambitv_special_sinkcommand_intensity_blue,
	ambitv_special_sinkcommand_min_intensity_red,
	ambitv_special_sinkcommand_min_intensity_green,
	ambitv_special_sinkcommand_min_intensity_blue
};

enum ambitv_special_audiocommand
{
	ambitv_special_audiocommand_type = 1,
	ambitv_special_audiocommand_sensitivity,
	ambitv_special_audiocommand_smoothing,
	ambitv_special_audiocommand_linear
};

enum ambitv_conf_parser_state
{
	ambitv_conf_parser_state_toplevel,
	ambitv_conf_parser_state_block,
	ambitv_conf_parser_state_block_name,
	ambitv_conf_parser_state_block_name_done,
	ambitv_conf_parser_state_block_var_name,
	ambitv_conf_parser_state_block_var_name_done,
	ambitv_conf_parser_state_block_value,
	ambitv_conf_parser_state_block_value_done
};

struct ambitv_conf_parser
{
	enum ambitv_conf_parser_state state;

	char* path;
	unsigned current_line_num;

	char* current_block_name;
	char* current_var_name;
	char* current_value;

	int argidx, arglen;
	char** argv;

	int (*f_handle_block)(const char*, int, char**);
};

struct ambitv_conf_parser*
ambitv_conf_parser_create(void);

void
ambitv_conf_parser_free(struct ambitv_conf_parser* parser);

int
ambitv_conf_parser_read_config_file(struct ambitv_conf_parser* parser, const char* path);

#endif // __AMBITV_PARSE_CONF_H__

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
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "parse-conf.h"
#include "util.h"
#include "log.h"

#define CONF_MAX_LINE_LEN     256

#define LOGNAME   "parse-config: "

static void
wordclock_conf_parser_free_after_block(struct wordclock_conf_parser* parser)
{
   if (NULL != parser->current_block_name) {
      free(parser->current_block_name);
      parser->current_block_name = NULL;
   }
   if (NULL != parser->current_var_name) {
      free(parser->current_var_name);
      parser->current_var_name = NULL;
   }
   if (NULL != parser->current_value) {
      free(parser->current_value);
      parser->current_value = NULL;
   }

   if (NULL != parser->argv) {
      while (--parser->argidx >= 1) {
         free(parser->argv[parser->argidx]);
      }

      free(parser->argv);
      parser->argv = NULL;

      parser->arglen = 0;
      parser->argidx = 1;
   }
}

static void
wordclock_conf_parser_store_varname_and_value(struct wordclock_conf_parser* parser)
{
   char* varname = (char*)malloc((strlen(parser->current_var_name) + 3) * sizeof(char));
   sprintf(varname, "--%s", parser->current_var_name);
     
   parser->argidx = wordclock_util_append_ptr_to_list(
      (void***)&parser->argv,
      parser->argidx,
      &parser->arglen,
      varname);
         
   parser->argidx = wordclock_util_append_ptr_to_list(
      (void***)&parser->argv,
      parser->argidx,
      &parser->arglen,
      parser->current_value);
         
   free(parser->current_var_name);
   parser->current_var_name = parser->current_value = NULL;
}

static int
wordclock_conf_parser_finish_block(struct wordclock_conf_parser* parser)
{
   int ret = 0;
   
   wordclock_util_append_ptr_to_list(
      (void***)&parser->argv,
      parser->argidx,
      &parser->arglen,
      NULL);
   
   parser->argv[0] = parser->current_block_name;
   
   if (NULL != parser->f_handle_block)
      ret = parser->f_handle_block(parser->current_block_name, parser->argidx, parser->argv);
   
   return ret;
}

static int
wordclock_conf_parser_process_line(struct wordclock_conf_parser* parser, const char* line)
{
   char c;
   int ret = 0;
   char buf[CONF_MAX_LINE_LEN+1], *p = buf;
   
   while ((c = *line++)) {
      if ('\n' == c) {
         switch (parser->state) {
            case wordclock_conf_parser_state_block_value:
               parser->state = wordclock_conf_parser_state_block_value_done;
               *p = 0;
               parser->current_value = strdup(buf);
               p = buf;
               wordclock_conf_parser_store_varname_and_value(parser);
               break;
            case wordclock_conf_parser_state_block_name:
               parser->state = wordclock_conf_parser_state_block_name_done;
               *p = 0;
               parser->current_block_name = strdup(buf);
               p = buf;
               break;
            case wordclock_conf_parser_state_block_var_name_done:
            case wordclock_conf_parser_state_block_var_name:
               goto missing_value;
            default:
               break;
         }
         
         continue;
      }
      
      if ('#'== c || isspace((int)c)) {
         switch (parser->state) {
            case wordclock_conf_parser_state_block_name:
               parser->state = wordclock_conf_parser_state_block_name_done;
               *p = 0;
               parser->current_block_name = strdup(buf);
               p = buf;
               break;
            case wordclock_conf_parser_state_block_var_name:
               parser->state = wordclock_conf_parser_state_block_var_name_done;
               *p = 0;
               parser->current_var_name = strdup(buf);
               p = buf;
               break;
            case wordclock_conf_parser_state_block_value:
               parser->state = wordclock_conf_parser_state_block_value_done;
               *p = 0;
               parser->current_value = strdup(buf);
               p = buf;
               wordclock_conf_parser_store_varname_and_value(parser);
               break;
            default:
               break;
         }
       
         if ('#' == c) {
            break;
         }
         
         continue;
      }
      
      if ('{' == c) {
         switch (parser->state) {
            case wordclock_conf_parser_state_block_name:
               *p = 0;
               parser->current_block_name = strdup(buf);
               p = buf;
            case wordclock_conf_parser_state_block_name_done:
               parser->state = wordclock_conf_parser_state_block;
               continue;
            default:
               goto unexpected_paren_open;
         }
      }
      
      if ('}' == c) {
         switch (parser->state) {
            case wordclock_conf_parser_state_block_value:
            case wordclock_conf_parser_state_block_value_done:
            case wordclock_conf_parser_state_block:
               parser->state = wordclock_conf_parser_state_toplevel;
               
               ret = wordclock_conf_parser_finish_block(parser);
               if (ret < 0)
                  goto finish_block_failed;
               
               wordclock_conf_parser_free_after_block(parser);
                  
               continue;
            
            default:
               goto unexpected_paren_close;
         }
      }
      
      *p++ = c;
      
      switch (parser->state) {
         case wordclock_conf_parser_state_toplevel:
            parser->state = wordclock_conf_parser_state_block_name;
            break;
         case wordclock_conf_parser_state_block:
            parser->state = wordclock_conf_parser_state_block_var_name;
            break;
         case wordclock_conf_parser_state_block_var_name_done:
            parser->state = wordclock_conf_parser_state_block_value;
            break;
         case wordclock_conf_parser_state_block_name_done:
         case wordclock_conf_parser_state_block_value_done:
            goto unexpected_char;
         default:
            break;
      }
   }
   
   switch(parser->state) {
      case wordclock_conf_parser_state_block_value_done:
         parser->state = wordclock_conf_parser_state_block;
         break;
      default:
         break;
   }

   return ret;

missing_value:
   wordclock_log(wordclock_log_error, LOGNAME "%s, line %d: missing value.\n",
      parser->path, parser->current_line_num);
   goto err_return;
   
unexpected_paren_open:
   wordclock_log(wordclock_log_error, LOGNAME "%s, line %d: unexpected '{'.\n",
      parser->path, parser->current_line_num);
   goto err_return;

unexpected_paren_close:
   wordclock_log(wordclock_log_error, LOGNAME "%s, line %d: unexpected '}'.\n",
      parser->path, parser->current_line_num);
   goto err_return;

unexpected_char:
   wordclock_log(wordclock_log_error, LOGNAME "%s, line %d: unexpected character '%c'.\n",
      parser->path, parser->current_line_num, *(p-1));
   goto err_return;
   
finish_block_failed:
   wordclock_log(wordclock_log_error, LOGNAME "%s, line %d: failed to instantiate configuration block '%s'.\n",
      parser->path, parser->current_line_num, parser->current_block_name);

err_return:
   wordclock_conf_parser_free_after_block(parser);
   return -1;
}

struct wordclock_conf_parser*
wordclock_conf_parser_create(void)
{
   struct wordclock_conf_parser* parser =
      (struct wordclock_conf_parser*)malloc(sizeof(struct wordclock_conf_parser));
   
   if (NULL != parser)
      memset(parser, 0, sizeof(struct wordclock_conf_parser));
   
   return parser;
}

void
wordclock_conf_parser_free(struct wordclock_conf_parser* parser)
{   
   if (NULL != parser) {
      wordclock_conf_parser_free_after_block(parser);
      
      if (NULL != parser->path)
         free(parser->path);
      
      free(parser);
   }
}

int
wordclock_conf_parser_read_config_file(struct wordclock_conf_parser* parser, const char* path)
{
   FILE* f;
   int ret;
   char line[CONF_MAX_LINE_LEN+1];
   
   parser->state = wordclock_conf_parser_state_toplevel;
   parser->current_line_num = 1;
   parser->argidx = 1;
   
   f = fopen(path, "r");
   if (NULL == f) {
      wordclock_log(wordclock_log_error, LOGNAME "failed to open configuration file '%s'.\n",
         path);
      ret = -errno;
      goto finish;
   }
   
   parser->path = strdup(path);

   while (NULL != fgets(line, CONF_MAX_LINE_LEN, f)) {
      int len = strlen(line);
      
      if (line[len-1] != '\n' && !feof(f)) {
         wordclock_log(wordclock_log_error, LOGNAME "%s, line %d: line too long.\n",
            parser->path, parser->current_line_num);
         ret = -1;
         goto close_and_finish;
      }
      ret = wordclock_conf_parser_process_line(parser, line);
      if (ret < 0)
         goto close_and_finish;
      
      parser->current_line_num++;
   }
   
   if (feof(f))
      ret = 0;
   else
      ret = -errno;

close_and_finish:
   fclose(f);

finish:
   return ret;
}

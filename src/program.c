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

#include "program.h"
#include "util.h"
#include "component.h"
#include "log.h"

#define LOGNAME   "program: "

struct ambitv_program** ambitv_programs;
int ambitv_num_programs, ambitv_len_programs;

static struct ambitv_program* ambitv_current_program;

static int
ambitv_program_contains_component(struct ambitv_program* program, void* component)
{
   int i;
   
   for (i=0; i<program->num_components; i++)
      if (program->components[i] == component)
         return 1;
   
   return 0;
}

static int
ambitv_program_append_component_with_name(struct ambitv_program* program, const char* name)
{
   int ret = -1;
   
   void* component = ambitv_component_find_by_name(name);
   
   if (NULL != component) {
      program->num_components = ambitv_util_append_ptr_to_list(
         &program->components,
         program->num_components,
         &program->len_components,
         component
      );
      
      ambitv_log(ambitv_log_info, LOGNAME "'%s': appended component '%s'.\n",
         program->name, name);
      
      ret = 0;
   }
   
   return ret;
}

static int
ambitv_program_configure(struct ambitv_program* program, int argc, char** argv)
{
   int c, ret = 0;
   
   static struct option lopts[] = {
      { "activate", required_argument, 0, 'a' },
      { NULL, 0, 0, 0 }
   };
   
   optind = 0;
   while (1) {      
      c = getopt_long(argc, argv, "", lopts, NULL);
      
      if (c < 0)
         break;
         
      switch (c) {
         case 'a': {
            if ('&' != optarg[0]) {
               ambitv_log(ambitv_log_error, LOGNAME "component name is not prefixed with '&': '%s'.\n",
                  optarg);
               goto errReturn;
            }
            
            ret = ambitv_program_append_component_with_name(program, &optarg[1]);
            if (ret < 0) {
               ambitv_log(ambitv_log_error, LOGNAME "failed to find component with name '%s'.\n",
                  optarg);
               goto errReturn;
            }
            
            break;
         }
         
         default:
            break;
      }
   }
   
   if (optind < argc) {
      ambitv_log(ambitv_log_error, LOGNAME "extraneous argument '%s'.\n",
         argv[optind]);
      ret = -1;
   }

errReturn:
   return ret;
}

int
ambitv_program_enable(struct ambitv_program* program)
{
   int i;
   
   for (i=0; i<ambitv_num_programs; i++)
      if (0 == strcmp(program->name, ambitv_programs[i]->name)) {
         ambitv_log(ambitv_log_error, LOGNAME "a program with name '%s' is already registered.\n",
            program->name);
         return -1;
      }
   
   ambitv_num_programs = ambitv_util_append_ptr_to_list(
      (void***)&ambitv_programs,
      ambitv_num_programs,
      &ambitv_len_programs,
      program
   );
   
   ambitv_log(ambitv_log_info, LOGNAME "registered program '%s'.\n",
      program->name);
   
   return 0;
}

struct ambitv_program*
ambitv_program_create(const char* name, int argc, char** argv)
{
   int ret;
   
   struct ambitv_program* program =
      (struct ambitv_program*)malloc(sizeof(struct ambitv_program));
   
   if (NULL != program) {
      memset(program, 0, sizeof(struct ambitv_program));
      
      program->name = strdup(name);
      
      ret = ambitv_program_configure(program, argc, argv);
      
      if (ret < 0) {
         ambitv_program_free(program);
         program = NULL;
      }
   }
   
   return program;
}

void
ambitv_program_free(struct ambitv_program* program)
{
   if (NULL != program) {
      if (NULL != program->name)
         free(program->name);
      
      if (NULL != program->components)
         free(program->components);
      
      free(program);
   }
}

int
ambitv_program_run(struct ambitv_program* program)
{
   int i, ret = 0;
   
   if (NULL != ambitv_current_program) {
      for (i=0; i<ambitv_current_program->num_components; i++) {
         if (!ambitv_program_contains_component(program, ambitv_current_program->components[i])) {
            ret = ambitv_component_deactivate(ambitv_current_program->components[i]);
            if (ret < 0)
               goto errReturn;
         }
      }
   }
   
   ambitv_current_program = program;
   
   for (i=0; i<program->num_components; i++) {
      ret = ambitv_component_activate(program->components[i]);
      if (ret < 0)
         goto errReturn;
   }

errReturn:
   return ret;
}

int
ambitv_program_stop_current()
{
   int i, ret = 0;
   
   if (NULL != ambitv_current_program) {
      for (i=0; i<ambitv_current_program->num_components; i++) {
         ret = ambitv_component_deactivate(ambitv_current_program->components[i]);
         if (ret < 0)
            return ret;
      }
      
      ambitv_current_program = NULL;
   }
   
   return ret;
}

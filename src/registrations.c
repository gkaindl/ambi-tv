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
#include <unistd.h>

#include "registrations.h"
#include "component.h"
#include "program.h"
#include "log.h"

#include "components/v4l2-grab-source.h"
#include "components/timer-source.h"
#include "components/avg-color-processor.h"
#include "components/edge-color-processor.h"
#include "components/mood-light-processor.h"
#include "components/lpd8806-spidev-sink.h"

#define LOGNAME      "registration: "

struct ambitv_component_registration {
   char* name;
   void* (*constructor)(const char*, int, char**);
};

static struct ambitv_component_registration registrations[] = {
   {
      .name             = "v4l2-grab-source",
      .constructor      = (void* (*)(const char*, int, char**))ambitv_v4l2_grab_create
   },
   {
      .name             = "timer-source",
      .constructor      = (void* (*)(const char*, int, char**))ambitv_timer_source_create
   },
   {
      .name             = "avg-color-processor",
      .constructor      = (void* (*)(const char*, int, char**))ambitv_avg_color_processor_create
   },
   {
      .name             = "edge-color-processor",
      .constructor      = (void* (*)(const char*, int, char**))ambitv_edge_color_processor_create
   },
   {
      .name             = "mood-light-processor",
      .constructor      = (void* (*)(const char*, int, char**))ambitv_mood_light_processor_create
   },
   {
      .name             = "lpd8806-spidev-sink",
      .constructor      = (void* (*)(const char*, int, char**))ambitv_lpd8806_create
   },
   
   { NULL, NULL }
};

int
ambitv_register_component_for_name(const char* name, int argc, char** argv)
{
   int i, argv_copied = 0, ret = -1, aargc = argc;
   char** aargv = argv;
   struct ambitv_component_registration* r = registrations;
   
   while (NULL != r->name) {
      if (0 == strcmp(name, r->name)) {
         void* component;
         
         if (argc > 1) {
            for (i=1; i<argc; i+=2) {               
               if (0 == strcmp(argv[i], "--name")) {                  
                  name = argv[i+1];
                  
                  aargv = (char**)malloc(sizeof(char*) * (argc-1));
                  memcpy(aargv, argv, sizeof(char*) * i);
                  memcpy(&aargv[i], &argv[i+2], sizeof(char*) * (argc-i-1));
                  aargc = argc - 2;
                  
                  argv_copied = 1;
                  
                  break;
               }
            }
         }
         
         optind = 0;
         component = r->constructor(name, aargc, aargv);
         
         if (NULL != component) {
            
            if (ambitv_component_enable(component) < 0) {
               ambitv_log(ambitv_log_info, LOGNAME "failed to register component '%s' (class %s).\n",
                  name, r->name);
               ret = -4;
               goto returning;
            }
            
            ambitv_log(ambitv_log_info, LOGNAME "registered component '%s' (class %s).\n",
               name, r->name);
            
            ambitv_component_print_configuration(component);
            
            ret = 0;
            goto returning;
         } else {
            // TODO: error while constructing
            ret = -3;
            goto returning;
         }
      }
      
      r++;
   }
   
   ambitv_log(ambitv_log_error, LOGNAME "failed to find component class %s.\n",
      name);
   
returning:
   if (argv_copied) {
      free(aargv);
   }

   return ret;
}

int
ambitv_register_program_for_name(const char* name, int argc, char** argv)
{
   int ret = -1;
   
   struct ambitv_program* program = ambitv_program_create(name, argc, argv);
   if (NULL != program) {
      ret = ambitv_program_enable(program);
      
      if (ret < 0)
         ambitv_program_free(program);   
   }
   
   return ret;
}

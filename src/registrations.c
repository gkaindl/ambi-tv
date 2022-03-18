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
#include <string.h>
#include <unistd.h>

#include "registrations.h"
#include "component.h"
#include "program.h"
#include "log.h"

#include "components/audio-grab-source.h"
#include "components/timer-source.h"
#include "components/word-processor.h"
#include "components/audio-processor.h"
#include "components/mood-light-processor.h"
#include "components/ledstripe-sink.h"

#define LOGNAME      "registration: "

struct wordclock_component_registration {
   char* name;
   void* (*constructor)(const char*, int, char**);
};

static struct wordclock_component_registration registrations[] = {
   {
      .name             = "audio-grab-source",
      .constructor      = (void* (*)(const char*, int, char**))wordclock_audio_grab_create
   },
   {
      .name             = "timer-source",
      .constructor      = (void* (*)(const char*, int, char**))wordclock_timer_source_create
   },
   {
      .name             = "word-processor",
      .constructor      = (void* (*)(const char*, int, char**))wordclock_word_processor_create
   },
   {
      .name             = "audio-processor",
      .constructor      = (void* (*)(const char*, int, char**))wordclock_audio_processor_create
   },
  {
      .name             = "mood-light-processor",
      .constructor      = (void* (*)(const char*, int, char**))wordclock_mood_light_processor_create
   },
   {
      .name             = "ledstripe-sink",
      .constructor      = (void* (*)(const char*, int, char**))wordclock_ledstripe_create
   },

   { NULL, NULL }
};

int
wordclock_register_component_for_name(const char* name, int argc, char** argv)
{
   int i, argv_copied = 0, ret = -1, aargc = argc;
   char** aargv = argv;
   struct wordclock_component_registration* r = registrations;
   
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
            
            if (wordclock_component_enable(component) < 0) {
               wordclock_log(wordclock_log_info, LOGNAME "failed to register component '%s' (class %s).\n",
                  name, r->name);
               ret = -4;
               goto returning;
            }
            
            wordclock_log(wordclock_log_info, LOGNAME "registered component '%s' (class %s).\n",
               name, r->name);
            
            wordclock_component_print_configuration(component);
            
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
   
   wordclock_log(wordclock_log_error, LOGNAME "failed to find component class %s.\n",
      name);
   
returning:
   if (argv_copied) {
      free(aargv);
   }

   return ret;
}

int
wordclock_register_program_for_name(const char* name, int argc, char** argv)
{
   int ret = -1;
   
   struct wordclock_program* program = wordclock_program_create(name, argc, argv);
   if (NULL != program) {
      ret = wordclock_program_enable(program);
      
      if (ret < 0)
         wordclock_program_free(program);   
   }
   
   return ret;
}

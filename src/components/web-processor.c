/* 
*  Copyright (C) 2014 Dennis Schwertel
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

#include "web-processor.h"

#include "../color.h"
#include "../util.h"
#include "../log.h"


#define LOGNAME               "web: "

#define MAX_COUNT            150

FILE *fr;            /* declare the file pointer */

long count = MAX_COUNT;
long my_r = 0;
long my_g = 0;
long my_b = 0;


struct ambitv_web_processor {
   float    step;
   float    offset;
};

static int
ambitv_web_processor_handle_frame(
   struct ambitv_processor_component* component,
   void* frame,
   int width,
   int height,
   int bytesperline,
   enum ambitv_video_format fmt
) {
	count += 1;
	if (count > MAX_COUNT){
	  fr = fopen("/home/pi/rgb", "rt");
	  char line[80];
	  count = 0;
	  fgets(line, 80, fr);
	  sscanf (line, "%ld", &my_r);	
	  fgets(line, 80, fr);
	  sscanf (line, "%ld", &my_g);	
	  fgets(line, 80, fr);
	  sscanf (line, "%ld", &my_b);	
	  fclose(fr);
	}

   return 0;
}

static int
ambitv_web_processor_update_sink(
   struct ambitv_processor_component* processor,
   struct ambitv_sink_component* sink)
{
   int i, n_out, ret = 0;

   if (sink->f_num_outputs && sink->f_map_output_to_point && sink->f_set_output_to_rgb) {
      n_out = sink->f_num_outputs(sink);

      for (i=0; i<n_out; i++) {
         sink->f_set_output_to_rgb(sink, i,my_r, my_g,my_b);
      }
   } else
      ret = -1;

   if (sink->f_commit_outputs)
      sink->f_commit_outputs(sink);

   return ret;
}

static int
ambitv_web_processor_configure(struct ambitv_processor_component* mood, int argc, char** argv)
{
   int c, ret = 0;

   static struct option lopts[] = {
      { NULL, 0, 0, 0 }
   };

   while (1) {      
      c = getopt_long(argc, argv, "", lopts, NULL);

      if (c < 0)
         break;

      switch (c) {
         case 's': {
            if (NULL != optarg) {
               char* eptr = NULL;
               double nbuf = strtod(optarg, &eptr);

               if ('\0' == *eptr && nbuf > 0) {

               } else {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  return -1;
               }
            }

            break;
         }

         default:
            break;
      }
   }

   if (optind < argc) {
      ambitv_log(ambitv_log_error, LOGNAME "extraneous configuration argument: '%s'.\n",
         argv[optind]);
      ret = -1;
   }

   return ret;
}

static void
ambitv_web_processor_print_configuration(struct ambitv_processor_component* component)
{
   ambitv_log(ambitv_log_info,
      "\tnothing to configure  \n"
   );
}

static void
ambitv_web_processor_free(struct ambitv_processor_component* component)
{
   free(component->priv);
}

struct ambitv_processor_component*
ambitv_web_processor_create(const char* name, int argc, char** argv)
{
   struct ambitv_processor_component* web_processor =
      ambitv_processor_component_create(name);

   if (NULL != web_processor) {
      struct ambitv_web_processor* priv =
         (struct ambitv_web_processor*)malloc(sizeof(struct ambitv_web_processor));

      web_processor->priv = (void*)priv;

      web_processor->f_print_configuration  = ambitv_web_processor_print_configuration;
      web_processor->f_consume_frame        = ambitv_web_processor_handle_frame;
      web_processor->f_update_sink          = ambitv_web_processor_update_sink;
      web_processor->f_free_priv            = ambitv_web_processor_free;
      
      if (ambitv_web_processor_configure(web_processor, argc, argv) < 0)
         goto errReturn;
   }

   return web_processor;

errReturn:
   ambitv_processor_component_free(web_processor);
   return NULL;
}

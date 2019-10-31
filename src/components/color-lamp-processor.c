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
#include <string.h>

#include "color-lamp-processor.h"

#include "../color.h"
#include "../util.h"
#include "../log.h"

#define DEFAULT_STEP          0.04f
#define DEFAULT_H             21.f
#define DEFAULT_S             171.f
#define DEFAULT_V             255.f

#define LOGNAME               "color-lamp: "

struct ambitv_color_lamp_processor {
   float    hsv_start[3];
   float    hsv_cur[3];
   float    hsv_prev[3];
   float    hsv_target[3];
   float    pos, step;
};

static int
ambitv_color_lamp_processor_handle_frame(
   struct ambitv_processor_component* component,
   void* frame,
   int width,
   int height,
   int bytesperline,
   enum ambitv_video_format fmt
) {
   struct ambitv_color_lamp_processor* lamp =
      (struct ambitv_color_lamp_processor*)component->priv;
      
   lamp->pos = MIN(lamp->pos + lamp->step, 1.0f);
   
   if (lamp->pos < 1.0f) {
      float p = sinf(lamp->pos * M_PI_2);
      
      for (int i=0; i<3; i++) {
         lamp->hsv_cur[i] =
            lamp->hsv_prev[i] * (1.0f - p) + lamp->hsv_target[i] * p;
      }
   } else {
      for (int i=0; i<3; i++) {
         lamp->hsv_cur[i] = lamp->hsv_target[i];
      }
   }

   return 0;
}

static int
ambitv_color_lamp_processor_update_sink(
   struct ambitv_processor_component* processor,
   struct ambitv_sink_component* sink)
{
   int i, n_out, ret = 0;

   struct ambitv_color_lamp_processor* lamp =
      (struct ambitv_color_lamp_processor*)processor->priv;

   if (sink->f_num_outputs && sink->f_map_output_to_point && sink->f_set_output_to_rgb) {
      n_out = sink->f_num_outputs(sink);

      for (i=0; i<n_out; i++) {
         int hsv[3];
         int r, g, b;
         
         for (int j=0; j<3; j++) {
            hsv[j] = CONSTRAIN(roundf(lamp->hsv_cur[j]), 0, 255);
         }
         
         ambitv_hsv_to_rgb(hsv[0], hsv[1], hsv[2], &r, &g, &b);
         
         sink->f_set_output_to_rgb(sink, i, r, g, b);
      }
   } else
      ret = -1;

   if (sink->f_commit_outputs)
      sink->f_commit_outputs(sink);

   return ret;
}

static int
ambitv_color_lamp_processor_configure(struct ambitv_processor_component* lamp, int argc, char** argv)
{
   int c, ret = 0;

   struct ambitv_color_lamp_processor* lamp_priv =
      (struct ambitv_color_lamp_processor*)lamp->priv;
   if (NULL == lamp_priv)
      return -1;

   static struct option lopts[] = {
      { "transition_speed", required_argument, 0, 't' },
      { "hue", required_argument, 0, 'h' },
      { "saturation", required_argument, 0, 's' },
      { "value", required_argument, 0, 'v' },
      { NULL, 0, 0, 0 }
   };

   while (1) {      
      c = getopt_long(argc, argv, "", lopts, NULL);

      if (c < 0)
         break;

      switch (c) {
         case 't': {
            if (NULL != optarg) {
               char* eptr = NULL;
               double nbuf = strtod(optarg, &eptr);

               if (0 == *eptr && nbuf > 0) {
                  lamp_priv->step = nbuf / 1000.0;
               } else {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  return -1;
               }
            }

            break;
         }
         
         case 'h':
         case 's':
         case 'v': {
            if (NULL != optarg) {
               char* eptr = NULL;
               double val = strtod(optarg, &eptr);
               
               if (0 == *eptr) {
                  int p = 0;
                  switch (c) {
                     case 's': p = 1; break;
                     case 'v': p = 2; break;
                     default: break;
                  }
                  
                  val = CONSTRAIN(val, .0f, 255.f);
                  
                  lamp_priv->hsv_start[p]  =
                  lamp_priv->hsv_cur[p]    =
                  lamp_priv->hsv_target[p] = (float)val;
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
ambitv_color_lamp_processor_print_configuration(struct ambitv_processor_component* component)
{
   struct ambitv_color_lamp_processor* lamp =
      (struct ambitv_color_lamp_processor*)component->priv;

   ambitv_log(ambitv_log_info,
      "\ttransition_speed:    %.1f\n"
      "\tstarting hue:        %.0f\n"
      "\tstarting saturation: %.0f\n"
      "\tstarting value:      %.0f\n",
         lamp->step * 1000.0f,
         lamp->hsv_start[0],
         lamp->hsv_start[1],
         lamp->hsv_start[2]
   );
}

static int
ambitv_color_lamp_processor_provide_keypairs(
   struct ambitv_processor_component* component,
   void* ctx,
   void (*callback)(const char*, int, const char*, void*)
) {
   if (callback && component) {
      int i;
      const char* names[] = { "h", "s", "v" };
      
      struct ambitv_color_lamp_processor* lamp =
         (struct ambitv_color_lamp_processor*)component->priv;
      
      for (i=0; i<3; i++) {
         callback(
            names[i],
            CONSTRAIN(roundf(lamp->hsv_cur[i]), 0, 255),
            NULL,
            ctx
         );
      }
      
      callback("t", (int)roundf(lamp->step * 1000.0f), NULL, ctx);
   }
   
   return 0;
}

static int
ambitv_color_lamp_processor_receive_keypair(
   struct ambitv_processor_component* component, const char* name, const char* value
) {
   char* eptr;
   double dbl_val;
   int i, res = -1;
   struct ambitv_color_lamp_processor* lamp;
   
   if (!component || !name || !value) {
      return -1;
   }
   
   lamp = (struct ambitv_color_lamp_processor*)component->priv;
   
   eptr    = NULL;
   dbl_val = strtod(value, &eptr);
   
   if (0 == *eptr) {
      switch (*name) {
         case 'h':
         case 's':
         case 'v': {
            if (0 == name[1]) {
               int p = 0;
               
               switch (*name) {
                  case 's': p = 1; break;
                  case 'v': p = 2; break;
                  default: break;
               }
               
               lamp->hsv_target[p] = (float)dbl_val;
               lamp->pos = .0f;
               
               for (i=0; i<3; i++) {
                  lamp->hsv_prev[i] = lamp->hsv_cur[i];
               }
               
               res = 0;
            }
            
            break;
         }
         
         case 't': {
            if (0 == name[1]) {
               lamp->step = (float)dbl_val / 1000.0f;
               res = 0;
            }
            
            break;
         }
      
         default:
            break;
      }
   }
   
   return res;
}

static void
ambitv_color_lamp_processor_free(struct ambitv_processor_component* component)
{
   free(component->priv);
}

struct ambitv_processor_component*
ambitv_color_lamp_processor_create(const char* name, int argc, char** argv)
{
   struct ambitv_processor_component* color_lamp_processor =
      ambitv_processor_component_create(name);

   if (NULL != color_lamp_processor) {
      struct ambitv_color_lamp_processor* priv =
         (struct ambitv_color_lamp_processor*)malloc(sizeof(struct ambitv_color_lamp_processor));

      color_lamp_processor->priv = (void*)priv;
      
      priv->step = DEFAULT_STEP;
      priv->pos  = 1.0;
      priv->hsv_start[0] = priv->hsv_cur[0] = priv->hsv_target[0] = DEFAULT_H;
      priv->hsv_start[1] = priv->hsv_cur[1] = priv->hsv_target[1] = DEFAULT_S;
      priv->hsv_start[2] = priv->hsv_cur[2] = priv->hsv_target[2] = DEFAULT_V;

      color_lamp_processor->f_print_configuration  = ambitv_color_lamp_processor_print_configuration;
      color_lamp_processor->f_provide_keypairs     = ambitv_color_lamp_processor_provide_keypairs;
      color_lamp_processor->f_receive_keypair      = ambitv_color_lamp_processor_receive_keypair;
      color_lamp_processor->f_consume_frame        = ambitv_color_lamp_processor_handle_frame;
      color_lamp_processor->f_update_sink          = ambitv_color_lamp_processor_update_sink;
      color_lamp_processor->f_free_priv            = ambitv_color_lamp_processor_free;
      
      if (ambitv_color_lamp_processor_configure(color_lamp_processor, argc, argv) < 0)
         goto errReturn;
   }

   return color_lamp_processor;

errReturn:
   ambitv_processor_component_free(color_lamp_processor);
   return NULL;
}

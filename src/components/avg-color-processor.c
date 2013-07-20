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

#include "avg-color-processor.h"

#include "../video-fmt.h"

struct ambitv_avg_processor_priv {
   unsigned char rgb[3];
};

static int
ambitv_avg_color_processor_handle_frame(
   struct ambitv_processor_component* component,
   void* frame,
   int width,
   int height,
   int bytesperline,
   enum ambitv_video_format fmt
) {
   struct ambitv_avg_processor_priv* avg =
      (struct ambitv_avg_processor_priv*)component->priv;
   
   if (NULL != frame) {
      int coarseness = width/30;
      coarseness = coarseness ? coarseness : 1;
      
      ambitv_video_fmt_avg_rgb_for_block(avg->rgb, frame, 0, 0, width, height, bytesperline, fmt, coarseness);
   }
   
   return 0;
}

static int
ambitv_avg_color_processor_update_sink(
   struct ambitv_processor_component* processor,
   struct ambitv_sink_component* sink)
{
   int i, n_out, ret = 0;
   
   struct ambitv_avg_processor_priv* avg =
      (struct ambitv_avg_processor_priv*)processor->priv;

   if (sink->f_num_outputs && sink->f_set_output_to_rgb) {
      n_out = sink->f_num_outputs(sink);
      
      for (i=0; i<n_out; i++) {
         sink->f_set_output_to_rgb(sink, i, avg->rgb[0], avg->rgb[1], avg->rgb[2]);
      }
   } else
      ret = -1;
      
   if (sink->f_commit_outputs)
      sink->f_commit_outputs(sink);
   
   return ret;
}

static void
ambitv_avg_color_processor_free(struct ambitv_processor_component* component)
{
   free(component->priv);
}

struct ambitv_processor_component*
ambitv_avg_color_processor_create(const char* name, int argc, char** argv)
{
   struct ambitv_processor_component* avg_processor =
      ambitv_processor_component_create(name);
   
   if (NULL != avg_processor) {
      struct ambitv_avg_processor_priv* priv =
         (struct ambitv_avg_processor_priv*)malloc(sizeof(struct ambitv_avg_processor_priv));
      
      avg_processor->priv = (void*)priv;
      
      avg_processor->f_consume_frame = ambitv_avg_color_processor_handle_frame;
      avg_processor->f_update_sink = ambitv_avg_color_processor_update_sink;
      avg_processor->f_free_priv = ambitv_avg_color_processor_free;
   }
   
   return avg_processor;
}

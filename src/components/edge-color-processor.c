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

#include "edge-color-processor.h"
#include "edge-color-processor.r"

#include "../video-fmt.h"
#include "../util.h"
#include "../log.h"

#define DEFAULT_BOX_WIDTH     4
#define DEFAULT_BOX_HEIGHT    4

#define LOGNAME   "edge-color-processor: "

struct ambitv_edge_processor_priv {
   void* frame;
   int width, height, bytesperline, boxsize[2];
   enum ambitv_video_format fmt;
};

static int
ambitv_edge_color_processor_handle_frame(
   struct ambitv_processor_component* component,
   void* frame,
   int width,
   int height,
   int bytesperline,
   enum ambitv_video_format fmt
) {
   struct ambitv_edge_processor_priv* edge =
      (struct ambitv_edge_processor_priv*)component->priv;

   edge->frame          = frame;
   edge->width          = width;
   edge->height         = height;
   edge->bytesperline   = bytesperline;
   edge->fmt            = fmt;

   return 0;
}

void point_to_box(int *x, int *y, int w, int h, int width, int height)
{
   *x = CONSTRAIN(*x-w, 0, width);
   *y = CONSTRAIN(*y-h, 0, height);
}

static int
ambitv_edge_color_processor_update_sink(
   struct ambitv_processor_component* processor,
   struct ambitv_sink_component* sink)
{
   int i, n_out, ret = 0;

   struct ambitv_edge_processor_priv* edge =
      (struct ambitv_edge_processor_priv*)processor->priv;
   
   if (NULL == edge->frame)
      return 0;

   if (sink->f_num_outputs && sink->f_set_output_to_rgb && sink->f_map_output_to_point) {
      n_out = sink->f_num_outputs(sink);
      
      // TODO: CRAP
      /*static int n = 0;
      for (i=0; i<n_out; i++) {
         sink->f_set_output_to_rgb(sink, i, (i==n) ? 255 : 0, 0, 0);
      }
      n = (n+1)%n_out;*/

      for (i=0; i<n_out; i++) {
         unsigned char rgb[3];
         int x, y;
         
         if (0 == sink->f_map_output_to_point(sink, i, edge->width, edge->height, &x, &y)) {
            point_to_box(&x, &y, DEFAULT_BOX_WIDTH, DEFAULT_BOX_HEIGHT, edge->width, edge->height);
            ambitv_video_fmt_avg_rgb_for_block(rgb, edge->frame, x, y, DEFAULT_BOX_WIDTH, DEFAULT_BOX_HEIGHT, edge->bytesperline, edge->fmt, 4);
            
            sink->f_set_output_to_rgb(sink, i, rgb[0], rgb[1], rgb[2]);
         }
      }
   } else
      ret = -1;

   if (sink->f_commit_outputs)
      sink->f_commit_outputs(sink);

   return ret;
}

static int
ambitv_edge_color_processor_configure(struct ambitv_edge_processor_priv* edge, int argc, char** argv)
{
   int c, ret = 0;

   static struct option lopts[] = {
      { "box-width", required_argument, 0, '0' },
      { "box-height", required_argument, 0, '1' },
      { NULL, 0, 0, 0 }
   };

   optind = 0;
   while (1) {      
      c = getopt_long(argc, argv, "", lopts, NULL);

      if (c < 0)
         break;

      switch (c) {
         case '0':
         case '1': {
            if (NULL != optarg) {
               char* eptr = NULL;
               long nbuf = strtol(optarg, &eptr, 10);
            
               if ('\0' == *eptr && nbuf > 0) {
                  edge->boxsize[c-'0'] = (int)nbuf;
               } else {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  ret = -1;
                  goto errReturn;
               }
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

static void
ambitv_edge_color_processor_print_configuration(struct ambitv_processor_component* component)
{
   struct ambitv_edge_processor_priv* edge =
      (struct ambitv_edge_processor_priv*)component->priv;

   ambitv_log(ambitv_log_info,
      "\tbox-width:  %d\n"
      "\tbox-height: %d\n",
      edge->boxsize[0],
      edge->boxsize[1]
   );
}

static void
ambitv_edge_color_processor_free(struct ambitv_processor_component* component)
{
   free(component->priv);
}

struct ambitv_processor_component*
ambitv_edge_color_processor_create(const char* name, int argc, char** argv)
{
   struct ambitv_processor_component* edge_processor =
      ambitv_processor_component_create(name);

   if (NULL != edge_processor) {
      struct ambitv_edge_processor_priv* priv =
         (struct ambitv_edge_processor_priv*)malloc(sizeof(struct ambitv_edge_processor_priv));
      memset(priv, 9, sizeof(struct ambitv_edge_processor_priv));

      edge_processor->priv = (void*)priv;
      
      priv->boxsize[0]  = DEFAULT_BOX_WIDTH;
      priv->boxsize[1]  = DEFAULT_BOX_HEIGHT;

      edge_processor->f_print_configuration  = ambitv_edge_color_processor_print_configuration;
      edge_processor->f_consume_frame        = ambitv_edge_color_processor_handle_frame;
      edge_processor->f_update_sink          = ambitv_edge_color_processor_update_sink;
      edge_processor->f_free_priv            = ambitv_edge_color_processor_free;
      
      if (ambitv_edge_color_processor_configure(priv, argc, argv) < 0) {
         goto errReturn;
      }
   }

   return edge_processor;
   
errReturn:
   ambitv_processor_component_free(edge_processor);
   return NULL;
}

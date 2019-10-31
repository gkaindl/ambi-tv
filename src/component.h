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

#ifndef __AMBITV_COMPONENT_H__
#define __AMBITV_COMPONENT_H__

#include <pthread.h>

#include "video-fmt.h"

enum ambitv_component_type {
   ambitv_component_type_source,
   ambitv_component_type_processor,
   ambitv_component_type_sink
};

struct ambitv_sink_component {
   enum ambitv_component_type type;
   char*    name;
   
   void*    priv;
   int      active;

   void(*f_print_configuration)(struct ambitv_sink_component*);
   int (*f_provide_keypairs)(struct ambitv_sink_component*, void* ctx, void (*f)(const char*, int, const char*, void*));
   int (*f_receive_keypair)(struct ambitv_sink_component*, const char*, const char*);
   
   int (*f_start_sink)(struct ambitv_sink_component*);
   int (*f_stop_sink)(struct ambitv_sink_component*);
   int (*f_num_outputs)(struct ambitv_sink_component*);
   int (*f_map_output_to_point)(struct ambitv_sink_component*, int, int, int, int*, int*);
   int (*f_set_output_to_rgb)(struct ambitv_sink_component*, int, int, int, int);
   int (*f_commit_outputs)(struct ambitv_sink_component*);
   void(*f_free_priv)(struct ambitv_sink_component*);
};

struct ambitv_processor_component {
   enum ambitv_component_type type;
   char*    name;

   void*    priv;
   int      active;

   void(*f_print_configuration)(struct ambitv_processor_component*);
   int (*f_provide_keypairs)(struct ambitv_processor_component*, void* ctx, void (*f)(const char*, int, const char*, void*));
   int (*f_receive_keypair)(struct ambitv_processor_component*, const char*, const char*);
   
   int (*f_consume_frame)(struct ambitv_processor_component*, void*, int, int, int, enum ambitv_video_format);
   int (*f_update_sink)(struct ambitv_processor_component*, struct ambitv_sink_component*);
   void(*f_free_priv)(struct ambitv_processor_component*);
};

struct ambitv_source_component {
   enum ambitv_component_type type;
   char*          name;
   
   void*          priv;
   volatile int   active;
   
   void(*f_print_configuration)(struct ambitv_source_component*);
   int (*f_provide_keypairs)(struct ambitv_source_component*, void* ctx, void (*f)(const char*, int, const char*, void*));
   int (*f_receive_keypair)(struct ambitv_source_component*, const char*, const char*);
   
   pthread_t      thread;
   
   int (*f_start_source)(struct ambitv_source_component*);
   int (*f_run)(struct ambitv_source_component*);
   int (*f_stop_source)(struct ambitv_source_component*);
   void(*f_free_priv)(struct ambitv_source_component*);
};

extern void**        ambitv_components;

int
ambitv_component_enable(void* component);

void*
ambitv_component_find_by_name(const char* name);

void
ambitv_component_print_configuration(void* component);

int
ambitv_component_activate(void* component);

int
ambitv_component_deactivate(void* component);

int
ambitv_component_provide_all_keypairs(
   void* ctx,
   int (*handle_keypair)(const char* key, int value_int, const char* value_str, void* ctx)
);

int
ambitv_component_receive_keypair(
   void* component, const char* name, const char* value
);


struct ambitv_source_component*
ambitv_source_component_create(const char* name);

void
ambitv_source_component_distribute_to_active_processors(
   struct ambitv_source_component* compoment, void* frame_data, int width, int height, int bytesperline, enum ambitv_video_format fmt);

void
ambitv_source_component_free(struct ambitv_source_component* component);



struct ambitv_processor_component*
ambitv_processor_component_create(const char* name);

void
ambitv_processor_component_free(struct ambitv_processor_component* component);



struct ambitv_sink_component*
ambitv_sink_component_create(const char* name);

void
ambitv_sink_component_free(struct ambitv_sink_component* component);

#endif // __AMBITV_COMPONENT_H__
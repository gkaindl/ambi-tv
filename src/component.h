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

#ifndef __WORDCLOCK_COMPONENT_H__
#define __WORDCLOCK_COMPONENT_H__

#include <pthread.h>

enum wordclock_component_type {
   wordclock_component_type_source,
   wordclock_component_type_processor,
   wordclock_component_type_sink
};

struct wordclock_sink_component {
   enum wordclock_component_type type;
   char*    name;
   
   void*    priv;
   int      active;

   void(*f_print_configuration)(struct wordclock_sink_component*);
   int (*f_start_sink)(struct wordclock_sink_component*);
   int (*f_stop_sink)(struct wordclock_sink_component*);
   int (*f_num_outputs)(struct wordclock_sink_component*);
   int (*f_map_output_to_point)(struct wordclock_sink_component*, int, int, int, int*, int*);
   int (*f_set_output_to_rgb)(struct wordclock_sink_component*, int, int, int, int);
   int (*f_commit_outputs)(struct wordclock_sink_component*);
   void(*f_free_priv)(struct wordclock_sink_component*);
};

struct wordclock_processor_component {
   enum wordclock_component_type type;
   char*    name;

   void*    priv;
   int      active;
   int 		first_run;

   void(*f_print_configuration)(struct wordclock_processor_component*);
   
   int (*f_consume_frame)(struct wordclock_processor_component*, void*, int, int, int, int);
   int (*f_update_sink)(struct wordclock_processor_component*, struct wordclock_sink_component*);
   void(*f_free_priv)(struct wordclock_processor_component*);
};

struct wordclock_source_component {
   enum wordclock_component_type type;
   char*          name;
   
   void*          priv;
   volatile int   active;
   
   void(*f_print_configuration)(struct wordclock_source_component*);
   
   pthread_t      thread;
   
   int (*f_start_source)(struct wordclock_source_component*);
   int (*f_run)(struct wordclock_source_component*);
   int (*f_stop_source)(struct wordclock_source_component*);
   void(*f_free_priv)(struct wordclock_source_component*);
};

extern void**        wordclock_components;

int
wordclock_component_enable(void* component);

void*
wordclock_component_find_by_name(const char* name);

void*
wordclock_component_find_in_group(const char* name, int active);

void
wordclock_component_print_configuration(void* component);

int
wordclock_component_activate(void* component);

int
wordclock_component_deactivate(void* component);


struct wordclock_source_component*
wordclock_source_component_create(const char* name);

void
wordclock_source_component_distribute_to_active_processors(
   struct wordclock_source_component* compoment, void* frame_data, int width, int height, int bytesperline, int fmt);

void
wordclock_source_component_free(struct wordclock_source_component* component);



struct wordclock_processor_component*
wordclock_processor_component_create(const char* name);

void
wordclock_processor_component_free(struct wordclock_processor_component* component);



struct wordclock_sink_component*
wordclock_sink_component_create(const char* name);

void
wordclock_sink_component_free(struct wordclock_sink_component* component);

#endif // __WORDCLOCK_COMPONENT_H__

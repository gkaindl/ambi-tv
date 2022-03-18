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
#include "util.h"
#include "log.h"

#define LOGNAME   "component: "

struct wordclock_any_component {
   enum wordclock_component_type type;
   char*    name;
   void*    priv;
   int      active;
   void(*f_print_configuration)(void*);
};

void** wordclock_components;
static int wordclock_len_components, wordclock_num_components;

int
wordclock_component_enable(void* component)
{
   wordclock_num_components = wordclock_util_append_ptr_to_list(
      &wordclock_components,
      wordclock_num_components,
      &wordclock_len_components,
      component);
   
   return 0;
}

void*
wordclock_component_find_by_name(const char* name)
{
   int i;
   struct wordclock_any_component* component;
   for (i=0; i<wordclock_num_components; i++) {
      component = wordclock_components[i];
      if (0 == strcmp(component->name, name))
         return (void*)component;
   }
   
   return NULL;
}

void*
wordclock_component_find_in_group(const char* name, int active)
{
   int i;
   struct wordclock_any_component* component;
   for (i=0; i<wordclock_num_components; i++) {
      component = wordclock_components[i];
      if (strstr(component->name, name) && ((!active) || (active && component->active)))
         return (void*)component;
   }

   return NULL;
}

void
wordclock_component_print_configuration(void* component)
{
   struct wordclock_any_component* any_component =
      (struct wordclock_any_component*)component;
   
   if (any_component->f_print_configuration)
      any_component->f_print_configuration(component);
}

static void*
wordclock_source_component_thread_main(void* arg)
{
   struct wordclock_source_component* component =
      (struct wordclock_source_component*)arg;
   
   while(component->active) {
      if (NULL != component->f_run)
         component->f_run(component);
      else
         sleep(1);
   }
   
   return NULL;
}

static int
wordclock_source_component_activate(struct wordclock_source_component* component)
{
   int ret = 0;
   
   if (!component->active) {
      pthread_attr_t attr;
      
      if (NULL != component->f_start_source) {
         ret = component->f_start_source(component);
      } else {
         // TODO: no start method!
         ret = -1;
      }
      
      if (ret < 0)
         goto errReturn;
      
      component->active = 1;
      
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
      
      ret = pthread_create(
         &component->thread,
         &attr,
         wordclock_source_component_thread_main,
         (void*)component); 
      
      pthread_attr_destroy(&attr);
      
      if (ret != 0) {
         component->active = 0;
         // TODO: couldn't start thread.
      }
   }

errReturn:
   return ret;
}

static int
wordclock_processor_component_activate(struct wordclock_processor_component* component)
{
   component->active = 1;
   component->first_run = 1;
   return 0;
}

static int
wordclock_sink_component_activate(struct wordclock_sink_component* component)
{
   int ret = 0;
   
   if (!component->active) {
      if (NULL != component->f_start_sink)
         ret = component->f_start_sink(component);
      else {
         // TODO: no start method!
         ret = -1;
      }
      
      if (ret < 0)
         goto errReturn;
      
      component->active = 1;
   }

errReturn:
   return ret;
}

static int
wordclock_source_component_deactivate(struct wordclock_source_component* component)
{
   int ret = 0;

   if (component->active) {
      void* status;
      
      component->active = 0;
      
      ret = pthread_join(component->thread, &status);
      
      if (ret != 0) {
         // TODO: failed to join thread!
         goto errReturn;
      }
            
      if (NULL != component->f_stop_source)
         component->f_stop_source(component);
   }

errReturn:
   return ret;
}

static int
wordclock_processor_component_deactivate(struct wordclock_processor_component* component)
{
   component->active = 0;
   return 0;
}

static int
wordclock_sink_component_deactivate(struct wordclock_sink_component* component)
{
   int ret = 0;
   
   if (component->active) {
      component->active = 0;
      
      if (NULL != component->f_stop_sink)
         (void)component->f_stop_sink(component);  
   }
   
   return ret;
}

int
wordclock_component_activate(void* component)
{
   int ret = -1;
   
   struct wordclock_any_component* any_component =
      (struct wordclock_any_component*)component;
   
   wordclock_log(wordclock_log_info, LOGNAME "activating component '%s'...\n",
      any_component->name);
   
   switch(any_component->type) {
      case wordclock_component_type_source:
         ret = wordclock_source_component_activate(
            (struct wordclock_source_component*)component);
         break;
      case wordclock_component_type_processor:
         ret = wordclock_processor_component_activate(
            (struct wordclock_processor_component*)component);
         break;
      case wordclock_component_type_sink:
         ret = wordclock_sink_component_activate(
            (struct wordclock_sink_component*)component);
         break;
      default:
         break;
   }
   
   return ret;
}

int
wordclock_component_deactivate(void* component)
{
   int ret = -1;
   
   struct wordclock_any_component* any_component =
      (struct wordclock_any_component*)component;
   
   wordclock_log(wordclock_log_info, LOGNAME "deactivating component '%s'...\n",
      any_component->name);
   
   switch(any_component->type) {
      case wordclock_component_type_source:
         ret = wordclock_source_component_deactivate(
            (struct wordclock_source_component*)component);
         break;
      case wordclock_component_type_processor:
         ret = wordclock_processor_component_deactivate(
            (struct wordclock_processor_component*)component);
         break;
      case wordclock_component_type_sink:
         ret = wordclock_sink_component_deactivate(
            (struct wordclock_sink_component*)component);
         break;
      default:
         break;
   }
   
   return ret;
}

struct wordclock_source_component*
wordclock_source_component_create(const char* name)
{
   struct wordclock_source_component* component;
   
   component = (struct wordclock_source_component*)malloc(sizeof(struct wordclock_source_component));
   
   if (NULL != component) {
      memset(component, 0, sizeof(*component));
      
      component->type = wordclock_component_type_source;
      component->name = strdup(name);
   }
   
   return component;
}

void
wordclock_source_component_distribute_to_active_processors(
   struct wordclock_source_component* compoment, void* frame_data, int width, int height, int bytesperline, int fmt)
{
   int i, j;
   
   for (i=0; i<wordclock_num_components; i++) {
      struct wordclock_any_component* component =
         (struct wordclock_any_component*)wordclock_components[i];
      
      if (wordclock_component_type_processor == component->type) {
         struct wordclock_processor_component* processor =
            (struct wordclock_processor_component*)component;
         
         if (processor->active && NULL != processor->f_consume_frame)
            (void)processor->f_consume_frame(processor, frame_data, width, height, bytesperline, fmt);
                  
         if (processor->active && NULL != processor->f_update_sink) {
            for (j=0; j<wordclock_num_components; j++) {
               struct wordclock_any_component* any_sink_component =
                  (struct wordclock_any_component*)wordclock_components[j];
               
               if (wordclock_component_type_sink == any_sink_component->type) {
                  struct wordclock_sink_component* sink =
                     (struct wordclock_sink_component*)any_sink_component;
                  
                  if (sink->active)
                     (void)processor->f_update_sink(processor, sink);
               }
            }
         }
      }
   }
}

void
wordclock_source_component_free(struct wordclock_source_component* component)
{
   if (NULL != component) {
      if (NULL != component->name)
         free(component->name);
      
      if (NULL != component->f_free_priv)
         component->f_free_priv(component);
      
      free(component);
   }
}

struct wordclock_processor_component*
wordclock_processor_component_create(const char* name)
{
   struct wordclock_processor_component* component;
   
   component = (struct wordclock_processor_component*)malloc(sizeof(struct wordclock_processor_component));
   
   if (NULL != component) {
      memset(component, 0, sizeof(*component));
      
      component->type = wordclock_component_type_processor;
      component->name = strdup(name);
   }
   
   return component;
}

void
wordclock_processor_component_free(struct wordclock_processor_component* component)
{
   if (NULL != component) {
      if (NULL != component->name)
         free(component->name);

      if (NULL != component->f_free_priv)
         component->f_free_priv(component);
      
      free(component);
   }
}



struct wordclock_sink_component*
wordclock_sink_component_create(const char* name)
{
   struct wordclock_sink_component* component;

   component = (struct wordclock_sink_component*)malloc(sizeof(struct wordclock_sink_component));

   if (NULL != component) {
      memset(component, 0, sizeof(*component));

      component->type = wordclock_component_type_sink;
      component->name = strdup(name);
   }

   return component;
}

void
wordclock_sink_component_free(struct wordclock_sink_component* component)
{
   if (NULL != component) {
      if (NULL != component->name)
         free(component->name);
      
      if (NULL != component->f_free_priv)
         component->f_free_priv(component);
      
      free(component);
   }
}

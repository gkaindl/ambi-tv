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

struct ambitv_any_component {
   enum ambitv_component_type type;
   char*    name;
   void*    priv;
   int      active;
   
   void(*f_print_configuration)(void*);
   int (*f_provide_keypairs)(struct ambitv_any_component*, void* ctx, int (*f)(const char*, int, const char*, void*));
   int (*f_receive_keypair)(struct ambitv_any_component*, const char*, const char*);
};

void** ambitv_components;
static int ambitv_len_components, ambitv_num_components;

int
ambitv_component_enable(void* component)
{
   ambitv_num_components = ambitv_util_append_ptr_to_list(
      &ambitv_components,
      ambitv_num_components,
      &ambitv_len_components,
      component);
   
   return 0;
}

void*
ambitv_component_find_by_name(const char* name)
{
   int i;
   struct ambitv_any_component* component;
   
   for (i=0; i<ambitv_num_components; i++) {
      component = ambitv_components[i];
      
      if (0 == strcmp(component->name, name))
         return (void*)component;
   }
   
   return NULL;
}

void
ambitv_component_print_configuration(void* component)
{
   struct ambitv_any_component* any_component =
      (struct ambitv_any_component*)component;
   
   if (any_component->f_print_configuration)
      any_component->f_print_configuration(component);
}

#define KEYPAIR_CTX_BUFSIZE      64

struct keypair_ctx {
   void* forwarded_ctx;
   char* name;
   char buf[KEYPAIR_CTX_BUFSIZE];
   int (*handle_keypair)(const char*, int, const char*, void*);
};

static int
ambit_tv_component_handle_keypair(
   const char* name, int value_int, const char* value_str, void* cctx
) {
   int res = -1;
   struct keypair_ctx* ctx = (struct keypair_ctx*)cctx;
   
   if (ctx && name) {
      int comp_len, name_len;
      
      comp_len = strlen(ctx->name);
      name_len = strlen(name);
      
      if (comp_len + name_len + 2 < KEYPAIR_CTX_BUFSIZE) {
         snprintf(
            ctx->buf,
            KEYPAIR_CTX_BUFSIZE-1,
            "%s~%s",
            ctx->name,
            name
         );
            
         ctx->buf[KEYPAIR_CTX_BUFSIZE-1] = 0;
         
         ctx->handle_keypair(
            ctx->buf,
            value_int,
            value_str,
            ctx->forwarded_ctx
         );
            
         res = 0;
      }
   }
   
   return res;
}

int
ambitv_component_provide_all_keypairs(
   void* ctx,
   int (*handle_keypair)(const char*, int, const char*, void*)
)
{
   int i, res;
   struct keypair_ctx k_ctx;
   struct ambitv_any_component* component;
   
   if (!handle_keypair) {
      return -1;
   }
   
   k_ctx.forwarded_ctx  = ctx;
   k_ctx.handle_keypair = handle_keypair;
   
   for (i=0; i<ambitv_num_components; i++) {
      component = ambitv_components[i];
      
      if (component->f_provide_keypairs) {
         k_ctx.name = component->name;
         
         res = component->f_provide_keypairs(
            component,
            &k_ctx,
            ambit_tv_component_handle_keypair
         );
         
         if (res) {
            return res;
         }
      }
   }
   
   return 0;
}

int
ambitv_component_receive_keypair(
   void* component, const char* name, const char* value
) {
   int res = -1;
   
   if (component) {
      struct ambitv_any_component* c =
         (struct ambitv_any_component*)component;
      
      if (c->f_receive_keypair) {
         res = c->f_receive_keypair(c, name, value);
      }
   }
   
   return res;
}

static void*
ambitv_source_component_thread_main(void* arg)
{
   struct ambitv_source_component* component =
      (struct ambitv_source_component*)arg;
   
   while(component->active) {
      if (NULL != component->f_run)
         component->f_run(component);
      else
         sleep(1);
   }
   
   return NULL;
}

static int
ambitv_source_component_activate(struct ambitv_source_component* component)
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
         ambitv_source_component_thread_main,
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
ambitv_processor_component_activate(struct ambitv_processor_component* component)
{
   component->active = 1;
   return 0;
}

static int
ambitv_sink_component_activate(struct ambitv_sink_component* component)
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
ambitv_source_component_deactivate(struct ambitv_source_component* component)
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
ambitv_processor_component_deactivate(struct ambitv_processor_component* component)
{
   component->active = 0;
   return 0;
}

static int
ambitv_sink_component_deactivate(struct ambitv_sink_component* component)
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
ambitv_component_activate(void* component)
{
   int ret = -1;
   
   struct ambitv_any_component* any_component =
      (struct ambitv_any_component*)component;
   
   ambitv_log(ambitv_log_info, LOGNAME "activating component '%s'...\n",
      any_component->name);
   
   switch(any_component->type) {
      case ambitv_component_type_source:
         ret = ambitv_source_component_activate(
            (struct ambitv_source_component*)component);
         break;
      case ambitv_component_type_processor:
         ret = ambitv_processor_component_activate(
            (struct ambitv_processor_component*)component);
         break;
      case ambitv_component_type_sink:
         ret = ambitv_sink_component_activate(
            (struct ambitv_sink_component*)component);
         break;
      default:
         break;
   }
   
   return ret;
}

int
ambitv_component_deactivate(void* component)
{
   int ret = -1;
   
   struct ambitv_any_component* any_component =
      (struct ambitv_any_component*)component;
   
   ambitv_log(ambitv_log_info, LOGNAME "deactivating component '%s'...\n",
      any_component->name);
   
   switch(any_component->type) {
      case ambitv_component_type_source:
         ret = ambitv_source_component_deactivate(
            (struct ambitv_source_component*)component);
         break;
      case ambitv_component_type_processor:
         ret = ambitv_processor_component_deactivate(
            (struct ambitv_processor_component*)component);
         break;
      case ambitv_component_type_sink:
         ret = ambitv_sink_component_deactivate(
            (struct ambitv_sink_component*)component);
         break;
      default:
         break;
   }
   
   return ret;
}

struct ambitv_source_component*
ambitv_source_component_create(const char* name)
{
   struct ambitv_source_component* component;
   
   component = (struct ambitv_source_component*)malloc(sizeof(struct ambitv_source_component));
   
   if (NULL != component) {
      memset(component, 0, sizeof(*component));
      
      component->type = ambitv_component_type_source;
      component->name = strdup(name);
   }
   
   return component;
}

void
ambitv_source_component_distribute_to_active_processors(
   struct ambitv_source_component* compoment, void* frame_data, int width, int height, int bytesperline, enum ambitv_video_format fmt)
{
   int i, j;
   
   for (i=0; i<ambitv_num_components; i++) {
      struct ambitv_any_component* component =
         (struct ambitv_any_component*)ambitv_components[i];
      
      if (ambitv_component_type_processor == component->type) {
         struct ambitv_processor_component* processor =
            (struct ambitv_processor_component*)component;
         
         if (processor->active && NULL != processor->f_consume_frame)
            (void)processor->f_consume_frame(processor, frame_data, width, height, bytesperline, fmt);
                  
         if (processor->active && NULL != processor->f_update_sink) {
            for (j=0; j<ambitv_num_components; j++) {
               struct ambitv_any_component* any_sink_component =
                  (struct ambitv_any_component*)ambitv_components[j];
               
               if (ambitv_component_type_sink == any_sink_component->type) {
                  struct ambitv_sink_component* sink =
                     (struct ambitv_sink_component*)any_sink_component;
                  
                  if (sink->active)
                     (void)processor->f_update_sink(processor, sink);
               }
            }
         }
      }
   }
}

void
ambitv_source_component_free(struct ambitv_source_component* component)
{
   if (NULL != component) {
      if (NULL != component->name)
         free(component->name);
      
      if (NULL != component->f_free_priv)
         component->f_free_priv(component);
      
      free(component);
   }
}

struct ambitv_processor_component*
ambitv_processor_component_create(const char* name)
{
   struct ambitv_processor_component* component;
   
   component = (struct ambitv_processor_component*)malloc(sizeof(struct ambitv_processor_component));
   
   if (NULL != component) {
      memset(component, 0, sizeof(*component));
      
      component->type = ambitv_component_type_processor;
      component->name = strdup(name);
   }
   
   return component;
}

void
ambitv_processor_component_free(struct ambitv_processor_component* component)
{
   if (NULL != component) {
      if (NULL != component->name)
         free(component->name);

      if (NULL != component->f_free_priv)
         component->f_free_priv(component);
      
      free(component);
   }
}



struct ambitv_sink_component*
ambitv_sink_component_create(const char* name)
{
   struct ambitv_sink_component* component;

   component = (struct ambitv_sink_component*)malloc(sizeof(struct ambitv_sink_component));

   if (NULL != component) {
      memset(component, 0, sizeof(*component));

      component->type = ambitv_component_type_sink;
      component->name = strdup(name);
   }

   return component;
}

void
ambitv_sink_component_free(struct ambitv_sink_component* component)
{
   if (NULL != component) {
      if (NULL != component->name)
         free(component->name);
      
      if (NULL != component->f_free_priv)
         component->f_free_priv(component);
      
      free(component);
   }
}

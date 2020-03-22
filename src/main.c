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
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <termios.h>

#include "parse-conf.h"
#include "registrations.h"
#include "component.h"
#include "program.h"
#include "gpio.h"
#include "util.h"
#include "http.h"
#include "log.h"

#define LOGNAME      "main: "

#define DEFAULT_CONFIG_PATH   "/etc/ambi-tv.conf"
#define DEFAULT_HTTP_PORT     40100
#define BUTTON_MILLIS         250
#define BUTTON_MILLIS_HYST    10

struct ambitv_main_conf {
   int            program_idx;
   int            gpio_idx;
   int            http_on;
   char*          http_addr;
   unsigned       http_port;
   char*          config_path;
   
   int            cur_prog, ambitv_on, gpio_fd;
   int            button_cnt;
   struct timeval last_button_press;
   volatile int   running;
};

static struct ambitv_main_conf conf;
static http* http_backend;

static int
ambitv_main_configure(int argc, char** argv, int from_file);

static void
ambitv_signal_handler(int signum)
{
   signal(signum, SIG_IGN);
   conf.running = 0;
}

static int
ambitv_handle_config_block(const char* name, int argc, char** argv)
{
   int ret = 0;
   
   switch(name[0]) {
      case '&':
         ret = ambitv_register_program_for_name(&name[1], argc, argv);
         break;
      
      case '@':
         if (0 == strcmp(&name[1], "main")) {
            ret = ambitv_main_configure(argc, argv, 1);
         }
         break;
      
      default:
         ret = ambitv_register_component_for_name(name, argc, argv);
         break;
   }
   
   return ret;
}

static long
ambitv_millis_between(struct timeval* now, struct timeval* earlier)
{
   return (long)((now->tv_sec - earlier->tv_sec) * 1000) +
      (long)((now->tv_usec - earlier->tv_usec) / 1000);
}

static int
ambitv_select_program(int program_idx)
{
   int ret = 0;
   
   if (program_idx >= ambitv_num_programs) {
      return -1;
   }
   
   conf.cur_prog = program_idx;
   
   if (conf.ambitv_on) {
      ret = ambitv_program_run(ambitv_programs[conf.cur_prog]);
   
      if (ret < 0) {
         ambitv_log(ambitv_log_error, LOGNAME "failed to switch to program '%s', aborting...\n",
            ambitv_programs[conf.cur_prog]->name);
      } else {
         ambitv_log(ambitv_log_info, LOGNAME "switched to program '%s'.\n",
            ambitv_programs[conf.cur_prog]->name);
      }
   }
   
   return ret;
}

static int
ambitv_cycle_next_program()
{   
   if (!conf.ambitv_on) {
      ambitv_log(ambitv_log_info,
         LOGNAME "not cycling program, because state is paused.\n");
      
      return 0;
   }
   
   return ambitv_select_program((conf.cur_prog + 1) % ambitv_num_programs);
}
   
static int
ambitv_set_on(int is_on)
{
   int ret = 0;
   
   if (is_on == conf.ambitv_on) {
      return 0;
   }
   
   conf.ambitv_on = is_on;
   
   if (conf.ambitv_on) {
      ret = ambitv_program_run(ambitv_programs[conf.cur_prog]);
      if (ret < 0)
         ambitv_log(ambitv_log_error, LOGNAME "failed to start program '%s'.\n",
            ambitv_programs[conf.cur_prog]->name);
   } else {
      ret = ambitv_program_stop_current();
      if (ret < 0)
         ambitv_log(ambitv_log_error, LOGNAME "failed to stop program '%s'.\n",
            ambitv_programs[conf.cur_prog]->name);
   }
   
   ambitv_log(ambitv_log_info, LOGNAME "now: %s\n",
      conf.ambitv_on ? "running" : "paused");
   
   return ret;
}

static int
ambitv_toggle_on()
{
   return ambitv_set_on(!conf.ambitv_on);
}

static int
ambitv_consume_http_keypair(
   const char* name, int value_int, const char* value_str, void* ctx
)
{
   int res = 0;
   http* server = (http*)ctx;
   
   if (!name || !server) {
      return -1;
   }
   
   if (value_str) {
      res = http_reply_keypair(server, name, value_str);
   } else {
      res = http_reply_keypair_int(server, name, value_int);
   }
   
   return res;
}

static int
ambitv_handle_http_backend_keypair(
   http* server, const char* name, const char* value
) {
   int success = -1;
   
   if (!name || !value) {
      return -1;
   }
   
   switch (*name) {
      case 'o': {
         if (0 == strcmp("on", name)) {
            success = ambitv_set_on(atoi(value));
         }
         break;
      }
      
      case 'p': {
         if (0 == strcmp("program", name)) {
            success = ambitv_select_program(atoi(value));
         }
         break;
      }
      
      case 'q': {
         if (0 == strcmp("query", name) && atoi(value)) {
            success =
               http_reply_keypair_int(server, "on", conf.ambitv_on);
            success = success ||
               http_reply_keypair_int(server, "program", conf.cur_prog);
            success = success ||
               ambitv_component_provide_all_keypairs(
                  server,
                  ambitv_consume_http_keypair
            );
         }
         break;
      }
      
      default: {
         char* pos = strchr(name, (int)'~');
         
         if (pos) {
            char* valname = (pos+1);
            *pos = 0;
            
            success = ambitv_component_receive_keypair(
               ambitv_component_find_by_name(name),
               valname,
               value
            );
               
            *pos = '~';
         }
         
         break;
      }
   }
   
   return success;
}

static int
ambitv_runloop(int is_tty)
{
   int ret = 0, max_fd = -1;
   unsigned char c = 0;
   fd_set read_fds, write_fds, ex_fds;
   struct timeval tv;
   
   FD_ZERO(&read_fds); FD_ZERO(&write_fds); FD_ZERO(&ex_fds);
   
   if (conf.gpio_fd >= 0) {
      FD_SET(conf.gpio_fd, &ex_fds);
      max_fd = conf.gpio_fd;
   }
   
   if (is_tty) {
      FD_SET(STDIN_FILENO, &read_fds);
      max_fd = MAX(STDIN_FILENO, conf.gpio_fd);
   }
   
   if (http_backend) {
      max_fd = http_fill_fd_sets(http_backend, &read_fds, &write_fds, max_fd);
   }
   
   tv.tv_sec   = 0;
   tv.tv_usec  = 500000;
   
   ret = select(max_fd+1, &read_fds, &write_fds, &ex_fds, &tv);
      
   if (ret < 0) {
      if (EINTR != errno && EWOULDBLOCK != errno) {
         ambitv_log(ambitv_log_error, LOGNAME "error during select(): %d (%s)\n",
            errno, strerror(errno));
         ret = 0;
      }
      
      goto finishLoop;
   }
   
   if (is_tty && FD_ISSET(STDIN_FILENO, &read_fds)) {
      ret = read(STDIN_FILENO, &c, 1);
      
      if (ret < 0) {
         if (EINTR != errno && EWOULDBLOCK != errno) {
            ambitv_log(ambitv_log_error, LOGNAME "error during read() on stdin: %d (%s)\n",
               errno, strerror(errno));
         } else
            ret = 0;
         
         goto finishLoop;
      } else if (0 == ret)
         goto finishLoop;
      
      switch (c) {
         case 0x20: { // space
            ret = ambitv_cycle_next_program();
            if (ret < 0)
               goto finishLoop;
            
            break;
         }
         
         case 't': {
            ret = ambitv_toggle_on();
            if (ret < 0)
               goto finishLoop;
            
            break;
         }
         
         default:
            break;
      }
   }
   
   if (conf.gpio_fd >= 0 && FD_ISSET(conf.gpio_fd, &ex_fds)) {
      char buf[16];
      
      ret = read(conf.gpio_fd, buf, sizeof(*buf));
      lseek(conf.gpio_fd, 0, SEEK_SET);
      
      if (ret < 0) {
         if (EINTR != errno && EWOULDBLOCK != errno) {
            ambitv_log(ambitv_log_error, LOGNAME "failed to read from gpio %d.\n",
               conf.gpio_idx);
            ret = -1;
         } else
            ret = 0;
         
         goto finishLoop;
      } else if (0 == ret)
         goto finishLoop;
      
      if ('0' == buf[0]) {
         struct timeval now;
                         
         (void)gettimeofday(&now, NULL);
         if (0 != conf.last_button_press.tv_sec) {
            long millis = ambitv_millis_between(&now, &conf.last_button_press);
                        
            if (millis <= BUTTON_MILLIS && BUTTON_MILLIS_HYST <= millis)
               conf.button_cnt++;
         } else
            conf.button_cnt++;
                  
         memcpy(&conf.last_button_press, &now, sizeof(struct timeval));
      }
   }
   
   if (conf.button_cnt > 0) {
      struct timeval now;
      (void)gettimeofday(&now, NULL);
      if (0 != conf.last_button_press.tv_sec) {
         long millis = ambitv_millis_between(&now, &conf.last_button_press);
               
         if (millis > BUTTON_MILLIS) {
            if (conf.button_cnt > 1) {
               ret = ambitv_cycle_next_program();
            } else {
               ret = ambitv_toggle_on();
            }
            
            conf.button_cnt = 0;
            memset(&conf.last_button_press, 0, sizeof(struct timeval));
         }
      }
   }
   
   if (http_backend) {
      http_handle_fd_sets(http_backend, &read_fds, &write_fds);
   }

finishLoop:
   return ret;
}

static void
ambitv_usage(const char* name)
{
   const char* p = name + strlen(name);
   while (p != name && *p != '/') p--;
   if ('/' == *p) p++;
   
   printf(
      "usage: %s [options]\n"
      "\n"
      "options:\n"
      "\t-b/--button_gpio [i]     gpio pin to use as physical button. function disabled if i < 0. (default: -1).\n"
      "\t-f/--file [path]         use the configuration file at [path] (default: %s).\n"
      "\t-h,--help                display this help text.\n"
      "\t-p,--program [i]         run the [i]-th program from the configuration file on start-up.\n"
      "\t-s,--http_backend [0|1]  enable or disable the http api backend (0 to disable, 1 to enable)\n"
      "\t-a,--http_address [ip]   optionally specifiy a local IP address for the http backend, like 127.0.0.1\n"
      "\t-l,--http_port [port]    specify a listening port for the http backend. defaults to %u\n"
      "\n",
         p, DEFAULT_CONFIG_PATH, DEFAULT_HTTP_PORT
   );
}

static int
ambitv_main_configure(int argc, char** argv, int from_file)
{
   int c, ret = 0;

   optind = 0;

   static struct option lopts[] = {
      { "button_gpio", required_argument, 0, 'b' },
      { "file", required_argument, 0, 'f' },
      { "help", no_argument, 0, 'h' },
      { "program", required_argument, 0, 'p' },
      { "http_backend", required_argument, 0, 's' },
      { "http_address", required_argument, 0, 'a' },
      { "http_port", required_argument, 0, 'l' },
      { NULL, 0, 0, 0 }
   };

   while (1) {      
      c = getopt_long(argc, argv, "b:f:hp:", lopts, NULL);

      if (c < 0)
         break;

      switch (c) {
         case 'f': {
            if (from_file) {
               ambitv_log(ambitv_log_error,
                  LOGNAME "\"file\" argument can't be used in config file.\n");
               return -1;
            }
            
            if (NULL != optarg) {
               conf.config_path = strdup(optarg);
            }
            
            break;
         }
         
         case 'a': {
            if (NULL != optarg) {
               conf.http_addr = strdup(optarg);
            }
            break;
         }
         
         case 'b':
         case 'p':
         case 's':
         case 'l': {
            if (NULL != optarg) {
               char* eptr = NULL;
               int valid = 0;
               long nbuf = strtol(optarg, &eptr, 10);

               if ('\0' == *eptr) {
                  switch (c) {
                     case 'b':
                        if (nbuf >= 0) {
                           conf.gpio_idx = (int)nbuf;
                           valid = 1;
                        }
                        break;
                     case 'p':
                        if (nbuf >= 0) {
                           conf.program_idx = (int)nbuf;
                           valid = 1;
                        }
                        break;
                     case 's':
                        if (0 == nbuf || 1 == nbuf) { 
                           conf.http_on = (int)nbuf;
                           valid = 1;
                        }
                        break;
                     case 'l': {
                        if (nbuf > 0 && nbuf <= 65535) {
                           conf.http_port = (unsigned)nbuf;
                           valid = 1;
                        }
                        break;   
                     }
                     default:
                        break;
                  }
               }
               
               if (!valid) {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  ambitv_usage(argv[0]);
                  return -1;
               }
            }
            
            break;
         }
         
         case 'h': {
            if (from_file) {
               ambitv_log(ambitv_log_error,
                  LOGNAME "\"help\" argument can't be used in config file.\n");
               return -1;
            }
            
            ambitv_usage(argv[0]);
            exit(0);
         }
         
         default:
            break;
      }
   }

   if (optind < argc) {
      ambitv_log(ambitv_log_error, LOGNAME "extraneous configuration argument: '%s'.\n",
         argv[optind]);
      ambitv_usage(argv[0]);
      ret = -1;
   }

   return ret;
}

static void
ambitv_print_main_configuration()
{
   printf(
      "\tstarting program:       %d\n"
      "\tbutton gpio:            %d\n"
      "\thttp backend active:    %s\n"
      "\thttp backend address:   %s\n"
      "\thttp backend port:      %u\n",
         conf.program_idx,
         conf.gpio_idx,
         conf.http_on ? "yes" : "no",
         conf.http_addr ? conf.http_addr : "all addresses",
         conf.http_port
   );
}

int
main(int argc, char** argv)
{
   int ret = 0, i, is_tty = 0;
   struct ambitv_conf_parser* parser;
   struct termios tt;
   unsigned long tt_orig;
   
   signal(SIGINT, ambitv_signal_handler);
   signal(SIGTERM, ambitv_signal_handler);
   
   printf(
      "\n"
      "*********************************************************\n"
      "*  ambi-tv: diy ambient lighting for your screen or tv  *\n"
      "*                                         (c) @gkaindl  *\n"
      "*********************************************************\n"
      "\n"
   );
   
   conf.program_idx     = 0;
   conf.gpio_idx        = -1;
   conf.config_path     = DEFAULT_CONFIG_PATH;
   conf.ambitv_on       = 1;
   conf.gpio_fd         = -1;
   conf.http_on         = 0;
   conf.http_addr       = NULL;
   conf.http_port       = DEFAULT_HTTP_PORT;
   conf.running         = 1;
      
   ret = ambitv_main_configure(argc, argv, 0);
   if (ret < 0)
      return -1;
      
   parser = ambitv_conf_parser_create();
   parser->f_handle_block = ambitv_handle_config_block;
   ret = ambitv_conf_parser_read_config_file(parser, conf.config_path);
   ambitv_conf_parser_free(parser);
      
   if (ret < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to parse configuration file, aborting...\n");
      ambitv_usage(argv[0]);
      return -1;
   }
   
   // re-run configuration from command line, so that command line arguments
   // can override settings from the configuration file.
   (void)ambitv_main_configure(argc, argv, 0);
   
   if (ambitv_num_programs <= 0) {
      ambitv_log(ambitv_log_error, LOGNAME "no programs available, aborting...\n");
      return -1;
   }
   
   ambitv_log(ambitv_log_info, LOGNAME "configuration finished, %d programs available.\n",
      ambitv_num_programs);
   
   for (i=0; i<ambitv_num_programs; i++)
      ambitv_log(ambitv_log_info, "\t%s\n", ambitv_programs[i]->name);
   
   if (conf.gpio_idx >= 0) {
      conf.gpio_fd = ambitv_gpio_open_button_irq(conf.gpio_idx);
      if (conf.gpio_fd < 0) {
         ambitv_log(ambitv_log_error, LOGNAME "failed to prepare gpio %d, aborting...\n",
            conf.gpio_idx);
         return -1;
      } else {
         ambitv_log(ambitv_log_info, LOGNAME "using gpio %d as physical button.\n",
            conf.gpio_idx);
      }
   }
   
   if (conf.http_on) {
      http_backend = http_create(
         conf.http_addr,
         conf.http_port
      );
      
      if (!http_backend) {
         ambitv_log(ambitv_log_error, LOGNAME "failed to create http backend: %s\n", strerror(errno));
         return -1;
      } else {
         ambitv_log(ambitv_log_info, LOGNAME "http backend created at %s:%u\n",
            conf.http_addr ? conf.http_addr : "<any>", conf.http_port);
      }
   
      http_set_keypair_callback(http_backend, ambitv_handle_http_backend_keypair);
   }
   
   is_tty = isatty(STDIN_FILENO);
   
   if (is_tty) {
     tcgetattr(STDIN_FILENO, &tt);
     tt_orig = tt.c_lflag;
     tt.c_lflag &= ~(ICANON | ECHO);
     tcsetattr(STDIN_FILENO, TCSANOW, &tt);
   }
   
   if (conf.program_idx >= ambitv_num_programs) {
      ambitv_log(ambitv_log_error, LOGNAME "program at index %d requested, but only %d programs available. aborting...\n",
         conf.program_idx, ambitv_num_programs);
      goto errReturn;
   }
   
   conf.cur_prog = conf.program_idx;
   
   printf("ambitv configuration:\n");
   ambitv_print_main_configuration();
   
   ret = ambitv_program_run(ambitv_programs[conf.cur_prog]);
   
   if (ret < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to start initial program '%s', aborting...\n",
         ambitv_programs[conf.cur_prog]->name);
      goto errReturn;
   }
   
   ambitv_log(ambitv_log_info, LOGNAME "started initial program '%s'.\n",
      ambitv_programs[conf.cur_prog]->name);
   
   if (is_tty) { 
     fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
   }
   
   ambitv_log(ambitv_log_info,
      LOGNAME "************* start-up complete\n"
      "\tpress <space> to cycle between programs.\n"
      "\tpress 't' to toggle pause.\n");
   if (conf.gpio_idx >= 0) {
      ambitv_log(ambitv_log_info,
         "\tphysical (gpio) button: click to pause/resume, double-click to cycle between programs.\n");
   }
   
   while (conf.running && ambitv_runloop(is_tty) >= 0);
   
   ret = ambitv_program_stop_current();
   
   if (ret < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to stop program '%s' before exiting.\n",
         ambitv_programs[conf.cur_prog]->name);
      goto errReturn;
   }

errReturn:
   if (is_tty) {
     tt.c_lflag = tt_orig;
     tcsetattr(STDIN_FILENO, TCSANOW, &tt);
   }
   
   if (conf.gpio_fd >= 0)
      ambitv_gpio_close_button_irq(conf.gpio_fd, conf.gpio_idx);
   
   if (http_backend) {
      http_free(http_backend);
      http_backend = NULL;
   }
   
   return ret;
}

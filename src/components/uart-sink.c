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

/* uart-sink: uart sink for ambi-tv project
* 
*  Author: stozze
*  https://github.com/stozze/ambi-tv
*/

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "uart-sink.h"

#include "../util.h"
#include "../log.h"
#include "../color.h"

#define DEFAULT_DEV_NAME                "/dev/ttyUSB0"
#define DEFAULT_UART_BAUDRATE           B460800
#define DEFAULT_UART_BAUDRATE_STR       "460800"
#define DEFAULT_GAMMA                   1.6   // works well for me, but ymmv...

#define LOGNAME                         "uart: "

struct ambitv_uart_priv {
   char*             device_name;
   char*             baudrate_str;
   int               fd, baudrate, num_leds, actual_num_leds, grblen;
   struct termios    device_options;
   int               led_len[4], *led_str[4];   // top, bottom, left, right
   double            led_inset[4];              // top, bottom, left, right
   unsigned char*    grb;
   unsigned char**   bbuf;
   int               num_bbuf, bbuf_idx;
   double            gamma[3];      // RGB gamma, not GRB!
   unsigned char*    gamma_lut[3];  // also RGB
};

static int*
ambitv_uart_ptr_for_output(struct ambitv_uart_priv* uart, int output, int* led_str_idx, int* led_idx)
{
   int idx = 0, *ptr = NULL;
   
   if (output < uart->num_leds) {
      while(output >= uart->led_len[idx]) {
         output -= uart->led_len[idx];
         idx++;
      }
      
      if (uart->led_str[idx][output] >= 0) {
         ptr = &uart->led_str[idx][output];
      
         if (led_str_idx)
            *led_str_idx = idx;
         
         if (led_idx)
            *led_idx = output;
      }
   }

   return ptr;
}

static int
ambitv_uart_map_output_to_point(
   struct ambitv_sink_component* component,
   int output,
   int width,
   int height,
   int* x,
   int* y)
{
   int ret = -1, *outp = NULL, str_idx = 0, led_idx = 0;
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   outp = ambitv_uart_ptr_for_output(uart, output, &str_idx, &led_idx);
   
   if (NULL != outp) {
      ret = 0;
      float llen  = uart->led_len[str_idx] - 1;
      float dim   = (str_idx < 2) ? width : height;
      float inset = uart->led_inset[str_idx] * dim;
      dim -= 2*inset;
      
      switch (str_idx) {
         case 0:  // top
            *x = (int)CONSTRAIN(inset + (dim / llen) * led_idx, 0, width);
            *y = 0;
            break;
         case 1:  // bottom
            *x = (int)CONSTRAIN(inset + (dim / llen) * led_idx, 0, width);
            *y = height;
            break;
         case 2:  // left
            *x = 0;
            *y = (int)CONSTRAIN(inset + (dim / llen) * led_idx, 0, height);
            break;
         case 3:  // right
            *x = width;
            *y = (int)CONSTRAIN(inset + (dim / llen) * led_idx, 0, height);
            break;
         default:
            ret = -1;
            break;
      }
   } else {
      *x = *y = -1;
   }
   
   return ret;
}

static int
ambitv_uart_commit_outputs(struct ambitv_sink_component* component)
{
   int ret = -1;
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
      
   if (uart->fd >= 0) {
      ret = write(uart->fd, uart->grb, uart->grblen);
      
      if (ret != uart->grblen) {
         if (ret <= 0) 
            ret = -errno;
         else 
            ret = -ret;
      } else
         ret = 0;
   }
   
   if (uart->num_bbuf)
      uart->bbuf_idx = (uart->bbuf_idx + 1) % uart->num_bbuf;
   
   return ret;
}

static void
ambitv_uart_clear_leds(struct ambitv_sink_component* component)
{
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   if (NULL != uart->grb && uart->fd >= 0) {
      int i;
      
      // send 3 times, in case there's noise on the line,
      // so that all LEDs will definitely be off afterwards.
      for (i=0; i<3; i++) {
         memset(uart->grb, 0x00, uart->grblen);
         (void)ambitv_uart_commit_outputs(component);
      }
   }
}

static int
ambitv_uart_set_output_to_rgb(
   struct ambitv_sink_component* component,
   int idx,
   int r,
   int g,
   int b)
{
   int ret = -1, *outp = NULL, i, *rgb[] = {&r, &g, &b};
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   outp = ambitv_uart_ptr_for_output(uart, idx, NULL, NULL);
   
   if (NULL != outp) {
      int ii = *outp;
      
      if (uart->num_bbuf) {
         unsigned char* acc = uart->bbuf[uart->bbuf_idx];
         
         acc[3 * ii]             = g;
         acc[3 * ii + 1]         = r;
         acc[3 * ii + 2]         = b;
         
         r = g = b = 0;
         for (i=0; i<uart->num_bbuf; i++) {
            g += uart->bbuf[i][3 * ii];
            r += uart->bbuf[i][3 * ii + 1];
            b += uart->bbuf[i][3 * ii + 2];
         }
         
         g /= uart->num_bbuf;
         r /= uart->num_bbuf;
         b /= uart->num_bbuf;
      }
      
      for (i=0; i<3; i++) {
         if (uart->gamma_lut[i])
            *rgb[i] = ambitv_color_map_with_lut(uart->gamma_lut[i], *rgb[i]);
      }
      
      uart->grb[3 * ii]       = g;
      uart->grb[3 * ii + 1]   = r;
      uart->grb[3 * ii + 2]   = b;
      
      ret = 0;
   }
   
   return ret;
}

static int
ambitv_uart_num_outputs(struct ambitv_sink_component* component)
{
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   return uart->num_leds;
}

static int
ambitv_uart_start(struct ambitv_sink_component* component)
{
   int ret = 0;
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   if (uart->fd < 0) {
      uart->fd = open(uart->device_name, O_WRONLY | O_NOCTTY | O_NDELAY);
      if (uart->fd < 0) {
         ret = uart->fd;
         ambitv_log(ambitv_log_error, LOGNAME "failed to open device '%s' : %d (%s).\n",
            uart->device_name, errno, strerror(errno));
         goto errReturn; 
      }
      
      tcgetattr(uart->fd, &uart->device_options);
      uart->device_options.c_cflag = uart->baudrate | CS8 | CLOCAL;
      uart->device_options.c_iflag = IGNPAR;
      uart->device_options.c_oflag = 0;
      uart->device_options.c_lflag = 0;
      
      ret = tcflush(uart->fd, TCIFLUSH);
      if (ret < 0) {
         ambitv_log(ambitv_log_error, LOGNAME "failed to flush on device '%s' : %d (%s).\n",
            uart->device_name, errno, strerror(errno));
         goto closeReturn;
      }
      
      ret = tcsetattr(uart->fd, TCSANOW, &uart->device_options);
      if (ret < 0) {
         ambitv_log(ambitv_log_error, LOGNAME "failed to set attributes on device '%s' : %d (%s).\n",
            uart->device_name, errno, strerror(errno));
         goto closeReturn;
      }
   }

   ambitv_uart_clear_leds(component);
   
   return ret;

closeReturn:
   close(uart->fd);
   uart->fd = -1;
errReturn:
   return ret;
}

static int
ambitv_uart_stop(struct ambitv_sink_component* component)
{
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   ambitv_uart_clear_leds(component);
   
   if (uart->fd >= 0) {
      close(uart->fd);
      uart->fd = -1;
   }
   
   return 0;
}

static int
ambitv_uart_configure(struct ambitv_sink_component* component, int argc, char** argv)
{
   int i, c, ret = 0;
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   memset(uart->led_str, 0, sizeof(int*) * 4);
   uart->num_leds = uart->actual_num_leds = 0;

      
   if (NULL == uart)
      return -1;

   static struct option lopts[] = {
      { "uart-device", required_argument, 0, 'd' },
      { "uart-baudrate", required_argument, 0, 'r' },
      { "leds-top", required_argument, 0, '0' },
      { "leds-bottom", required_argument, 0, '1' },
      { "leds-left", required_argument, 0, '2' },
      { "leds-right", required_argument, 0, '3' },
      { "blended-frames", required_argument, 0, 'b' },
      { "gamma-red", required_argument, 0, '4'},
      { "gamma-green", required_argument, 0, '5'},
      { "gamma-blue", required_argument, 0, '6'},
      { "led-inset-top", required_argument, 0, 'w'},
      { "led-inset-bottom", required_argument, 0, 'x'},
      { "led-inset-left", required_argument, 0, 'y'},
      { "led-inset-right", required_argument, 0, 'z'},
      { NULL, 0, 0, 0 }
   };
   
   while (1) {    
      c = getopt_long(argc, argv, "", lopts, NULL);

      if (c < 0)
         break;

      switch (c) {
         case 'd': {            
            if (NULL != optarg) {
               if (NULL != uart->device_name)
                  free(uart->device_name);

               uart->device_name = strdup(optarg);
            }
            break;
         }

         case 'r': {
            if (NULL != optarg) {
               int new_baudrate = -1;
               
               if (strcmp("115200", optarg) == 0) {
                  new_baudrate = B115200;
               }
               else if (strcmp("230400", optarg) == 0) {
                  new_baudrate = B230400;
               }
               else if (strcmp("460800", optarg) == 0) {
                  new_baudrate = B460800;
               }
               else if (strcmp("500000", optarg) == 0) {
                  new_baudrate = B500000;
               }
               else if (strcmp("576000", optarg) == 0) {
                  new_baudrate = B576000;
               }
               else if (strcmp("921600", optarg) == 0) {
                  new_baudrate = B921600;
               }
               else if (strcmp("1000000", optarg) == 0) {
                  new_baudrate = B1000000;
               }
               else if (strcmp("1152000", optarg) == 0) {
                  new_baudrate = B1152000;
               }
               else {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  ret = -1;
                  goto errReturn;
               }
               
               if (NULL != uart->baudrate_str)
                  free(uart->baudrate_str);

               uart->baudrate_str = strdup(optarg);
               uart->baudrate = new_baudrate;
            }

            break;
         }
         
         case 'b': {
            if (NULL != optarg) {
               char* eptr = NULL;
               long nbuf = strtol(optarg, &eptr, 10);
         
               if ('\0' == *eptr && nbuf >= 0) {
                  uart->num_bbuf = (int)nbuf;
               } else {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  ret = -1;
                  goto errReturn;
               }
            }
         
            break;
         }
         
         case '0':
         case '1':
         case '2':
         case '3': {
            int idx = c - '0';
                        
            ret = ambitv_parse_led_string(optarg, &uart->led_str[idx], &uart->led_len[idx]);
            if (ret < 0) {
               ambitv_log(ambitv_log_error, LOGNAME "invalid led configuration string for '%s': '%s'.\n",
                  argv[optind-2], optarg);
               goto errReturn;
            }
            
            uart->num_leds += uart->led_len[idx];
            for (i=0; i<uart->led_len[idx]; i++)
               if (uart->led_str[idx][i] >= 0)
                  uart->actual_num_leds++;
                
            break;
         }
         
         case '4':
         case '5':
         case '6': {
            if (NULL != optarg) {
               char* eptr = NULL;
               double nbuf = strtod(optarg, &eptr);
         
               if ('\0' == *eptr) {
                  uart->gamma[c-'4'] = nbuf;
               } else {
                  ambitv_log(ambitv_log_error, LOGNAME "invalid argument for '%s': '%s'.\n",
                     argv[optind-2], optarg);
                  ret = -1;
                  goto errReturn;
               }
            }
         
            break;
         }
         
         case 'w':
         case 'x':
         case 'y':
         case 'z': {
            if (NULL != optarg) {
               char* eptr = NULL;
               double nbuf = strtod(optarg, &eptr);
         
               if ('\0' == *eptr) {
                  uart->led_inset[c-'w'] = nbuf / 100.0;
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
      ambitv_log(ambitv_log_error, LOGNAME "extraneous argument: '%s'.\n",
         argv[optind]);
      ret = -1;
   }

errReturn:
   return ret;
}

static void
ambitv_uart_print_configuration(struct ambitv_sink_component* component)
{
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;

   ambitv_log(ambitv_log_info,
      "\tdevice name:       %s\n"
      "\tbaudrate:          %s\n"
      "\tnumber of leds:    %d\n"
      "\tblending frames:   %d\n"
      "\tled insets (tblr): %.1f%%, %.1f%%, %.1f%%, %.1f%%\n"
      "\tgamma (rgb):       %.2f, %.2f, %.2f\n",
         uart->device_name,
         uart->baudrate_str,
         uart->actual_num_leds,
         uart->num_bbuf,
         uart->led_inset[0]*100.0, uart->led_inset[1]*100.0,
         uart->led_inset[2]*100.0, uart->led_inset[3]*100.0,
         uart->gamma[0], uart->gamma[1], uart->gamma[2]
   );
}

void
ambitv_uart_free(struct ambitv_sink_component* component)
{
   int i;
   struct ambitv_uart_priv* uart =
      (struct ambitv_uart_priv*)component->priv;
   
   if (NULL != uart) {
      if (NULL != uart->device_name)
         free(uart->device_name);
      
      if (NULL != uart->baudrate_str)
         free(uart->baudrate_str);
      
      if (NULL != uart->grb)
         free(uart->grb);
      
      if (NULL != uart->bbuf) {
         for (i=0; i<uart->num_bbuf; i++)
            free(uart->bbuf[i]);
         free(uart->bbuf);
      }
      
      for (i=0; i<3; i++) {
         if (NULL != uart->gamma_lut[i])
            ambitv_color_gamma_lookup_table_free(uart->gamma_lut[i]);
      }
      
      free(uart);
   }
}

struct ambitv_sink_component*
ambitv_uart_create(const char* name, int argc, char** argv)
{  
   struct ambitv_sink_component* uart =
      ambitv_sink_component_create(name);
   
   if (NULL != uart) {
      int i;
      struct ambitv_uart_priv* priv =
         (struct ambitv_uart_priv*)malloc(sizeof(struct ambitv_uart_priv));
      
      uart->priv = priv;
      
      memset(priv, 0, sizeof(struct ambitv_uart_priv));
      
      priv->fd             = -1;
      priv->baudrate       = DEFAULT_UART_BAUDRATE;
      priv->baudrate_str   = strdup(DEFAULT_UART_BAUDRATE_STR);
      priv->device_name    = strdup(DEFAULT_DEV_NAME);
      priv->gamma[0]       = DEFAULT_GAMMA;
      priv->gamma[1]       = DEFAULT_GAMMA;
      priv->gamma[2]       = DEFAULT_GAMMA;
      
      bzero(&priv->device_options, sizeof(struct termios));
      
      uart->f_print_configuration   = ambitv_uart_print_configuration;
      uart->f_start_sink            = ambitv_uart_start;
      uart->f_stop_sink             = ambitv_uart_stop;
      uart->f_num_outputs           = ambitv_uart_num_outputs;
      uart->f_set_output_to_rgb     = ambitv_uart_set_output_to_rgb;
      uart->f_map_output_to_point   = ambitv_uart_map_output_to_point;
      uart->f_commit_outputs        = ambitv_uart_commit_outputs;
      uart->f_free_priv             = ambitv_uart_free;
      
      if (ambitv_uart_configure(uart, argc, argv) < 0)
         goto errReturn;
      
      priv->grblen   = sizeof(unsigned char) * 3 * priv->actual_num_leds;
      priv->grb      = (unsigned char*)malloc(priv->grblen);
      
      if (priv->num_bbuf > 1) {
         priv->bbuf = (unsigned char**)malloc(sizeof(unsigned char*) * priv->num_bbuf);
         for (i=0; i<priv->num_bbuf; i++) {
            priv->bbuf[i] = (unsigned char*)malloc(priv->grblen);
            memset(priv->bbuf[i], 0, priv->grblen);
         }
      } else
         priv->num_bbuf = 0;

      memset(priv->grb, 0x00, priv->grblen);
      
      for (i=0; i<3; i++) {
         if (priv->gamma[i] >= 0.0) {
            priv->gamma_lut[i] =
               ambitv_color_gamma_lookup_table_create(priv->gamma[i]);
         }
      }
   }
   
   return uart;

errReturn:
   ambitv_sink_component_free(uart);
   return NULL;
}

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
#include <stdarg.h>

#include "log.h"

enum ambitv_log_mode ambitv_log_mode      = ambitv_log_mode_console;

static void
ambitv_log_console(enum ambitv_log_priority priority, const char* fmt, va_list args)
{
//   if (priority >= ambitv_log_error)
//      vfprintf(stderr, fmt, args);
//   else
      vprintf(fmt, args);
}

void
ambitv_log(enum ambitv_log_priority priority, const char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   
   switch (ambitv_log_mode) {
      case ambitv_log_mode_console:
      default: {
         ambitv_log_console(priority, fmt, args);
         break;
      }
   }
   
   va_end(args);
}

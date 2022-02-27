/* word-clock: a flexible ambilight clone for embedded linux
*  Copyright (C) 2013 Georg Kaindl
*  
*  This file is part of word-clock.
*  
*  word-clock is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 2 of the License, or
*  (at your option) any later version.
*  
*  word-clock is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with word-clock.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdarg.h>

#include "log.h"

enum wordclock_log_mode wordclock_log_mode      = wordclock_log_mode_console;

static void
wordclock_log_console(enum wordclock_log_priority priority, const char* fmt, va_list args)
{
//   if (priority >= wordclock_log_error)
//      vfprintf(stderr, fmt, args);
//   else
      vprintf(fmt, args);
}

void
wordclock_log(enum wordclock_log_priority priority, const char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   
   switch (wordclock_log_mode) {
      case wordclock_log_mode_console:
      default: {
         wordclock_log_console(priority, fmt, args);
         break;
      }
   }
   
   va_end(args);
}

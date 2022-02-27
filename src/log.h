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

#ifndef __WORDCLOCK_LOG_H__
#define __WORDCLOCK_LOG_H__

enum wordclock_log_mode {
   wordclock_log_mode_console
};

enum wordclock_log_priority {
   wordclock_log_info    = 1,
   wordclock_log_warn,
   wordclock_log_error
};

extern enum wordclock_log_mode wordclock_log_mode;

void
wordclock_log(enum wordclock_log_priority priority, const char* fmt, ...);

#endif // __WORDCLOCK_LOG_H__

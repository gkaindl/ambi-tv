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

#ifndef __AMBITV_LOG_H__
#define __AMBITV_LOG_H__

enum ambitv_log_mode {
   ambitv_log_mode_console
};

enum ambitv_log_priority {
   ambitv_log_info    = 1,
   ambitv_log_warn,
   ambitv_log_error
};

extern enum ambitv_log_mode ambitv_log_mode;

void
ambitv_log(enum ambitv_log_priority priority, const char* fmt, ...);

#endif // __AMBITV_LOG_H__

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

#ifndef __AMBITV_HTTP_H__
#define __AMBITV_HTTP_H__

#include <sys/select.h>

typedef struct http http;

http*
http_create(const char* ip_addr, unsigned port);

void
http_free(http* server);

void
http_set_keypair_callback(
   http* server, int (*callback) (http*, const char*, const char* )
);

int
http_fill_fd_sets(
   http* server, fd_set* read_fds, fd_set* write_fds, int cur_max_fd
);

void
http_handle_fd_sets(http* server, fd_set* read_fds, fd_set* write_fds);

int
http_reply_keypair(http* server, const char* name, const char* value);

int
http_reply_keypair_int(http* server, const char* name, int value);

#endif // __AMBITV_HTTP_H__
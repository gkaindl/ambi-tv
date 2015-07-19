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

#ifndef __AMBITV_UTIL_H__
#define __AMBITV_UTIL_H__

#define MIN(x,y)              ((x) > (y)) ? (y) : (x)
#define MAX(x,y)              ((x) > (y)) ? (x) : (y)
#define CONSTRAIN(a, l, r)    (MIN(MAX((l), (a)), (r)))

#define NETIF_MODE_FIRST 	0x01
#define NETIF_MODE_MID		0x02
#define NETIF_MODE_LAST		0x04
#define NETIF_MODE_SINGLE	(NETIF_MODE_FIRST | NETIF_MODE_MID | NETIF_MODE_LAST)

typedef enum { false, true } bool;

int ambitv_util_append_ptr_to_list(void*** list_ptr, int idx, int* len_ptr, void* ptr);

int ambitv_parse_led_string(const char* str, int** out_ptr, int* out_len);

char *stristr(const char *String, const char *Pattern);

void netif_send(int socket, char *data, int length, int mode, bool bb);

#endif // __AMBITV_UTIL_H__

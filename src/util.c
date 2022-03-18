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

#include <stdlib.h>
#include <stdio.h>
#include <libio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include "log.h"

#define LOGNAME            "util: "

#define LIST_GROW_STEP     4

static const char html_header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n";

int wordclock_util_append_ptr_to_list(void*** list_ptr, int idx, int* len_ptr, void* ptr)
{
	if (NULL == *list_ptr)
	{
		*list_ptr = (void**) malloc(sizeof(void*) * LIST_GROW_STEP);
		*len_ptr = LIST_GROW_STEP;
	}
	else if (idx >= *len_ptr)
	{
		*len_ptr += LIST_GROW_STEP;
		*list_ptr = (void**) realloc(*list_ptr, sizeof(void*) * (*len_ptr));
	}

	(*list_ptr)[idx] = ptr;

	return ++idx;
}

char *stristr(const char *String, const char *Pattern)
{
	char *pptr, *sptr, *start;

	for (start = (char *) String; *start; start++)
	{
		/* find start of pattern in string */
		for (; (*start && (toupper(*start) != toupper(*Pattern))); start++)
			;
		if (!*start)
			return 0;

		pptr = (char *) Pattern;
		sptr = (char *) start;

		while (toupper(*sptr) == toupper(*pptr))
		{
			sptr++;
			pptr++;

			/* if end of pattern then pattern was found */

			if (!*pptr)
				return (start);
		}
	}
	return 0;
}

void netif_send(int socket, char *data, int length, int mode, bool bb)
{
	if (socket >= 0)
	{
		if (mode & NETIF_MODE_FIRST)
		{
			char tbuf[128];

			write(socket, html_header, sizeof(html_header));
			sprintf(tbuf, "Content-Length: %ld\r\n%s", (unsigned long) ((length) ? length : strlen(data)),
					(bb) ? "" : "\r\n");
			write(socket, tbuf, strlen(tbuf));
		}
		if (mode & NETIF_MODE_MID)
		{
			write(socket, data, (length) ? length : strlen(data));
		}
	}
}


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

#ifndef __WORDCLOCK_PROGRAM_H__
#define __WORDCLOCK_PROGRAM_H__

struct wordclock_program {
   char* name;
   
   void** components;
   int num_components, len_components;
};

extern struct wordclock_program** wordclock_programs;
extern int wordclock_num_programs;


int
wordclock_program_enable(struct wordclock_program* program);

struct wordclock_program*
wordclock_program_create(const char* name, int argc, char** argv);

void
wordclock_program_free(struct wordclock_program* program);

int
wordclock_program_run(struct wordclock_program* program);

int
wordclock_program_stop_current();

#endif // __WORDCLOCK_PROGRAM_H__

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

#ifndef __AMBITV_PROGRAM_H__
#define __AMBITV_PROGRAM_H__

struct ambitv_program {
   char* name;
   
   void** components;
   int num_components, len_components;
};

extern struct ambitv_program** ambitv_programs;
extern int ambitv_num_programs;


int
ambitv_program_enable(struct ambitv_program* program);

struct ambitv_program*
ambitv_program_create(const char* name, int argc, char** argv);

void
ambitv_program_free(struct ambitv_program* program);

int
ambitv_program_run(struct ambitv_program* program);

int
ambitv_program_stop_current();

#endif // __AMBITV_PROGRAM_H__

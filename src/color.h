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

#ifndef __AMBITV_COLOR_H__
#define __AMBITV_COLOR_H__

unsigned char*
ambitv_color_gamma_lookup_table_create(double gamma_value);

void
ambitv_color_gamma_lookup_table_free(unsigned char* lut);

unsigned char
ambitv_color_map_with_lut(unsigned char* lut, unsigned char color_component);

void
ambitv_hsl_to_rgb(int hue, int sat, int lum, int* r, int* g, int* b);

#endif // __AMBITV_COLOR_H__

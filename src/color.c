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

#include <stdlib.h>
#include <math.h> 

#include "color.h"
#include "log.h"

#define LOGNAME   "color: "

unsigned char*
ambitv_color_gamma_lookup_table_create(double gamma_value)
{
   int i;
   unsigned char* lut = (unsigned char*)malloc(256 * sizeof(unsigned char));
   
   if (NULL == lut) {
      ambitv_log(ambitv_log_error, LOGNAME "out of memory!");
      return NULL;
   }
   
   for (i=0; i<256; i++)
      lut[i] = pow((double)i / 255.0, gamma_value) * 255.0;
   
   return lut;
}

void
ambitv_color_gamma_lookup_table_free(unsigned char* lut)
{
   if (NULL != lut)
      free(lut);
}

inline unsigned char
ambitv_color_map_with_lut(unsigned char* lut, unsigned char color_component)
{
   unsigned char mapped_component = color_component;
   
   if (NULL != lut)
      mapped_component = lut[color_component];
   
   return mapped_component;
}

void
ambitv_hsl_to_rgb(int hue, int sat, int lum, int* r, int* g, int* b)
{
    int v;

    v = (lum < 128) ? (lum * (256 + sat)) >> 8 :
          (((lum + sat) << 8) - lum * sat) >> 8;
    if (v <= 0) {
        *r = *g = *b = 0;
    } else {
        int m;
        int sextant;
        int fract, vsf, mid1, mid2;

        m = lum + lum - v;
        hue *= 6;
        sextant = hue >> 8;
        fract = hue - (sextant << 8);
        vsf = v * fract * (v - m) / v >> 8;
        mid1 = m + vsf;
        mid2 = v - vsf;
        switch (sextant) {
           case 0: *r = v; *g = mid1; *b = m; break;
           case 1: *r = mid2; *g = v; *b = m; break;
           case 2: *r = m; *g = v; *b = mid1; break;
           case 3: *r = m; *g = mid2; *b = v; break;
           case 4: *r = mid1; *g = m; *b = v; break;
           case 5: *r = v; *g = m; *b = mid2; break;
        }
    }
}
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

#ifndef __AMBITV_VIDEO_FMT_H__
#define __AMBITV_VIDEO_FMT_H__

#include <stdint.h>

enum ambitv_video_format {
   ambitv_video_format_yuyv,
   ambitv_video_format_unknown 
};

const char*
v4l2_string_from_fourcc(uint32_t fourcc);

enum ambitv_video_format
v4l2_to_ambitv_video_format(uint32_t fourcc);

int
ambitv_video_fmt_avg_rgb_for_block(unsigned char* rgb, const void* pixbuf, int x, int y, int w, int h, int bytesperline,
   enum ambitv_video_format fmt, int coarseness);

int
ambitv_video_fmt_detect_crop_for_frame(
   int crop[4], int luminance_threshold, const void* pixbuf, int w, int h, int bytesperline, enum ambitv_video_format fmt);

#endif // __AMBITV_VIDEO_FMT_H__

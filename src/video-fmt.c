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

#include <stdio.h>
#include <sys/types.h>
#include <linux/videodev2.h>

#include "video-fmt.h"
#include "video-fmt.r"
#include "log.h"

#define LOGNAME      "video-fmt: "

const char* v4l2_string_from_fourcc(uint32_t fourcc)
{
   static char s[5];
   s[0] = 0xff & (fourcc);
   s[1] = 0xff & (fourcc >> 8);
   s[2] = 0xff & (fourcc >> 16);
   s[3] = 0xff & (fourcc >> 24);
   s[4] = 0;
   return s;
}

enum ambitv_video_format
v4l2_to_ambitv_video_format(uint32_t fourcc)
{
   enum ambitv_video_format fmt = ambitv_video_format_unknown;
   
   switch (fourcc) {
      case V4L2_PIX_FMT_YUYV:
         fmt = ambitv_video_format_yuyv;
         break;
      default:
         break;
   }
   
   return fmt;
}

void yuv_to_rgb(int y, int u, int v, unsigned char* r, unsigned char* g, unsigned char* b)
{
   int c, d, e, i;
   int rgb[3];

   c = y - 16;
   d = u - 128;
   e = v - 128;

   rgb[0] = (298 * c           + 409 * e + 128) >> 8;
   rgb[1] = (298 * c - 100 * d - 208 * e + 128) >> 8;
   rgb[2] = (298 * c + 516 * d           + 128) >> 8;

   for (i=0; i<3; i++) {
      if (rgb[i] > 255) rgb[i] = 255;
      if (rgb[i] < 0) rgb[i] = 0; 
   }

   *r = (unsigned char)rgb[0];
   *g = (unsigned char)rgb[1];
   *b = (unsigned char)rgb[2];
}


#define YUYV_BYTES_PER_PIXEL 2
#define YUYV_PIXEL_PER_TUPLE 2

/*
            point_to_box(&x, &y, DEFAULT_BOX_WIDTH, DEFAULT_BOX_HEIGHT, edge->width, edge->height);
            ambitv_video_fmt_avg_rgb_for_block(rgb, edge->frame, x, y, DEFAULT_BOX_WIDTH, DEFAULT_BOX_HEIGHT, edge->bytesperline, edge->fmt, 4);
*/
int avg_rgb_for_block_yuyv(unsigned char* rgb, const void* pixbuf, int x, int y, int w, int h, int bytesperline, int coarseness)
{
   int i, j, k, v, y1, u, y2, cnt = 0;

   long avg_rgb[3] = {0,0,0};
   unsigned char irgb[6];

   if (0 == bytesperline)
      bytesperline = 2*w;

   x = (x >> 2) << 2;

   for (i = x; i < x + w; i += YUYV_PIXEL_PER_TUPLE * coarseness) {
      for (j=y; j<y+h; j++) {
         unsigned char* yuyv = &(((unsigned char*)pixbuf)[YUYV_BYTES_PER_PIXEL*i + j*bytesperline]);

         y1 = yuyv[0];
         u  = yuyv[1];
         y2 = yuyv[2];
         v  = yuyv[3];

         yuv_to_rgb(y1, u, v, &irgb[0], &irgb[1], &irgb[2]);
         yuv_to_rgb(y2, u, v, &irgb[3], &irgb[4], &irgb[5]);

         for (k=0; k<6; k++)
            avg_rgb[k%3] += irgb[k];

         cnt += 2;
      }
   }
   
   rgb[0] = avg_rgb[0] / cnt;
   rgb[1] = avg_rgb[1] / cnt;
   rgb[2] = avg_rgb[2] / cnt;

   return 0;
}

static int
ambitv_video_fmt_detect_crop_for_frame_yuyv(int crop[4], int luminance_threshold, const void* pixbuf, int w, int h, int bytesperline)
{
   int ret = -1;
   
   if (NULL != pixbuf && NULL != crop) {
      /*
       * we take 32 samples along the axis each, scanning from the middle.
       * this way, we should maximize the chance to crop correctly, unless
       * the image is extremely dark, so that none of our samples would fall
       * into a bright area.
       *
       * we limit the scanning depth to 1/5 of the height (should capture most
       * sensible letterboxing) and 1/16 along the width, since this is usually
       * not letterboxed, so the crop will only be necessary to get rid of the
       * inset caused by crappy video capture devices.
       */
      
      int i, j, sx = ((w >> 6) << 2), sy = (h >> 5);
      int ss[32];
      
      luminance_threshold += 16;
      bytesperline = bytesperline ? bytesperline : 2*w;
      
      for (i=0; i<32; i++)
         ss[i] = (sy * ((i+16) % 32));
            
      // left
      for (i=0; i<w/8; i+=4) {
         for (j=0; j<32; j++) {
            unsigned char* pix = &(((unsigned char*)pixbuf)[i + ss[j]*bytesperline]);      
            if (luminance_threshold < pix[0] && luminance_threshold < pix[2]) {
               crop[3] = i>>1;
               goto left_done;
            }
         }
      }
      
   left_done:
      
      // right
      for (i=w*2; i>2*w-w/8; i-=4) {
         for (j=0; j<32; j++) {
            unsigned char* pix = &(((unsigned char*)pixbuf)[i + ss[j]*bytesperline]);
            if (luminance_threshold < pix[0] && luminance_threshold < pix[2]) {
               crop[1] = (w*2-i)>>1;
               goto right_done;
            }
         }
      }
      
   right_done:
      
      for (i=0; i<32; i++)
         ss[i] = (sx * ((i+16) % 32));
      
      // top
     for (i=0; i<h/5; i++) {
         for (j=0; j<32; j++) {
            unsigned char* pix = &(((unsigned char*)pixbuf)[ss[j] + i*bytesperline]);
            if (luminance_threshold < pix[0] && luminance_threshold < pix[2]) {
               crop[0] = i;
               goto top_done;
            }
         }
      }
      
   top_done:
      
      // bottom
      for (i=h; i>h-h/5; i--) {
         for (j=0; j<32; j++) {
            unsigned char* pix = &(((unsigned char*)pixbuf)[ss[j] + i*bytesperline]);
            if (luminance_threshold < pix[0] && luminance_threshold < pix[2]) {
               crop[2] = h-i;
               goto bottom_done;
            }
         }
      }
      
   bottom_done:
      
      ret = 0;
   }
   
   return ret;
}

int
ambitv_video_fmt_avg_rgb_for_block(unsigned char* rgb, const void* pixbuf, int x, int y, int w, int h, int bytesperline,
   enum ambitv_video_format fmt, int coarseness)
{
   int ret = -1;

   switch (fmt) {
      case ambitv_video_format_yuyv:
         ret = avg_rgb_for_block_yuyv(rgb, pixbuf, x, y, w, h, bytesperline, coarseness);
         break;

      default:
         ambitv_log(ambitv_log_warn, LOGNAME "unsupported video format in %s.\n",
            __func__);
         break;
   }

   return ret;
}

int
ambitv_video_fmt_detect_crop_for_frame(int crop[4], int luminance_threshold, const void* pixbuf, int w, int h, int bytesperline, enum ambitv_video_format fmt)
{
   int ret = -1;
   
   switch (fmt) {
      case ambitv_video_format_yuyv:
         ret = ambitv_video_fmt_detect_crop_for_frame_yuyv(crop, luminance_threshold, pixbuf, w, h, bytesperline);
         break;
   
      default:
         ambitv_log(ambitv_log_warn, LOGNAME "unsupported video format in %s.\n",
            __func__);
         break;
   }
   
   return ret;
}

/* ambi-tv: a flexible ambilight clone for embedded linux
*  Copyright (C) 2024 Georg Kaindl
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

/**
 * Adaptively stretches contrast and increases color saturation, mostly to
 * deal with HDR/Dolby Vision content that is captured at standard dynamic
 * range levels.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <math.h>

#include "yuv-enhance-coloreffect.h"

#include "../video-fmt.h"
#include "../log.h"
#include "../util.h"

#define LOGNAME               			"yuv-enhance: "

#define DEFAULT_Y_SCAN_STRIDE				(8)
#define DEFAULT_Y_HISTOGRAM_INSET		(.0005f)
#define DEFAULT_Y_MIN_DYN_RANGE			(35)
#define DEFAULT_Y_FLAT_SCALE				(.85f)
#define DEFAULT_UV_BOOST						(.6f)

#define FIXPT												(12)

struct ambitv_yuv_enhance_coloreffect_priv {
	int y_scan_stride;
	float y_histogram_inset_percent;
	int y_min_dynamic_range;
	float y_flat_scale;
	float uv_boost;
	
	int init_done;
	
	uint32_t lut_y_stretch[256];
	uint32_t lut_y_scale[256];
	uint32_t lut_uv_scale[256];
	uint32_t lut_uv_exp[129];
	
	uint8_t y_min, y_max;
	uint8_t uv_max;
	int use_flat_scale, stretch_idx;
};

static void
ambitv_yuv_enhance_init_luts(struct ambitv_yuv_enhance_coloreffect_priv* priv)
{
	int i;
	
	// LUT for the (y_max - y_min) standardization during contrast stretching, so
	// that we can avoid float conversions and division during frame processing.
	priv->lut_y_stretch[0] = (255 << FIXPT);
	for (i=1; i<256; i++) {
		priv->lut_y_stretch[i] = (uint32_t)round((float)(255 << FIXPT) / (float)i);
	}
	
	// LUT for a flat scale of luminance. The flat scaling is used if the image
	// has too low of a dynamic range to contrast-stretch it meaningfully.
	for (i=0; i<256; i++) {
		priv->lut_y_scale[i] = (uint32_t)round(
			((float)(1 << FIXPT)) * MIN(
				2.0f,
				1.0f + priv->y_flat_scale * ((float)i) / 255.0f
			)
		);
	}
	
	// LUT for a scaling factor applied to UV values during saturation
	// stretching. This includes some empirically determined "magic numbers", but
	// the basic idea is to stretch U/V levels more, the smaller the maximum U/V
	// component of the current frame is, e.g. the less saturated the entire
	// frame seems to be, the more we want to stretch U/V levels.
	const float max_f = 28.0f;
	const float scale = 1.5f;
	for (i=0; i<=128; i++) {
		float f = MIN(max_f, scale * (128.0f / (float)(i ? i : 1)));
		priv->lut_uv_exp[i] =
			(uint32_t)round((float)(1 << FIXPT) * pow(f, priv->uv_boost));
	}
	
	// LUT for the actual gamma transform applied to U/V levels – it's actually
	// inverted, where we apply "more" saturation the less saturated a color is.
	for (i=0; i<255; i++) {
		float val = ((float)(i - 128) / 128.0f);
		priv->lut_uv_scale[i] =
			(uint32_t)round(
				priv->uv_boost * pow(1.f-ABS(val), priv->uv_boost) *
				(float)(1 << FIXPT)
			);	
	}
	priv->lut_uv_scale[255] = (1 << FIXPT);	
}

static void
ambitv_yuv_enhance_count_one_yuv(
	struct ambitv_yuv_enhance_coloreffect_priv* priv,
	int y_vals[256], uint8_t y, uint8_t u, uint8_t v
)
{
	// We try to determine "effective" U/V levels of the current pixel in terms
	// of saturation, e.g. we assume that U/V would appear most saturated at
	// Y = 128, e.g. in the middle of the luma range. We use this to determine
	// the maximum saturation as a parameter for adaptive saturation stretching.
	int yd = ABS(128 - (int)y);
	uint8_t du = MAX(0, MIN(128, ABS((int)u - 128) - yd));
	uint8_t dv = MAX(0, MIN(128, ABS((int)v - 128) - yd));

	priv->uv_max = MAX(priv->uv_max, du);
	priv->uv_max = MAX(priv->uv_max, dv);
	
	// We also collect Y values in order to determine our max/min levels for
	// contrast stretching.
	y_vals[y] += 1;
}

static int
ambitv_yuv_enhance_prepare_for_frame(
   struct ambitv_coloreffect_component* component,
   void* frame,
   int width,
   int height,
   int bytesperline,
   enum ambitv_video_format fmt
) {
	int i, j, t, total;
	int y_vals[256];
	struct ambitv_yuv_enhance_coloreffect_priv* priv =
		(struct ambitv_yuv_enhance_coloreffect_priv*)component->priv;

	if (NULL != frame && ambitv_video_format_yuyv == fmt) {
		if (!priv->init_done) {
			ambitv_yuv_enhance_init_luts(priv);
			priv->init_done = 1;
		}
		
		memset(y_vals, 0, sizeof(y_vals));
		priv->uv_max = 0;
		total = 0;
		
		for (j = 0; j < height; j += priv->y_scan_stride) {
			for (i = 0; i < width; i += priv->y_scan_stride) {
				unsigned char* yuyv = &(((unsigned char*)frame)[2*i + j*bytesperline]);				
				
				ambitv_yuv_enhance_count_one_yuv(priv, y_vals, yuyv[0], yuyv[1], yuyv[3]);
				ambitv_yuv_enhance_count_one_yuv(priv, y_vals, yuyv[2], yuyv[1], yuyv[3]);
				
				total += 2;
			}
		}
		
		int inset = (int)((priv->y_histogram_inset_percent) * total);
		
		t = inset;
		for (i = 0; i<256; i++) {
			t -= y_vals[i];
			if (t < 0) {
				priv->y_min = i;
				break;
			}
		}
		
		t = inset;
		for (i = 255; i>=0; i--) {
			t -= y_vals[i];
			if (t < 0) {
				priv->y_max = i;
				break;
			}
		}
		
		// We use our "flat scaling" approach if the luma dynamic range is low,
		// otherwise we contrast-stretch.
		priv->use_flat_scale =
			(((int)priv->y_max - (int)priv->y_min) < priv->y_min_dynamic_range) ? 1 : 0;			
		
		// The index into our LUT for contrast stretching.
		priv->stretch_idx = priv->y_max - priv->y_min;
	}

	return 0;
}

static void
ambitv_yuv_enhance_apply_color_effect(
	struct ambitv_coloreffect_component* component, int color[3], enum ambitv_video_format fmt
) {
	struct ambitv_yuv_enhance_coloreffect_priv* priv =
		(struct ambitv_yuv_enhance_coloreffect_priv*)component->priv;
	
	if (ambitv_video_format_yuyv != fmt) {
		return;
	}
	
	if (priv->use_flat_scale) {
		color[0] = ((uint32_t)color[0] * priv->lut_y_scale[priv->y_max]) >> FIXPT;
	} else {
		uint32_t yy =
			(((uint32_t)color[0] - (uint32_t)priv->y_min) * priv->lut_y_stretch[priv->stretch_idx]) >> FIXPT;
		color[0] = CONSTRAIN(yy, 0, 255);
	}
	
	// We pick the saturation scale depending on the more saturated component,
	// so that we scale colors less if one component is more saturated than the
	// other. Otherwise, we'd oversaturate colors that fall firmly into either
	// the U/V sector.
	int32_t uu = (int32_t)color[1], vv = (int32_t)color[2];
	uint8_t v = ABS(uu-128) > ABS(vv-128) ? color[1] : color[2];
	int32_t s = (priv->lut_uv_scale[v] * priv->lut_uv_exp[priv->uv_max]) >> FIXPT;
	
	int32_t su = ((((int32_t)(1<<FIXPT) + s) * ((uu - 128))) >> FIXPT) + 128;
	int32_t sv = ((((int32_t)(1<<FIXPT) + s) * ((vv - 128))) >> FIXPT) + 128;
	
	color[1] = CONSTRAIN(su, 0, 255);
	color[2] = CONSTRAIN(sv, 0, 255);
}

static void
ambitv_yuv_enhance_free(struct ambitv_coloreffect_component* component)
{
   free(component->priv);
}

static void
ambitv_yuv_enhance_print_configuration(struct ambitv_coloreffect_component* component)
{
   struct ambitv_yuv_enhance_coloreffect_priv* priv =
      (struct ambitv_yuv_enhance_coloreffect_priv*)component->priv;

   ambitv_log(ambitv_log_info,
      "\ty-scan-stride:         %d\n"
      "\ty-histogram-inset:     %f\n"
			"\ty-min-dynamic-range:   %d\n"
			"\ty-flat-scale:          %f\n"
			"\tuv-boost:              %f\n",
			priv->y_scan_stride,
			priv->y_histogram_inset_percent,
			priv->y_min_dynamic_range,
			priv->y_flat_scale,
			priv->uv_boost
   );
}

static void*
ambitv_yuv_enhance_ptr_for_option(
	struct ambitv_coloreffect_component* enhancer,
	char opt
) {
	struct ambitv_yuv_enhance_coloreffect_priv* priv =
		(struct ambitv_yuv_enhance_coloreffect_priv*)enhancer->priv;
	
	switch (opt) {
		case 's': return &priv->y_scan_stride;
		case 'h': return &priv->y_histogram_inset_percent;
		case 'd': return &priv->y_min_dynamic_range;
		case 'f': return &priv->y_flat_scale;
		case 'g': return &priv->uv_boost;
		default: break;
	}
	
	return NULL;
}

static int
ambitv_yuv_enhance_configure(struct ambitv_coloreffect_component* enhancer, int argc, char** argv)
{
   int c, ret = 0;
   
   struct ambitv_yuv_enhance_coloreffect_priv* priv =
		 (struct ambitv_yuv_enhance_coloreffect_priv*)enhancer->priv;
   
	 if (NULL == priv)
      return -1;
   
   static struct option lopts[] = {
      { "y-scan-stride", required_argument, 0, 's' },
      { "y-histogram-inset", required_argument, 0, 'h' },
      { "y-min-dynamic-range", required_argument, 0, 'd' },
      { "y-flat-scale", required_argument, 0, 'f' },
      { "uv-boost", required_argument, 0, 'g' },
      { NULL, 0, 0, 0 }
   };
   
   while (1) {      
      c = getopt_long(argc, argv, "", lopts, NULL);
      
      if (c < 0)
         break;
         
      switch (c) {
				 case 's':
				 case 'd': {
					 if (0 > ambitv_assign_int_option(
						 ambitv_yuv_enhance_ptr_for_option(enhancer, c),
					   optarg,
						 argv[optind-2],
						 LOGNAME
					 )) {
						 return -1;
					 }
					 break;
				 }

				 case 'h':
				 case 'f':
				 case 'g': {
					 if (0 > ambitv_assign_float_option(
						 ambitv_yuv_enhance_ptr_for_option(enhancer, c),
					   optarg,
						 argv[optind-2],
						 LOGNAME
					 )) {
						 return -1;
					 }
					 break;
				 }
         
         default:
            break;
      }
   }
   
   if (optind < argc) {
      ambitv_log(ambitv_log_error, LOGNAME "extraneous configuration argument: '%s'.\n",
         argv[optind]);
      ret = -1;
   }
   
   return ret;
}

struct ambitv_coloreffect_component*
ambitv_yuv_enhance_create(const char* name, int argc, char** argv)
{
  struct ambitv_coloreffect_component* enhancer =
     ambitv_coloreffect_component_create(name);
  
  if (NULL != enhancer) {
    struct ambitv_yuv_enhance_coloreffect_priv* priv =
       (struct ambitv_yuv_enhance_coloreffect_priv*)malloc(sizeof(struct ambitv_yuv_enhance_coloreffect_priv));
    memset(priv, 0, sizeof(struct ambitv_yuv_enhance_coloreffect_priv));

    enhancer->priv = (void*)priv;
		
		priv->y_scan_stride = DEFAULT_Y_SCAN_STRIDE;
		priv->y_histogram_inset_percent = DEFAULT_Y_HISTOGRAM_INSET;
		priv->y_min_dynamic_range = DEFAULT_Y_MIN_DYN_RANGE;
		priv->y_flat_scale = DEFAULT_Y_FLAT_SCALE;
		priv->uv_boost = DEFAULT_UV_BOOST;
		
    if (ambitv_yuv_enhance_configure(enhancer, argc, argv) < 0)
       goto errReturn;
		
		enhancer->f_print_configuration = ambitv_yuv_enhance_print_configuration;
		enhancer->f_free_priv = ambitv_yuv_enhance_free;
		enhancer->f_prepare_for_frame = ambitv_yuv_enhance_prepare_for_frame;
		enhancer->f_apply_color_effect = ambitv_yuv_enhance_apply_color_effect;
	}
	
	return enhancer;
	
errReturn:
   ambitv_coloreffect_component_free(enhancer);
 
   return NULL;
}

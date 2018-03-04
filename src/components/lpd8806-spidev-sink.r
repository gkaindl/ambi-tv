
#ifndef AMBITV_LPD8806_SPIDEV_SINK_R
#define AMBITV_LPD8806_SPIDEV_SINK_R

#include "../component.h"

struct ambitv_lpd8806_priv {
   char*             device_name;
   int               fd, spi_speed, num_leds, actual_num_leds, grblen;
   int               led_len[4], *led_str[4];   // top, bottom, left, right
   double            led_inset[4];              // top, bottom, left, right
   unsigned char*    grb;
   unsigned char**   bbuf;
   int               num_bbuf, bbuf_idx;
   double            gamma[3];      // RGB gamma, not GRB!
   unsigned char*    gamma_lut[3];  // also RGB
};

int*
ambitv_lpd8806_ptr_for_output(struct ambitv_lpd8806_priv* lpd8806, int output, int* led_str_idx, int* led_idx);

int
ambitv_lpd8806_map_output_to_point(
   struct ambitv_sink_component* component,
   int output,
   int width,
   int height,
   int* x,
   int* y
);

#endif /* AMBITV_LPD8806_SPIDEV_SINK_R */

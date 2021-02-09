/*
 * pwm_dev.h
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef __PWM_DEV_H__
#define __PWM_DEV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rpihw.h"
#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"

#define TRAILING_LEDS			 10

#define TARGET_FREQ              800000   // Can go as low as 400000
#define GPIO_PIN                 18
#define DMA                      5
#define SYMBOL_HIGH              0x6  // 1 1 0
#define SYMBOL_LOW               0x4  // 1 0 0

#define LED_RESET_uS                             55
#define LED_BIT_COUNT(leds, freq)                ((leds * 3 * 8 * 3) + ((LED_RESET_uS * \
                                                  (freq * 3)) / 1000000))
#define PWM_BYTE_COUNT(leds, freq)               (((((LED_BIT_COUNT(leds, freq) >> 3) & ~0x7) + 4) + 4) * \
                                                  RPI_PWM_CHANNELS)

// We use the mailbox interface to request memory from the VideoCore.
// This lets us request one physically contiguous chunk, find its
// physical address, and map it 'uncached' so that writes from this
// code are immediately visible to the DMA controller.  This struct
// holds data relevant to the mailbox interface.
typedef struct videocore_mbox {
    int handle;             /* From mbox_open() */
    unsigned mem_ref;       /* From mem_alloc() */
    unsigned bus_addr;      /* From mem_lock() */
    unsigned size;          /* Size of allocation */
    uint8_t *virt_addr;     /* From mapmem() */
} videocore_mbox_t;

typedef struct pwm_dev_device
{
    volatile uint8_t *pwm_raw;
    volatile dma_t *dma;
    volatile pwm_t *pwm;
    volatile dma_cb_t *dma_cb;
    uint32_t dma_cb_addr;
    volatile gpio_t *gpio;
    volatile cm_pwm_t *cm_pwm;
    videocore_mbox_t mbox;
    int max_count;
} pwm_dev_device_t;

extern pwm_dev_device_t pwm_dev_device;

#define PWM_DEV_TARGET_FREQ                       800000   // Can go as low as 400000

#define PWM_DEV_STRIP_RGB                         0x100800
#define PWM_DEV_STRIP_RBG                         0x100008
#define PWM_DEV_STRIP_GRB                         0x081000
#define PWM_DEV_STRIP_GBR                         0x080010
#define PWM_DEV_STRIP_BRG                         0x001008
#define PWM_DEV_STRIP_BGR                         0x000810

struct pwm_dev_device;

//typedef uint32_t pwm_dev_led_t;                   //< 0x00RRGGBB
typedef struct
{
    int gpionum;                                 //< GPIO Pin with PWM alternate function, 0 if unused
    int invert;                                  //< Invert output signal
    int count;                                   //< Number of LEDs, 0 if channel is unused
    int brightness;                              //< Brightness value between 0 and 255
    int strip_type;                              //< Strip color layout -- one of PWM_DEV_STRIP_xxx constants
    unsigned char *leds;                         //< LED buffers, allocated by driver based on count
} pwm_dev_channel_t;

typedef struct
{
    struct pwm_dev_device *device;                //< Private data for driver use
    const rpi_hw_t *rpi_hw;                      //< RPI Hardware Information
    uint32_t freq;                               //< Required output frequency
    int dmanum;                                  //< DMA number _not_ already in use
    pwm_dev_channel_t channel[RPI_PWM_CHANNELS];
} pwm_dev_t;

extern pwm_dev_t pwm_dev;


int pwm_dev_init(pwm_dev_t *pwm_dev);               //< Initialize buffers/hardware
void pwm_dev_fini(pwm_dev_t *pwm_dev);              //< Tear it all down
int pwm_dev_render(pwm_dev_t *pwm_dev);             //< Send LEDs off to hardware
int pwm_dev_wait(pwm_dev_t *pwm_dev);               //< Wait for DMA completion
void dma_start(pwm_dev_t *pwm_dev);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_DEV_H__ */


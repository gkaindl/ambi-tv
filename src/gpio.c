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
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "gpio.h"
#include "log.h"

#define LOGNAME      "gpio: "

#define SYSFS_GPIO_BASE    "/sys/class/gpio"

static int
ambitv_gpio_open_button_irq_sysfs(int num_gpio)
{
   int fd = -1, len;
   char buf[64];
   
   fd = open(SYSFS_GPIO_BASE "/export", O_WRONLY);
   if (fd < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to open %s : %d (%s).\n",
         SYSFS_GPIO_BASE "/export", errno, strerror(errno));
      return fd;
   }
   
   len = snprintf(buf, sizeof(buf), "%d", num_gpio);
   
   if (write(fd, buf, len) != len) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to write to %s : %d (%s).\n",
         SYSFS_GPIO_BASE "/export", errno, strerror(errno));
      close(fd);
      return -1;
   }
   
   close(fd);
   
   len = snprintf(buf, sizeof(buf), SYSFS_GPIO_BASE  "/gpio%d/direction", num_gpio);
   fd  = open(buf, O_WRONLY);
   if (fd < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to open %s : %d (%s).\n",
         buf, errno, strerror(errno));
      return fd;
   }
   
   if (3 != write(fd, "in", 3)) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to write to %s : %d (%s).\n",
         buf, errno, strerror(errno));
      close(fd);
      return -1;
   }
   
   close(fd);
   
   len = snprintf(buf, sizeof(buf), SYSFS_GPIO_BASE  "/gpio%d/edge", num_gpio);
   fd  = open(buf, O_WRONLY);
   if (fd < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to open %s : %d (%s).\n",
         buf, errno, strerror(errno));
      return fd;
   }
   
   if (8 != write(fd, "falling", 8)) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to write to %s : %d (%s).\n",
         buf, errno, strerror(errno));
      close(fd);
      return -1;
   }
   
   close(fd);
   
   len = snprintf(buf, sizeof(buf), SYSFS_GPIO_BASE "/gpio%d/value", num_gpio);
   fd = open(buf, O_RDONLY | O_NONBLOCK);
   if (fd < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to open %s : %d (%s).\n",
         buf, errno, strerror(errno));
   }
   
   return fd;
}

void
ambitv_gpio_close_button_irq_sysfs(int fd, int num_gpio)
{
   int len;
   char buf[16];
   
   close(fd);
   
   fd = open(SYSFS_GPIO_BASE "/unexport", O_WRONLY);
   if (fd < 0) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to open %s : %d (%s).\n",
         SYSFS_GPIO_BASE "/unexport", errno, strerror(errno));
      return;
   }
   
   len = snprintf(buf, sizeof(buf), "%d", num_gpio);
   if (len != write(fd, buf, len)) {
      ambitv_log(ambitv_log_error, LOGNAME "failed to write to %s : %d (%s).\n",
         buf, errno, strerror(errno));
   }
   
   close(fd);   
}

int
ambitv_gpio_open_button_irq(int num_gpio)
{
   return ambitv_gpio_open_button_irq_sysfs(num_gpio);
}

void
ambitv_gpio_close_button_irq(int descriptor, int num_gpio)
{
   return ambitv_gpio_close_button_irq_sysfs(descriptor, num_gpio);
}

# ambi-tv: a flexible ambilight clone for embedded linux
# Copyright (C) 2013 Georg Kaindl
# 
# This file is part of ambi-tv.
# 
# ambi-tv is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
# 
# ambi-tv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with ambi-tv.  If not, see <http://www.gnu.org/licenses/>.
#

AMBITV = bin/ambi-tv

CFLAGS = -Wall
LDFLAGS = -lpthread -lm
CC = gcc

#
# Set the following variables to customize the build process
#
# LOCALBUILD - build the system locally with your default arch
# DEBUG			 - compile with ggdb debug flags
#

ifndef LOCALBUILD
	CFLAGS += -march=armv6 -mfpu=vfp -mfloat-abi=hard
endif

ifdef DEBUG
	CFLAGS += -ggdb
else
	CFLAGS += -O3
endif

#
# Build
#
#
all: $(AMBITV)

SRC_AMBITV_LIB = src/video-fmt.c src/parse-conf.c src/component.c   \
	src/registrations.c src/util.c src/program.c src/log.c src/color.c      \
	src/gpio.c                                                              \
	src/components/v4l2-grab-source.c src/components/avg-color-processor.c  \
	src/components/lpd8806-spidev-sink.c src/components/timer-source.c      \
	src/components/edge-color-processor.c                                   \
	src/components/mood-light-processor.c

SRC_AMBITV_MAIN = src/main.c
OBJ_AMBITV_MAIN = $(SRC_AMBITV_MAIN:.c=.o)

OBJ_AMBITV_LIB = $(SRC_AMBITV_LIB:.c=.o)

dir=@mkdir -p bin

bin/ambi-tv: $(OBJ_AMBITV_LIB) $(OBJ_AMBITV_MAIN)
	$(dir)
	$(CC) $(LDFLAGS) $^ -o $@      

#
# Automated tests
#

SRC_TESTS = $(wildcard src/test/*.c)

OBJ_TESTS = $(SRC_TESTS:.c=.o)

bin/testrunner: LDFLAGS += -lcunit
bin/testrunner: CFLAGS += -Isrc
bin/testrunner: $(OBJ_AMBITV_LIB) $(OBJ_TESTS)
	$(dir)
	$(CC) $(LDFLAGS) $^ -o $@

test: bin/testrunner
	./bin/testrunner

#
# Common goals
#

.c.o:
	gcc $(CFLAGS) -c $< -o $@    

clean:
	rm -f $(OBJ_AMBITV_LIB)
	rm -f $(OBJ_TESTS)
	rm -rf bin

.PHONY = all clean


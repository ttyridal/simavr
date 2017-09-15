# 
# 	Copyright 2017 Torbjorn Tyridal <ttyridal@gmail.com>
#
#	This file is part of simavr.
#
#	simavr is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	simavr is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with simavr.  If not, see <http://www.gnu.org/licenses/>.

target=	ssd1306demo
firm_src = ${wildcard at*${board}.c}
firmware = ${firm_src:.c=.axf}
simavr = ../../

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim
IPATH += ${simavr}/simavr/sim
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts

LDFLAGS += -lpthread

OBJAVR=obj_avr

include ../Makefile.opengl

all: obj ${OBJAVR} atmega32u4_ssd1306.axf ${target}

VPATH+=arduino:adafruit
avrobj = ${OBJAVR}/atmega32u4_ssd1306.o ${OBJAVR}/simdef.o 
avrobj += ${OBJAVR}/GFX.o 
avrobj += ${OBJAVR}/SSD1306.o 
avrobj += ${OBJAVR}/Print.o
avrobj += ${OBJAVR}/Wire.o
avrobj += ${OBJAVR}/twi.o

part = atmega32u4

${OBJAVR}/%.o: %.c
	@echo AVR-CC ${^}
	@${AVR}gcc -c -Wall -gdwarf-2 -Os -std=gnu99 \
			-mmcu=${part} -DPART=${part} \
			-DF_CPU=8000000 \
			-fno-inline-small-functions \
			-ffunction-sections -fdata-sections \
			-I../simavr/sim/avr -I../../simavr/sim/avr \
			${^} -o ${@}

${OBJAVR}/%.o: %.cpp
	@echo AVR-CXX ${^}
	@${AVR}g++ -c -Wall -gdwarf-2 -Os -std=gnu++14 \
			-mmcu=${part} -DPART=${part} \
			-DF_CPU=8000000 -DARDUINO=100 \
			-I./ \
			-fno-inline-small-functions \
			-ffunction-sections -fdata-sections \
			-fno-exceptions \
			-I../simavr/sim/avr -I../../simavr/sim/avr \
			${^} -o ${@}


atmega32u4_ssd1306.axf: ${avrobj}
	@echo AVR-LD ${@}
	@${AVR}g++ -mmcu=${part} \
			-Wl,--relax,--gc-sections \
			-Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 \
			${^} -o ${@}
	@${AVR}size ${@}|sed '1d'


include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/ac_input.o
${board} : ${OBJ}/ssd1306_virt.o
${board} : ${OBJ}/ssd1306_glut.o
${board} : ${OBJ}/${target}.o

${target}: ${board}
	@echo $@ done


${OBJAVR}:
	${E}mkdir -p ${OBJAVR}
clean-${OBJAVR}:
	rm -rf ${OBJAVR}


clean: clean-${OBJ} clean-${OBJAVR}
	rm -rf *.hex *.a *.axf ${target} *.vcd .*.swo .*.swp .*.swm .*.swn

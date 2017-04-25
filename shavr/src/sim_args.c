/*
	sim_args.c

	Copyright 2017 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "sim_hex.h"
#include "sim_vcd_file.h"

#include "sim_core_decl.h"

void
sim_args_display_usage(
	const char * app)
{
	printf("Usage: %s [...] <firmware>\n", app);
	printf( "		[--freq|-f <freq>]  Sets the frequency for an .hex firmware\n"
			"		[--mcu|-m <device>] Sets the MCU type for an .hex firmware\n"
			"       [--list-cores]      List all supported AVR cores and exit\n"
			"       [--help|-h]         Display this usage message and exit\n"
			"       [--trace, -t]       Run full scale decoder trace\n"
			"       [-ti <vector>]      Add traces for IRQ vector <vector>\n"
			"       [--gdb|-g]          Listen for gdb connection on port 1234\n"
			"       [-ff <.hex file>]   Load next .hex file as flash\n"
			"       [-ee <.hex file>]   Load next .hex file as eeprom\n"
			"       [--input|-i <file>] A .vcd file to use as input signals\n"
			"       [-v]                Raise verbosity level\n"
			"                           (can be passed more than once)\n"
			"       <firmware>          A .hex or an ELF file. ELF files are\n"
			"                           prefered, and can include debugging syms\n");
}

void
sim_args_list_cores()
{
	printf( "Supported AVR cores:\n");
	for (int i = 0; avr_kind[i]; i++) {
		printf("       ");
		for (int ti = 0; ti < 4 && avr_kind[i]->names[ti]; ti++)
			printf("%s ", avr_kind[i]->names[ti]);
		printf("\n");
	}
}

typedef struct sim_args_t {
	elf_firmware_t f;	// ELF firmware
	uint32_t f_cpu;	// AVR core frequency
	char name[24];	// AVR core name
	uint32_t trace : 1, gdb : 1,  log : 4;

	uint8_t trace_vectors[8];
	int trace_vectors_count;

	const char vcd_input[1024];
} sim_args_t;

typedef int (*sim_args_parse_t)(
	sim_args_t *a,
	int argc,
	char *argv[]);

int
sim_args_parse(
	sim_args_t *a,
	int argc,
	char *argv[],
	sim_args_parse_t passthru)
{
}


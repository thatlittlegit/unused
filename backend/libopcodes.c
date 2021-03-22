/* backend/libopcodes.c: backend implementation for libopcodes
 *
 * Copyright (C) 2021 thatlittlegit
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, under version 3 of the License,
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _POSIX_C_SOURCE 200809L /* fmemopen */

#include "../backend.h"
#include "../debug.h"
#include "../dict.h"
#include <alloca.h>
#include <assert.h>
#include <bfd.h>
#include <dis-asm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fake_fprintf_data {
	FILE* stream;
	int waiting;
	bfd_vma value;
};

static int
fake_fprintf(struct fake_fprintf_data* data, const char* str, const char* arg)
{
	/*
	 * NOTE: libopcodes does really weird stuff with fprintf. It is used
	 * for formatting, but it gets strings like "       #" *then* "0x%s",
	 * instead of doing it all at once. Why? No clue. However, this
	 * function looks for the earlier '#', and sets a flag indicating it
	 * should intercept the next string.
	 *
	 * If this changes in future, then this will have to change too. I'm
	 * pretty sure that libopcodes statically links, though, so it should
	 * be fixable. (Besides, we assert based on sizeof(bfd) at the very
	 * start.)
	 */
	if (data->waiting) {
		data->value = strtoul(arg, NULL, 16);
		data->waiting = 0;
		return 0;
	}

	if (str[0] == ' ' && strchr(str, '#')) {
		data->waiting = 1;
		return 0;
	}

	if (debug_stream)
		return fprintf(data->stream, str, arg);

	return 0;
}

void
uu_backend(bfd* bfd, struct bfd_section* section, struct uu_dict* dict)
{
	char* buffer;
	struct fake_fprintf_data fprintf_data;
	FILE* buffer_file;

	enum bfd_architecture arch;
	unsigned long mach;
	disassembler_ftype disasm;
	struct disassemble_info info;

	bfd_vma pc;
	int disasm_result;

	if (!(section->flags & SEC_CODE))
		return;

	debug("starting disassembly of section %s\n", section->name);

	arch = bfd_get_arch(bfd);
	mach = bfd_get_mach(bfd);
	disasm = disassembler(arch, 0, mach, bfd);

	if (debug_stream) {
		buffer = alloca(1024);
		buffer[0] = 0;
		buffer_file = fmemopen(buffer, 1024, "w");
		assert(buffer_file);
	} else {
		buffer = NULL;
		buffer_file = NULL;
	}

	fprintf_data.stream = buffer_file;
	fprintf_data.waiting = 0;
	fprintf_data.value = 0;

	init_disassemble_info(&info, (FILE*)&fprintf_data,
			      (fprintf_ftype)fake_fprintf);
	info.arch = arch;
	info.mach = mach;
	info.buffer_vma = section->vma;
	info.buffer_length = section->size;
	info.section = section;
	bfd_malloc_and_get_section(bfd, section, &info.buffer);
	disassemble_init_for_target(&info);

	pc = section->vma;
	do {
		disasm_result = disasm(pc, &info);
		pc += disasm_result;

		if (debug_stream) {
			fputc('\0', buffer_file);
			fflush(buffer_file);
			rewind(buffer_file);
		}

		debug(" %.8lx> [%d] %s\n", pc, info.insn_type, buffer);

		if (fprintf_data.value) {
			debug(" 0-------> %lx\n", fprintf_data.value);
			uu_dict_remove(dict, fprintf_data.value);
			fprintf_data.value = 0;
		}

		if (info.target) {
			debug(" 1-------> %lx\n", info.target);
			uu_dict_remove(dict, info.target);
		}

		if (info.target2) {
			debug(" 2-------> %lx\n", info.target2);
			uu_dict_remove(dict, info.target2);
		}
	} while (pc < section->vma + section->size && disasm_result > 0);

	if (buffer_file)
		fclose(buffer_file);
	free(info.buffer);
}

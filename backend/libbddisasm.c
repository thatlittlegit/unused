/* backend/bddisasm.c: backend implementation for bddisasm
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
#define _XOPEN_SOURCE 500

#include "../backend.h"
#include "../debug.h"
#include "../dict.h"
#include <assert.h>
#include <bfd.h>
#include <stdarg.h>
#include <string.h>

#define bool int
#include <bddisasm/bddisasm.h>

int
nd_vsnprintf_s(
    char* buffer, size_t size, size_t count, const char* format, va_list argptr)
{
	(void)count;
	return vsnprintf(buffer, size, format, argptr);
}

void*
nd_memset(void* s, int c, size_t n)
{
	return memset(s, c, n);
}

void
uu_backend(bfd* bfd, struct bfd_section* section, struct uu_dict* dict)
{
	bfd_byte* data;
	bfd_vma pc;

	if (!(section->flags & SEC_CODE))
		return;

	debug("starting disassembly of section %s\n", section->name);

	assert(bfd_get_arch(bfd) == bfd_arch_i386);
	bfd_malloc_and_get_section(bfd, section, &data);

	pc = 0;
	while (pc < section->size) {
		INSTRUX insn;
		NDSTATUS status;
		unsigned i;

		memset(insn.Operands, 0, sizeof(ND_OPERAND) * 10);
		status = NdDecodeEx(&insn, data + pc, section->size - pc,
				    ND_CODE_64, ND_DATA_64);

		if (!ND_SUCCESS(status)) {
			pc += 1;
			debug("!--------> (unknown)\n");
			continue;
		}

		pc += insn.Length;

		if (debug_stream) {
			char line[ND_MIN_BUF_SIZE];
			if (!ND_SUCCESS(NdToText(&insn, 0, sizeof(line), line)))
				strcpy(line, "(unknown)");

			debug(" %.8lx> %s\n", pc - insn.Length, line);
		}

		for (i = 0; i < 10; i++) {
			ND_OPERAND* op = &insn.Operands[i];
			bfd_vma location;

			if (op->Type == ND_OP_OFFS) {
				location = op->Info.RelativeOffset.Rel;
				debug("%d--------> %lx\n", i, location);
				uu_dict_remove(dict, location);
				continue;
			}

			if (op->Type != ND_OP_MEM)
				continue;

			location = op->Info.Memory.Disp + section->vma + pc
			    - insn.Length;
			debug("%d--------> %lx\n", i, location);
			uu_dict_remove(dict, location);
		}
	}
}

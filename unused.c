/* unused.c: main file for unused
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
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "dict.h"
#include <assert.h>
#include <bfd.h>
#include <dis-asm.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* progname = "unused";
FILE* debug;

struct options {
	const char* filename;
	int debug;
};

struct definition {
	struct definition* next;
	const char* name;
	unsigned id;
};

struct fake_fprintf_data {
	FILE* stream;
	int waiting;
	bfd_vma value;
};

static int
parse_options(int argc, char** argv, struct options* opts)
{
	int c;
	opterr = 0;
	while ((c = getopt(argc, argv, "d")) > 0)
		switch (c) {
		case 'd':
			opts->debug = 1;
			break;
		default:
			fprintf(stderr, "%s: unknown argument '%c'\n", progname,
				c);
			return -1;
		}

	opts->filename = argv[optind];
	return 0;
}

static int
get_symbols(bfd* bfd,
	    struct bfd_symbol*** static_syms,
	    unsigned* static_len,
	    struct bfd_symbol*** dynamic_syms,
	    unsigned* dynamic_len)
{
	long static_size;
	long dynamic_size;

	*static_syms = NULL;
	*dynamic_syms = NULL;

	static_size = bfd_get_symtab_upper_bound(bfd);
	if (static_size < 0)
		goto fail;

	dynamic_size = bfd_get_dynamic_symtab_upper_bound(bfd);
	if (dynamic_size < 0)
		goto fail;

	*static_syms = (struct bfd_symbol**)malloc(static_size);
	if (static_size > 0 && *static_syms == NULL)
		goto fail;

	/* why is the function named this?! */
	*static_len = bfd_canonicalize_symtab(bfd, *static_syms);

	*dynamic_syms = (struct bfd_symbol**)malloc(dynamic_size);
	if (dynamic_size > 0 && *dynamic_syms == NULL)
		goto fail;

	*dynamic_len = bfd_canonicalize_dynamic_symtab(bfd, *dynamic_syms);

	return 0;

fail:
	free(*static_syms);
	free(*dynamic_syms);

	return -1;
}

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
	const char* number_sign;

	if (data->waiting) {
		data->value = strtoul(arg, NULL, 16);
		data->waiting = 0;
		return 0;
	}

	if (str[0] == ' ') {
		number_sign = strchr(str, '#');

		if (number_sign != NULL) {
			data->waiting = 1;
			return 0;
		}
	}

	if (debug)
		return fprintf(data->stream, str, arg);

	return 0;
}

static void
process_section(bfd* bfd, struct bfd_section* section, void* _data)
{
	char buffer[1024];
	struct fake_fprintf_data fprintf_data;
	FILE* buffer_file;

	enum bfd_architecture arch;
	unsigned long mach;
	disassembler_ftype disasm;
	struct disassemble_info info;

	bfd_vma pc;
	int disasm_result;

	struct uu_dict* dict;

	if (!(section->flags & SEC_CODE))
		return;

	fprintf(debug, "starting disassembly of section %s\n", section->name);

	arch = bfd_get_arch(bfd);
	mach = bfd_get_mach(bfd);
	disasm = disassembler(arch, 0, mach, bfd);

	buffer[0] = 0;
	buffer_file = fmemopen(buffer, sizeof(buffer), "w");
	assert(buffer_file);

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

	dict = (struct uu_dict*)_data;

	pc = section->vma;
	do {
		disasm_result = disasm(pc, &info);
		pc += disasm_result;

		fputc('\0', buffer_file);
		fflush(buffer_file);
		rewind(buffer_file);


		fprintf(debug, " %.8lx> [%d] %s\n", pc, info.insn_type, buffer);

		if (fprintf_data.value) {
			fprintf(debug, " 0-------> %lx\n", fprintf_data.value);
			uu_dict_remove(dict, fprintf_data.value);
		}

		if (info.target) {
			fprintf(debug, " 1-------> %lx\n", info.target);
			uu_dict_remove(dict, info.target);
		}

		if (info.target2) {
			fprintf(debug, " 2-------> %lx\n", info.target2);
			uu_dict_remove(dict, info.target2);
		}
	} while (pc < section->vma + section->size && disasm_result > 0);

	fclose(buffer_file);
	free(info.buffer);
}

static void
process_symbol(struct bfd_symbol* sym, void* data)
{
	bfd_vma location;
	(void)data;

	location = sym->value + sym->section->vma;
	fprintf(stdout, "%lx\t%s\n", location, sym->name);
}

int
main(int argc, char** argv)
{
	int status;
	struct options opts;
	FILE* input;

	bfd* bfd;
	struct bfd_symbol** static_symbols;
	unsigned static_len;
	struct bfd_symbol** dynamic_symbols;
	unsigned dynamic_len;

	struct uu_dict* table;

	unsigned i;

	status = EXIT_SUCCESS;
	input = stdin;

	opts.filename = NULL;
	opts.debug = 0;

	assert(bfd_init() == BFD_INIT_MAGIC);

	if (argc >= 1)
		progname = argv[0];

	if (parse_options(argc, argv, &opts) < 0) {
		status = EXIT_FAILURE;
		goto cleanup_none;
	}

	if (opts.filename != NULL)
		input = fopen(opts.filename, "r");

	if (input == NULL) {
		fprintf(stderr, "%s: failed to open input file '%s': %s\n",
			progname, opts.filename, strerror(errno));
		status = EXIT_FAILURE;
		goto cleanup_none;
	}

	if (opts.filename == NULL)
		opts.filename = "(stdin)";

	if (opts.debug)
		debug = stderr;
	else
#ifdef _WIN32
		debug = fopen("nul", "w");
#else
		debug = fopen("/dev/null", "w");
#endif

	assert(debug != NULL);

	bfd = bfd_openstreamr(opts.filename, NULL, input);
	if (bfd == NULL) {
		fprintf(stderr, "%s: failed to read input file '%s': %s\n",
			progname, opts.filename, bfd_errmsg(bfd_get_error()));
		status = EXIT_FAILURE;
		goto cleanup_stream;
	}

	if (!bfd_check_format(bfd, bfd_object)) {
		fprintf(stderr, "%s: failed to get type of '%s': %s\n", progname,
			opts.filename, bfd_errmsg(bfd_get_error()));
		status = EXIT_FAILURE;
		goto cleanup_bfd;
	}

	if (get_symbols(bfd, &static_symbols, &static_len, &dynamic_symbols,
			&dynamic_len)
	    < 0) {
		fprintf(stderr, "%s: failed to get symbols in '%s': %s\n",
			progname, opts.filename, bfd_errmsg(bfd_get_error()));
		status = EXIT_FAILURE;
		goto cleanup_bfd;
	}

	table = uu_dict_new();

	for (i = 0; i < static_len + dynamic_len; i++) {
		struct bfd_symbol* sym;

		if (i < static_len)
			sym = static_symbols[i];
		else
			sym = dynamic_symbols[i - static_len];

		if (!(sym->section->flags & SEC_CODE))
			continue;
		if (sym->value == 0)
			continue;

		uu_dict_add(table, sym->value + sym->section->vma, sym);
	}

	bfd_map_over_sections(bfd, process_section, table);
	uu_dict_for_each(table, (void (*)(void*, void*))process_symbol, NULL);

	uu_dict_free(table);
	free(static_symbols);
	free(dynamic_symbols);

cleanup_bfd:
	bfd_close(bfd);
	input = NULL;
cleanup_stream:
	if (input)
		fclose(input);
	fclose(debug);
cleanup_none:
	return status;
}

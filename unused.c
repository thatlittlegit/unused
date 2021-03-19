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

static void
process_section(bfd* bfd, struct bfd_section* section, void* _ret)
{
	char buffer[1024];
	FILE* buffer_file;

	enum bfd_architecture arch;
	unsigned long mach;
	disassembler_ftype disasm;
	struct disassemble_info info;

	bfd_vma pc;
	int disasm_result;
	char* number_sign;
	bfd_vma nouveaux[3];

	bfd_vma** ret = (bfd_vma**)_ret;
	unsigned ret_len;

	if (!(section->flags & SEC_CODE))
		return;

	fprintf(debug, "starting disassembly of section %s\n", section->name);

	arch = bfd_get_arch(bfd);
	mach = bfd_get_mach(bfd);
	disasm = disassembler(arch, 0, mach, bfd);

	buffer[0] = 0;
	buffer_file = fmemopen(buffer, sizeof(buffer), "w");
	assert(buffer_file);

	init_disassemble_info(&info, buffer_file, (fprintf_ftype)fprintf);
	info.arch = arch;
	info.mach = mach;
	info.buffer_vma = section->vma;
	info.buffer_length = section->size;
	info.section = section;
	bfd_malloc_and_get_section(bfd, section, &info.buffer);
	disassemble_init_for_target(&info);

	ret_len = 0;
	if (*ret)
		for (; (*ret)[ret_len] != 0; ++ret_len)
			continue;

	pc = section->vma;
	do {
		disasm_result = disasm(pc, &info);
		pc += disasm_result;

		fputc('\0', buffer_file);
		fflush(buffer_file);
		rewind(buffer_file);

		memset(nouveaux, 0, sizeof(nouveaux));

		/* HACK there doesn't seem to be a good way to get this
		 * information otherwise :( */
		number_sign = strchr(buffer, '#');
		if (number_sign) {
			nouveaux[0] = strtoul(number_sign + 4, NULL, 16);
			*number_sign = 0;
		}

		if (info.target)
			nouveaux[1] = info.target;

		if (info.target2)
			nouveaux[2] = info.target2;

		fprintf(debug, " %.8lx> [%d] %s\n", pc, info.insn_type, buffer);

		for (unsigned i = 0; i < 3; i++) {
			if (nouveaux[i] == 0)
				continue;

			*ret = reallocarray(*ret, ++ret_len, sizeof(bfd_vma));
			(*ret)[ret_len - 1] = nouveaux[i];

			fprintf(debug, "%d--------> 0x%lx\n", i, nouveaux[i]);
		}
	} while (pc < section->vma + section->size && disasm_result > 0);

	fclose(buffer_file);

	*ret = reallocarray(*ret, ++ret_len, sizeof(bfd_vma));
	(*ret)[ret_len - 1] = 0;
}

static void
process_symbol(struct bfd_symbol* sym, bfd_vma* references)
{
	bfd_vma location;

	if (!(sym->section->flags & SEC_CODE))
		return;
	if (sym->value == 0)
		return;

	location = sym->value + sym->section->vma;

	fprintf(debug, "checking uses of %s (0x%lx)... ", sym->name, location);

	for (unsigned i = 0; references[i] != 0; ++i) {
		if (references[i] == location) {
			fputs("found!\n", debug);
			return;
		}
	}

	fputs("not found...\n", debug);
}

int
main(int argc, char** argv)
{
	int status;
	struct options opts;
	FILE* input;

	struct bfd_symbol** static_symbols;
	unsigned static_len;
	struct bfd_symbol** dynamic_symbols;
	unsigned dynamic_len;

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
		debug = fopen("nul", "r");
#else
		debug = fopen("/dev/null", "r");
#endif

	bfd* bfd = bfd_openstreamr(opts.filename, NULL, input);
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

	bfd_vma* references = (bfd_vma*)calloc(2, sizeof(bfd_vma));

	/* the start address is implicitly found */
	references[0] = bfd_get_start_address(bfd);
	references[1] = 0;

	bfd_map_over_sections(bfd, process_section, &references);

	for (unsigned i = 0; i < static_len; i++)
		process_symbol(static_symbols[i], references);
	for (unsigned i = 0; i < dynamic_len; i++)
		process_symbol(dynamic_symbols[i], references);

	free(static_symbols);
	free(dynamic_symbols);

cleanup_bfd:
	bfd_close(bfd);
	goto cleanup_none;
cleanup_stream:
	if (input)
		fclose(input);
cleanup_none:
	return status;
}

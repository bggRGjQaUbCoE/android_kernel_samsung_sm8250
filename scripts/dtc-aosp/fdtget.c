// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 *
 * Portions from U-Boot cmd_fdt.c (C) Copyright 2007
 * Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com
 * Based on code written by:
 *   Pantelis Antoniou <pantelis.antoniou@gmail.com> and
 *   Matthew McClintock <msm@freescale.com>
 */

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfdt.h>

#include "util.h"

enum display_mode {
	MODE_SHOW_VALUE,	/* show values for node properties */
	MODE_LIST_PROPS,	/* list the properties for a node */
	MODE_LIST_SUBNODES,	/* list the subnodes of a node */
};

/* Holds information which controls our output and options */
struct display_info {
	int type;		/* data type (s/i/u/x or 0 for default) */
	int size;		/* data size (1/2/4) */
	enum display_mode mode;	/* display mode that we are using */
	const char *default_val; /* default value if node/property not found */
};

static void report_error(const char *where, int err)
{
	fprintf(stderr, "Error at '%s': %s\n", where, fdt_strerror(err));
}

/**
 * Shows a list of cells in the requested format
 *
 * @param disp		Display information / options
 * @param data		Data to display
 * @param len		Maximum length of buffer
 * @param size		Data size to use for display (e.g. 4 for 32-bit)
 * @return 0 if ok, -1 on error
 */
static int show_cell_list(struct display_info *disp, const char *data, int len,
			  int size)
{
	const uint8_t *p = (const uint8_t *)data;
	char fmt[3];
	int value;
	int i;

	fmt[0] = '%';
	fmt[1] = disp->type ? disp->type : 'd';
	fmt[2] = '\0';
	for (i = 0; i < len; i += size, p += size) {
		if (i)
			printf(" ");
		value = size == 4 ? fdt32_ld((const fdt32_t *)p) :
			size == 2 ? (*p << 8) | p[1] : *p;
		printf(fmt, value);
	}

	return 0;
}

/**
 * Displays data of a given length according to selected options
 *
 * If a specific data type is provided in disp, then this is used. Otherwise
 * we try to guess the data type / size from the contents.
 *
 * @param disp		Display information / options
 * @param data		Data to display
 * @param len		Maximum length of buffer
 * @return 0 if ok, -1 if data does not match format
 */
static int show_data(struct display_info *disp, const char *data, int len)
{
	int size;
	const char *s;
	int is_string;

	/* no data, don't print */
	if (len == 0)
		return 0;

	is_string = (disp->type) == 's' ||
		(!disp->type && util_is_printable_string(data, len));
	if (is_string) {
		if (data[len - 1] != '\0') {
			fprintf(stderr, "Unterminated string\n");
			return -1;
		}
		for (s = data; s - data < len; s += strlen(s) + 1) {
			if (s != data)
				printf(" ");
			printf("%s", (const char *)s);
		}
		return 0;
	}
	size = disp->size;
	if (size == -1) {
		size = (len % 4) == 0 ? 4 : 1;
	} else if (len % size) {
		fprintf(stderr, "Property length must be a multiple of "
				"selected data size\n");
		return -1;
	}

	return show_cell_list(disp, data, len, size);
}

/**
 * List all properties in a node, one per line.
 *
 * @param blob		FDT blob
 * @param node		Node to display
 * @return 0 if ok, or FDT_ERR... if not.
 */
static int list_properties(const void *blob, int node)
{
	const char *name;
	int prop;

	prop = fdt_first_property_offset(blob, node);
	do {
		/* Stop silently when there are no more properties */
		if (prop < 0)
			return prop == -FDT_ERR_NOTFOUND ? 0 : prop;
		fdt_getprop_by_offset(blob, prop, &name, NULL);
		if (name)
			puts(name);
		prop = fdt_next_property_offset(blob, prop);
	} while (1);
}

#define MAX_LEVEL	32		/* how deeply nested we will go */

/**
 * List all subnodes in a node, one per line
 *
 * @param blob		FDT blob
 * @param node		Node to display
 * @return 0 if ok, or FDT_ERR... if not.
 */
static int list_subnodes(const void *blob, int node)
{
	int nextoffset;		/* next node offset from libfdt */
	uint32_t tag;		/* current tag */
	int level = 0;		/* keep track of nesting level */
	const char *pathp;
	int depth = 1;		/* the assumed depth of this node */

	while (level >= 0) {
		tag = fdt_next_tag(blob, node, &nextoffset);
		switch (tag) {
		case FDT_BEGIN_NODE:
			pathp = fdt_get_name(blob, node, NULL);
			if (level <= depth) {
				if (pathp == NULL)
					pathp = "/* NULL pointer error */";
				if (*pathp == '\0')
					pathp = "/";	/* root is nameless */
				if (level == 1)
					puts(pathp);
			}
			level++;
			if (level >= MAX_LEVEL) {
				printf("Nested too deep, aborting.\n");
				return 1;
			}
			break;
		case FDT_END_NODE:
			level--;
			if (level == 0)
				level = -1;		/* exit the loop */
			break;
		case FDT_END:
			return 1;
		case FDT_PROP:
			break;
		default:
			if (level <= depth)
				printf("Unknown tag 0x%08X\n", tag);
			return 1;
		}
		node = nextoffset;
	}
	return 0;
}

/**
 * Show the data for a given node (and perhaps property) according to the
 * display option provided.
 *
 * @param blob		FDT blob
 * @param disp		Display information / options
 * @param node		Node to display
 * @param property	Name of property to display, or NULL if none
 * @return 0 if ok, -ve on error
 */
static int show_data_for_item(const void *blob, struct display_info *disp,
		int node, const char *property)
{
	const void *value = NULL;
	int len, err = 0;

	switch (disp->mode) {
	case MODE_LIST_PROPS:
		err = list_properties(blob, node);
		break;

	case MODE_LIST_SUBNODES:
		err = list_subnodes(blob, node);
		break;

	default:
		assert(property);
		value = fdt_getprop(blob, node, property, &len);
		if (value) {
			if (show_data(disp, value, len))
				err = -1;
			else
				printf("\n");
		} else if (disp->default_val) {
			puts(disp->default_val);
		} else {
			report_error(property, len);
			err = -1;
		}
		break;
	}

	return err;
}

/**
 * Run the main fdtget operation, given a filename and valid arguments
 *
 * @param disp		Display information / options
 * @param filename	Filename of blob file
 * @param arg		List of arguments to process
 * @param arg_count	Number of arguments
 * @return 0 if ok, -ve on error
 */
static int do_fdtget(struct display_info *disp, const char *filename,
		     char **arg, int arg_count, int args_per_step)
{
	char *blob;
	const char *prop;
	int i, node;

	blob = utilfdt_read(filename, NULL);
	if (!blob)
		return -1;

	for (i = 0; i + args_per_step <= arg_count; i += args_per_step) {
		node = fdt_path_offset(blob, arg[i]);
		if (node < 0) {
			if (disp->default_val) {
				puts(disp->default_val);
				continue;
			} else {
				report_error(arg[i], node);
				free(blob);
				return -1;
			}
		}
		prop = args_per_step == 1 ? NULL : arg[i + 1];

		if (show_data_for_item(blob, disp, node, prop)) {
			free(blob);
			return -1;
		}
	}

	free(blob);

	return 0;
}

/* Usage related data. */
static const char usage_synopsis[] =
	"read values from device tree\n"
	"	fdtget <options> <dt file> [<node> <property>]...\n"
	"	fdtget -p <options> <dt file> [<node> ]...\n"
	"\n"
	"Each value is printed on a new line.\n"
	USAGE_TYPE_MSG;
static const char usage_short_opts[] = "t:pld:" USAGE_COMMON_SHORT_OPTS;
static struct option const usage_long_opts[] = {
	{"type",              a_argument, NULL, 't'},
	{"properties",       no_argument, NULL, 'p'},
	{"list",             no_argument, NULL, 'l'},
	{"default",           a_argument, NULL, 'd'},
	USAGE_COMMON_LONG_OPTS,
};
static const char * const usage_opts_help[] = {
	"Type of data",
	"List properties for each node",
	"List subnodes for each node",
	"Default value to display when the property is missing",
	USAGE_COMMON_OPTS_HELP
};

int main(int argc, char *argv[])
{
	int opt;
	char *filename = NULL;
	struct display_info disp;
	int args_per_step = 2;

	/* set defaults */
	memset(&disp, '\0', sizeof(disp));
	disp.size = -1;
	disp.mode = MODE_SHOW_VALUE;
	while ((opt = util_getopt_long()) != EOF) {
		switch (opt) {
		case_USAGE_COMMON_FLAGS

		case 't':
			if (utilfdt_decode_type(optarg, &disp.type,
					&disp.size))
				usage("invalid type string");
			break;

		case 'p':
			disp.mode = MODE_LIST_PROPS;
			args_per_step = 1;
			break;

		case 'l':
			disp.mode = MODE_LIST_SUBNODES;
			args_per_step = 1;
			break;

		case 'd':
			disp.default_val = optarg;
			break;
		}
	}

	if (optind < argc)
		filename = argv[optind++];
	if (!filename)
		usage("missing filename");

	argv += optind;
	argc -= optind;

	/* Allow no arguments, and silently succeed */
	if (!argc)
		return 0;

	/* Check for node, property arguments */
	if (args_per_step == 2 && (argc % 2))
		usage("must have an even number of arguments");

	if (do_fdtget(&disp, filename, argv, argc, args_per_step))
		return 1;
	return 0;
}

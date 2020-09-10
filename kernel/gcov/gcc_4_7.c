// SPDX-License-Identifier: GPL-2.0
/*
 *  This code provides functions to handle gcc's profiling data format
 *  introduced with gcc 4.7.
 *
 *  This file is based heavily on gcc_3_4.c file.
 *
 *  For a better understanding, refer to gcc source:
 *  gcc/gcov-io.h
 *  libgcc/libgcov.c
 *
 *  Uses gcc-internal data definitions.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include "gcov.h"

#if (__GNUC__ >= 10)
#define GCOV_COUNTERS			8
#elif (__GNUC__ >= 7)
#define GCOV_COUNTERS			9
#elif (__GNUC__ > 5) || (__GNUC__ == 5 && __GNUC_MINOR__ >= 1)
#define GCOV_COUNTERS			10
#elif __GNUC__ == 4 && __GNUC_MINOR__ >= 9
#define GCOV_COUNTERS			9
#else
#define GCOV_COUNTERS			8
#endif

#define GCOV_TAG_FUNCTION_LENGTH	3

static struct gcov_info *gcov_info_head;

/**
 * struct gcov_ctr_info - information about counters for a single function
 * @num: number of counter values for this type
 * @values: array of counter values for this type
 *
 * This data is generated by gcc during compilation and doesn't change
 * at run-time with the exception of the values array.
 */
struct gcov_ctr_info {
	unsigned int num;
	gcov_type *values;
};

/**
 * struct gcov_fn_info - profiling meta data per function
 * @key: comdat key
 * @ident: unique ident of function
 * @lineno_checksum: function lineo_checksum
 * @cfg_checksum: function cfg checksum
 * @ctrs: instrumented counters
 *
 * This data is generated by gcc during compilation and doesn't change
 * at run-time.
 *
 * Information about a single function.  This uses the trailing array
 * idiom. The number of counters is determined from the merge pointer
 * array in gcov_info.  The key is used to detect which of a set of
 * comdat functions was selected -- it points to the gcov_info object
 * of the object file containing the selected comdat function.
 */
struct gcov_fn_info {
	const struct gcov_info *key;
	unsigned int ident;
	unsigned int lineno_checksum;
	unsigned int cfg_checksum;
	struct gcov_ctr_info ctrs[0];
};

/**
 * struct gcov_info - profiling data per object file
 * @version: gcov version magic indicating the gcc version used for compilation
 * @next: list head for a singly-linked list
 * @stamp: uniquifying time stamp
 * @filename: name of the associated gcov data file
 * @merge: merge functions (null for unused counter type)
 * @n_functions: number of instrumented functions
 * @functions: pointer to pointers to function information
 *
 * This data is generated by gcc during compilation and doesn't change
 * at run-time with the exception of the next pointer.
 */
struct gcov_info {
	unsigned int version;
	struct gcov_info *next;
	unsigned int stamp;
	const char *filename;
	void (*merge[GCOV_COUNTERS])(gcov_type *, unsigned int);
	unsigned int n_functions;
	struct gcov_fn_info **functions;
};

/**
 * gcov_info_filename - return info filename
 * @info: profiling data set
 */
const char *gcov_info_filename(struct gcov_info *info)
{
	return info->filename;
}

/**
 * gcov_info_version - return info version
 * @info: profiling data set
 */
unsigned int gcov_info_version(struct gcov_info *info)
{
	return info->version;
}

/**
 * gcov_info_next - return next profiling data set
 * @info: profiling data set
 *
 * Returns next gcov_info following @info or first gcov_info in the chain if
 * @info is %NULL.
 */
struct gcov_info *gcov_info_next(struct gcov_info *info)
{
	if (!info)
		return gcov_info_head;

	return info->next;
}

/**
 * gcov_info_link - link/add profiling data set to the list
 * @info: profiling data set
 */
void gcov_info_link(struct gcov_info *info)
{
	info->next = gcov_info_head;
	gcov_info_head = info;
}

/**
 * gcov_info_unlink - unlink/remove profiling data set from the list
 * @prev: previous profiling data set
 * @info: profiling data set
 */
void gcov_info_unlink(struct gcov_info *prev, struct gcov_info *info)
{
	if (prev)
		prev->next = info->next;
	else
		gcov_info_head = info->next;
}

/**
 * gcov_info_within_module - check if a profiling data set belongs to a module
 * @info: profiling data set
 * @mod: module
 *
 * Returns true if profiling data belongs module, false otherwise.
 */
bool gcov_info_within_module(struct gcov_info *info, struct module *mod)
{
	return within_module((unsigned long)info, mod);
}

/* Symbolic links to be created for each profiling data file. */
const struct gcov_link gcov_link[] = {
	{ OBJ_TREE, "gcno" },	/* Link to .gcno file in $(objtree). */
	{ 0, NULL},
};

/*
 * Determine whether a counter is active. Doesn't change at run-time.
 */
static int counter_active(struct gcov_info *info, unsigned int type)
{
	return info->merge[type] ? 1 : 0;
}

/* Determine number of active counters. Based on gcc magic. */
static unsigned int num_counter_active(struct gcov_info *info)
{
	unsigned int i;
	unsigned int result = 0;

	for (i = 0; i < GCOV_COUNTERS; i++) {
		if (counter_active(info, i))
			result++;
	}
	return result;
}

/**
 * gcov_info_reset - reset profiling data to zero
 * @info: profiling data set
 */
void gcov_info_reset(struct gcov_info *info)
{
	struct gcov_ctr_info *ci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;

	for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) {
		ci_ptr = info->functions[fi_idx]->ctrs;

		for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
			if (!counter_active(info, ct_idx))
				continue;

			memset(ci_ptr->values, 0,
					sizeof(gcov_type) * ci_ptr->num);
			ci_ptr++;
		}
	}
}

/**
 * gcov_info_is_compatible - check if profiling data can be added
 * @info1: first profiling data set
 * @info2: second profiling data set
 *
 * Returns non-zero if profiling data can be added, zero otherwise.
 */
int gcov_info_is_compatible(struct gcov_info *info1, struct gcov_info *info2)
{
	return (info1->stamp == info2->stamp);
}

/**
 * gcov_info_add - add up profiling data
 * @dest: profiling data set to which data is added
 * @source: profiling data set which is added
 *
 * Adds profiling counts of @source to @dest.
 */
void gcov_info_add(struct gcov_info *dst, struct gcov_info *src)
{
	struct gcov_ctr_info *dci_ptr;
	struct gcov_ctr_info *sci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;
	unsigned int val_idx;

	for (fi_idx = 0; fi_idx < src->n_functions; fi_idx++) {
		dci_ptr = dst->functions[fi_idx]->ctrs;
		sci_ptr = src->functions[fi_idx]->ctrs;

		for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
			if (!counter_active(src, ct_idx))
				continue;

			for (val_idx = 0; val_idx < sci_ptr->num; val_idx++)
				dci_ptr->values[val_idx] +=
					sci_ptr->values[val_idx];

			dci_ptr++;
			sci_ptr++;
		}
	}
}

/**
 * gcov_info_dup - duplicate profiling data set
 * @info: profiling data set to duplicate
 *
 * Return newly allocated duplicate on success, %NULL on error.
 */
struct gcov_info *gcov_info_dup(struct gcov_info *info)
{
	struct gcov_info *dup;
	struct gcov_ctr_info *dci_ptr; /* dst counter info */
	struct gcov_ctr_info *sci_ptr; /* src counter info */
	unsigned int active;
	unsigned int fi_idx; /* function info idx */
	unsigned int ct_idx; /* counter type idx */
	size_t fi_size; /* function info size */
	size_t cv_size; /* counter values size */

	dup = kmemdup(info, sizeof(*dup), GFP_KERNEL);
	if (!dup)
		return NULL;

	dup->next = NULL;
	dup->filename = NULL;
	dup->functions = NULL;

	dup->filename = kstrdup(info->filename, GFP_KERNEL);
	if (!dup->filename)
		goto err_free;

	dup->functions = kcalloc(info->n_functions,
				 sizeof(struct gcov_fn_info *), GFP_KERNEL);
	if (!dup->functions)
		goto err_free;

	active = num_counter_active(info);
	fi_size = sizeof(struct gcov_fn_info);
	fi_size += sizeof(struct gcov_ctr_info) * active;

	for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) {
		dup->functions[fi_idx] = kzalloc(fi_size, GFP_KERNEL);
		if (!dup->functions[fi_idx])
			goto err_free;

		*(dup->functions[fi_idx]) = *(info->functions[fi_idx]);

		sci_ptr = info->functions[fi_idx]->ctrs;
		dci_ptr = dup->functions[fi_idx]->ctrs;

		for (ct_idx = 0; ct_idx < active; ct_idx++) {

			cv_size = sizeof(gcov_type) * sci_ptr->num;

			dci_ptr->values = vmalloc(cv_size);

			if (!dci_ptr->values)
				goto err_free;

			dci_ptr->num = sci_ptr->num;
			memcpy(dci_ptr->values, sci_ptr->values, cv_size);

			sci_ptr++;
			dci_ptr++;
		}
	}

	return dup;
err_free:
	gcov_info_free(dup);
	return NULL;
}

/**
 * gcov_info_free - release memory for profiling data set duplicate
 * @info: profiling data set duplicate to free
 */
void gcov_info_free(struct gcov_info *info)
{
	unsigned int active;
	unsigned int fi_idx;
	unsigned int ct_idx;
	struct gcov_ctr_info *ci_ptr;

	if (!info->functions)
		goto free_info;

	active = num_counter_active(info);

	for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) {
		if (!info->functions[fi_idx])
			continue;

		ci_ptr = info->functions[fi_idx]->ctrs;

		for (ct_idx = 0; ct_idx < active; ct_idx++, ci_ptr++)
			vfree(ci_ptr->values);

		kfree(info->functions[fi_idx]);
	}

free_info:
	kfree(info->functions);
	kfree(info->filename);
	kfree(info);
}

#define ITER_STRIDE	PAGE_SIZE

/**
 * struct gcov_iterator - specifies current file position in logical records
 * @info: associated profiling data
 * @buffer: buffer containing file data
 * @size: size of buffer
 * @pos: current position in file
 */
struct gcov_iterator {
	struct gcov_info *info;
	void *buffer;
	size_t size;
	loff_t pos;
};

/**
 * store_gcov_u32 - store 32 bit number in gcov format to buffer
 * @buffer: target buffer or NULL
 * @off: offset into the buffer
 * @v: value to be stored
 *
 * Number format defined by gcc: numbers are recorded in the 32 bit
 * unsigned binary form of the endianness of the machine generating the
 * file. Returns the number of bytes stored. If @buffer is %NULL, doesn't
 * store anything.
 */
static size_t store_gcov_u32(void *buffer, size_t off, u32 v)
{
	u32 *data;

	if (buffer) {
		data = buffer + off;
		*data = v;
	}

	return sizeof(*data);
}

/**
 * store_gcov_u64 - store 64 bit number in gcov format to buffer
 * @buffer: target buffer or NULL
 * @off: offset into the buffer
 * @v: value to be stored
 *
 * Number format defined by gcc: numbers are recorded in the 32 bit
 * unsigned binary form of the endianness of the machine generating the
 * file. 64 bit numbers are stored as two 32 bit numbers, the low part
 * first. Returns the number of bytes stored. If @buffer is %NULL, doesn't store
 * anything.
 */
static size_t store_gcov_u64(void *buffer, size_t off, u64 v)
{
	u32 *data;

	if (buffer) {
		data = buffer + off;

		data[0] = (v & 0xffffffffUL);
		data[1] = (v >> 32);
	}

	return sizeof(*data) * 2;
}

/**
 * convert_to_gcda - convert profiling data set to gcda file format
 * @buffer: the buffer to store file data or %NULL if no data should be stored
 * @info: profiling data set to be converted
 *
 * Returns the number of bytes that were/would have been stored into the buffer.
 */
static size_t convert_to_gcda(char *buffer, struct gcov_info *info)
{
	struct gcov_fn_info *fi_ptr;
	struct gcov_ctr_info *ci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;
	unsigned int cv_idx;
	size_t pos = 0;

	/* File header. */
	pos += store_gcov_u32(buffer, pos, GCOV_DATA_MAGIC);
	pos += store_gcov_u32(buffer, pos, info->version);
	pos += store_gcov_u32(buffer, pos, info->stamp);

	for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) {
		fi_ptr = info->functions[fi_idx];

		/* Function record. */
		pos += store_gcov_u32(buffer, pos, GCOV_TAG_FUNCTION);
		pos += store_gcov_u32(buffer, pos, GCOV_TAG_FUNCTION_LENGTH);
		pos += store_gcov_u32(buffer, pos, fi_ptr->ident);
		pos += store_gcov_u32(buffer, pos, fi_ptr->lineno_checksum);
		pos += store_gcov_u32(buffer, pos, fi_ptr->cfg_checksum);

		ci_ptr = fi_ptr->ctrs;

		for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
			if (!counter_active(info, ct_idx))
				continue;

			/* Counter record. */
			pos += store_gcov_u32(buffer, pos,
					      GCOV_TAG_FOR_COUNTER(ct_idx));
			pos += store_gcov_u32(buffer, pos, ci_ptr->num * 2);

			for (cv_idx = 0; cv_idx < ci_ptr->num; cv_idx++) {
				pos += store_gcov_u64(buffer, pos,
						      ci_ptr->values[cv_idx]);
			}

			ci_ptr++;
		}
	}

	return pos;
}

/**
 * gcov_iter_new - allocate and initialize profiling data iterator
 * @info: profiling data set to be iterated
 *
 * Return file iterator on success, %NULL otherwise.
 */
struct gcov_iterator *gcov_iter_new(struct gcov_info *info)
{
	struct gcov_iterator *iter;

	iter = kzalloc(sizeof(struct gcov_iterator), GFP_KERNEL);
	if (!iter)
		goto err_free;

	iter->info = info;
	/* Dry-run to get the actual buffer size. */
	iter->size = convert_to_gcda(NULL, info);
	iter->buffer = vmalloc(iter->size);
	if (!iter->buffer)
		goto err_free;

	convert_to_gcda(iter->buffer, info);

	return iter;

err_free:
	kfree(iter);
	return NULL;
}


/**
 * gcov_iter_get_info - return profiling data set for given file iterator
 * @iter: file iterator
 */
void gcov_iter_free(struct gcov_iterator *iter)
{
	vfree(iter->buffer);
	kfree(iter);
}

/**
 * gcov_iter_get_info - return profiling data set for given file iterator
 * @iter: file iterator
 */
struct gcov_info *gcov_iter_get_info(struct gcov_iterator *iter)
{
	return iter->info;
}

/**
 * gcov_iter_start - reset file iterator to starting position
 * @iter: file iterator
 */
void gcov_iter_start(struct gcov_iterator *iter)
{
	iter->pos = 0;
}

/**
 * gcov_iter_next - advance file iterator to next logical record
 * @iter: file iterator
 *
 * Return zero if new position is valid, non-zero if iterator has reached end.
 */
int gcov_iter_next(struct gcov_iterator *iter)
{
	if (iter->pos < iter->size)
		iter->pos += ITER_STRIDE;

	if (iter->pos >= iter->size)
		return -EINVAL;

	return 0;
}

/**
 * gcov_iter_write - write data for current pos to seq_file
 * @iter: file iterator
 * @seq: seq_file handle
 *
 * Return zero on success, non-zero otherwise.
 */
int gcov_iter_write(struct gcov_iterator *iter, struct seq_file *seq)
{
	size_t len;

	if (iter->pos >= iter->size)
		return -EINVAL;

	len = ITER_STRIDE;
	if (iter->pos + len > iter->size)
		len = iter->size - iter->pos;

	seq_write(seq, iter->buffer + iter->pos, len);

	return 0;
}

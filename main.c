/* main.c - main function for markdown module testing */

/*
 * Copyright (c) 2009, Natacha Porté
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "markdown.h"

#include <stdio.h>

#define READ_UNIT 1024
#define OUTPUT_UNIT 64


/* buffer statistics, to track some memleaks */
extern long buffer_stat_nb;
extern size_t buffer_stat_alloc_bytes;



/* main • main function, interfacing STDIO with the parser */
int
main(void) {
	struct buf *ib, *ob;
	size_t ret;

	/* reading everything from stdin */
	ib = bufnew(READ_UNIT);
	bufgrow(ib, READ_UNIT);
	while ((ret = fread(ib->data + ib->size, 1,
			ib->asize - ib->size, stdin)) > 0) {
		ib->size += ret;
		bufgrow(ib, ib->size + READ_UNIT); }

	/* performing markdown parsing */
	ob = bufnew(OUTPUT_UNIT);
	markdown(ob, ib, &mkd_xhtml);

	/* writing the result to stdout */
	fwrite(ob->data, 1, ob->size, stdout);

	/* cleanup */
	bufrelease(ib);
	bufrelease(ob);

	/* memory checks */
	if (buffer_stat_nb)
		fprintf(stderr, "Warning: %ld buffers still active\n",
				buffer_stat_nb);
	if (buffer_stat_alloc_bytes)
		fprintf(stderr, "Warning: %zu bytes still allocated\n",
				buffer_stat_alloc_bytes);
	return 0; }

/* vim: set filetype=c: */

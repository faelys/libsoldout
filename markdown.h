/* markdown.h - generic markdown parser */

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

#ifndef LITHIUM_MARKDOWN_H
#define LITHIUM_MARKDOWN_H

#include "buffer.h"


/********************
 * TYPE DEFINITIONS *
 ********************/

/* mkd_renderer • functions for rendering parsed data */
struct mkd_renderer {
	void (*blockcode)(struct buf *ob, struct buf *text);
	void (*blockquote)(struct buf *ob, struct buf *text);
	void (*header)(struct buf *ob, struct buf *text, int level);
	void (*hrule)(struct buf *ob);
	void (*linebreak)(struct buf *ob);
	void (*list)(struct buf *ob, struct buf *text, int flags);
	void (*listitem)(struct buf *ob, struct buf *text, int flags);
	void (*paragraph)(struct buf *ob, struct buf *text); };



/************************
 * PREDEFINED RENDERERS *
 ************************/

extern struct mkd_renderer mkd_xhtml; /* XHTML 1.0 renderer */



/*********
 * FLAGS *
 *********/

#define MKD_LIST_ORDERED	1
#define MKD_LI_BLOCK		2  /* <li> containing block data */



/**********************
 * EXPORTED FUNCTIONS *
 **********************/

/* markdown • parses the input buffer and renders it into the output buffer */
void
markdown(struct buf *ob, struct buf *ib, struct mkd_renderer *rndr, int flags);


#endif /* ndef LITHIUM_MARKDOWN_H */

/* vim: set filetype=c: */

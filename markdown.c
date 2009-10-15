/* markdown.c - generic markdown parser */

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

#include "array.h"

#include <stdio.h> /* only used for debug output */
#include <string.h>
#include <strings.h> /* for strncasecmp */

#define TEXT_UNIT 64	/* unit for the copy of the input buffer */
#define WORK_UNIT 64	/* block-level working buffer */


/***************
 * LOCAL TYPES *
 ***************/

/* link_ref • reference to a link */
struct link_ref {
	struct buf *	id;
	struct buf *	link;
	struct buf *	title; };


/* char_trigger • function pointer to render active chars */
/*   returns the number of chars taken care of */
/*   data is the pointer of the beginning of the span */
/*   offset is the number of valid chars before data */
struct render;
typedef size_t
(*char_trigger)(struct buf *ob, struct render *rndr,
		char *data, size_t offset, size_t size);


/* render • structure containing one particular render */
struct render {
	struct mkd_renderer	make;
	struct array		refs;
	char_trigger		active_char[256]; };



/********************
 * GENERIC RENDERER *
 ********************/

static int
rndr_autolink(struct buf *ob, struct buf *link, enum mkd_autolink type,
						void *opaque) {
	if (!link || !link->size) return 0;
	BUFPUTSL(ob, "<a href=\"");
	if (type == MKDA_IMPLICIT_EMAIL) BUFPUTSL(ob, "mailto:");
	bufput(ob, link->data, link->size);
	BUFPUTSL(ob, "\">");
	if (type == MKDA_EXPLICIT_EMAIL && link->size > 7)
		bufput(ob, link->data + 7, link->size - 7);
	else	bufput(ob, link->data, link->size);
	BUFPUTSL(ob, "</a>");
	return 1; }

static void
rndr_blockcode(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<pre><code>");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</code></pre>\n"); }

static void
rndr_blockquote(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<blockquote>\n");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</blockquote>\n"); }

static int
rndr_codespan(struct buf *ob, struct buf *text, void *opaque) {
	BUFPUTSL(ob, "<code>");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</code>");
	return 1; }

static int
rndr_double_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "<strong>");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</strong>");
	return 1; }

static int
rndr_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "<em>");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</em>");
	return 1; }

static void
rndr_header(struct buf *ob, struct buf *text, int level, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	bufprintf(ob, "<h%d>", level);
	if (text) bufput(ob, text->data, text->size);
	bufprintf(ob, "</h%d>\n", level); }

static int
rndr_link(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *content, void *opaque) {
	BUFPUTSL(ob, "<a");
	if (link && link->size) {
		BUFPUTSL(ob, " href=\"");
		bufput(ob, link->data, link->size);
		bufputc(ob, '"'); }
	if (title && title->size) {
		BUFPUTSL(ob, " title=\"");
		bufput(ob, title->data, title->size);
		bufputc(ob, '"'); }
	bufputc(ob, '>');
	if (content && content->size) bufput(ob, content->data, content->size);
	BUFPUTSL(ob, "</a>");
	return 1; }

static void
rndr_list(struct buf *ob, struct buf *text, int flags, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	bufput(ob, flags & MKD_LIST_ORDERED ? "<ol>\n" : "<ul>\n", 5);
	if (text) bufput(ob, text->data, text->size);
	bufput(ob, flags & MKD_LIST_ORDERED ? "</ol>\n" : "</ul>\n", 6); }

static void
rndr_listitem(struct buf *ob, struct buf *text, int flags, void *opaque) {
	BUFPUTSL(ob, "<li>");
	if (text) {
		while (text->size && text->data[text->size - 1] == '\n')
			text->size -= 1;
		bufput(ob, text->data, text->size); }
	BUFPUTSL(ob, "</li>\n"); }

static void
rndr_paragraph(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<p>");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</p>\n"); }

static int
rndr_raw_inline(struct buf *ob, struct buf *text, void *opaque) {
	bufput(ob, text->data, text->size);
	return 1; }

static int
rndr_triple_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "<strong><em>");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "</em></strong>");
	return 1; }



/**********************
 * XHTML 1.0 RENDERER *
 **********************/

static void
xhtml_hrule(struct buf *ob, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, "<hr />\n"); }

static int
xhtml_image(struct buf *ob, struct buf *link, struct buf *title,
			struct buf *alt, void *opaque) {
	if (!link || !link->size) return 0;
	BUFPUTSL(ob, "<img src=\"");
	bufput(ob, link->data, link->size);
	BUFPUTSL(ob, "\" alt=\"");
	if (alt && alt->size)
		bufput(ob, alt->data, alt->size);
	if (title && title->size) {
		BUFPUTSL(ob, "\" title=\"");
		bufput(ob, title->data, title->size); }
	BUFPUTSL(ob, "\" />");
	return 1; }

static int
xhtml_linebreak(struct buf *ob, void *opaque) {
	BUFPUTSL(ob, "<br />\n");
	return 1; }


/* exported renderer structure */
const struct mkd_renderer mkd_xhtml = {
	rndr_blockcode,
	rndr_blockquote,
	rndr_header,
	xhtml_hrule,
	rndr_list,
	rndr_listitem,
	rndr_paragraph,

	rndr_autolink,
	rndr_codespan,
	rndr_double_emphasis,
	rndr_emphasis,
	xhtml_image,
	xhtml_linebreak,
	rndr_link,
	rndr_raw_inline,
	rndr_triple_emphasis,

	"*_",
	NULL };



/***************************
 * STATIC HELPER FUNCTIONS *
 ***************************/

/* attr_escape • copy data into a buffer, escaping '"', '<' '&' and '>' */
static void
attr_escape(struct buf *ob, char *data, size_t size) {
	size_t i;
	for (i = 0; i < size; i += 1)
		if (data[i] == '&') BUFPUTSL(ob, "&amp;");
		else if (data[i] == '<') BUFPUTSL(ob, "&lt;");
		else if (data[i] == '>') BUFPUTSL(ob, "&gt;");
		else if (data[i] == '"') BUFPUTSL(ob, "&quot;");
		else bufputc(ob, data[i]); }


/* html_escape • copy data into a buffer, escaping '<' '&' and '>' */
static void
html_escape(struct buf *ob, char *data, size_t size) {
	size_t i;
	for (i = 0; i < size; i += 1)
		if (data[i] == '&') BUFPUTSL(ob, "&amp;");
		else if (data[i] == '<') BUFPUTSL(ob, "&lt;");
		else if (data[i] == '>') BUFPUTSL(ob, "&gt;");
		else bufputc(ob, data[i]); }


/* cmp_link_ref • comparison function for link_ref sorted arrays */
static int
cmp_link_ref(void *array_entry, void *key) {
	struct link_ref *lr = array_entry;
	return bufcasecmp(lr->id, key); }



/****************************
 * INLINE PARSING FUNCTIONS *
 ****************************/

/* is_mail_autolink • looks for the address part of a mail autolink and '>' */
/* this is less strict than the original markdown e-mail address matching */
static size_t
is_mail_autolink(char *data, size_t size) {
	size_t i = 0, nb = 0;
	/* address is assumed to be: [-@._a-zA-Z0-9]+ with exactly one '@' */
	while (i < size && (data[i] == '-' || data[i] == '.'
	|| data[i] == '_' || data[i] == '@'
	|| (data[i] >= 'a' && data[i] <= 'z')
	|| (data[i] >= 'A' && data[i] <= 'Z')
	|| (data[i] >= '0' && data[i] <= '9'))) {
		if (data[i] == '@') nb += 1;
		i += 1; }
	if (i >= size || data[i] != '>' || nb != 1) return 0;
	return i + 1; }


/* tag_length • returns the length of the given tag, or 0 is it's not valid */
static size_t
tag_length(char *data, size_t size, enum mkd_autolink *autolink) {
	size_t i, j;

	/* a valid tag can't be shorter than 3 chars */
	if (size < 3) return 0;

	/* begins with a '<' optionally followed by '/', followed by letter */
	if (data[0] != '<') return 0;
	i = (data[1] == '/') ? 2 : 1;
	if ((data[i] < 'a' || data[i] > 'z')
	&&  (data[i] < 'A' || data[i] > 'Z')) return 0;

	/* scheme test */
	*autolink = MKDA_NOT_AUTOLINK;
	if (size > 6 && strncasecmp(data + 1, "http", 4) == 0 && (data[5] == ':'
	|| ((data[5] == 's' || data[5] == 'S') && data[6] == ':'))) {
		i = data[5] == ':' ? 6 : 7;
		*autolink = MKDA_NORMAL; }
	else if (size > 5 && strncasecmp(data + 1, "ftp:", 4) == 0) {
		i = 5;
		*autolink = MKDA_NORMAL; }
	else if (size > 7 && strncasecmp(data + 1, "mailto:", 7) == 0) {
		i = 8;
		/* not changing *autolink to go to the address test */ }

	/* completing autolink test: no whitespace or ' or " */
	if (i >= size || i == '>')
		*autolink = MKDA_NOT_AUTOLINK;
	else if (*autolink) {
		j = i;
		while (i < size && data[i] != '>' && data[i] != '\''
		&& data[i] != '"' && data[i] != ' ' && data[i] != '\t'
		&& data[i] != '\t')
			i += 1;
		if (i >= size) return 0;
		if (i > j && data[i] == '>') return i + 1;
		/* one of the forbidden chars has been found */
		*autolink = MKDA_NOT_AUTOLINK; }
	else if ((j = is_mail_autolink(data + i, size - i)) != 0) {
		*autolink = (i == 8)
				? MKDA_EXPLICIT_EMAIL : MKDA_IMPLICIT_EMAIL;
		return i + j; }

	/* looking for sometinhg looking like a tag end */
	while (i < size && data[i] != '>') i += 1;
	if (i >= size) return 0;
	return i + 1; }


/* parse_inline • parses inline markdown elements */
static void
parse_inline(struct buf *ob, struct render *rndr, char *data, size_t size) {
	size_t i = 0, end;
	char_trigger action;

	while (i < size) {
		/* copying inactive chars into the output */
		end = i;
		while (end < size
		&& (action = rndr->active_char[(unsigned char)data[end]]) == 0)
			end += 1;
		bufput(ob, data + i, end - i);
		if (end >= size) break;
		i = end;

		/* calling the trigger */
		end = action(ob, rndr, data + i, i, size - i);
		if (!end) { /* no action from the callback */
			bufputc(ob, data[i]);
			i += 1; }
		else i += end; } }


/* find_emph_char • looks for the next emph char, skipping other constructs */
static size_t
find_emph_char(char *data, size_t size, char c) {
	size_t i = 1;

	while (i < size) {
		while (i < size && data[i] != c
		&& data[i] != '`' && data[i] != '[')
			i += 1;
		if (data[i] == c) return i;

		/* not counting escaped chars */
		if (i && data[i - 1] == '\\') { i += 1; continue; }

		/* skipping a code span */
		if (data[i] == '`') {
			size_t tmp_i = 0;
			i += 1;
			while (i < size && data[i] != '`') {
				if (!tmp_i && data[i] == c) tmp_i = i;
				i += 1; }
			if (i >= size) return tmp_i;
			i += 1; }

		/* skipping a link */
		else if (data[i] == '[') {
			size_t tmp_i = 0;
			char cc;
			i += 1;
			while (i < size && data[i] != ']') {
				if (!tmp_i && data[i] == c) tmp_i = i;
				i += 1; }
			i += 1;
			while (i < size && (data[i] == ' '
			|| data[i] == '\t' || data[i] == '\n'))
				i += 1;
			if (i >= size) return tmp_i;
			if (data[i] != '[' && data[i] != '(') { /* not a link*/
				if (tmp_i) return tmp_i;
				else continue; }
			cc = data[i];
			i += 1;
			while (i < size && data[i] != cc) {
				if (!tmp_i && data[i] == c) tmp_i = i;
				i += 1; }
			if (i >= size) return tmp_i;
			i += 1; } }
	return 0; }


/* parse_emph1 • parsing single emphase */
/* closed by a symbol not preceded by whitespace and not followed by symbol */
static size_t
parse_emph1(struct buf *ob, struct render *rndr,
			char *data, size_t size, char c) {
	size_t i = 0, len;
	struct buf *work;

	if (!rndr->make.emphasis) return 0;

	/* skipping one symbol if coming from emph3 */
	if (size > 1 && data[0] == c && data[1] == c) i = 1;

	while (i < size) {
		len = find_emph_char(data + i, size - i, c);
		if (!len) return 0;
		i += len;
		if (i >= size) return 0;

		if (i + 1 < size && data[i + 1] == c) {
			i += 1;
			continue; }
		if (data[i] == c && data[i - 1] != ' '
		&& data[i - 1] != '\t' && data[i - 1] != '\n') {
			work = bufnew(WORK_UNIT);
			parse_inline(work, rndr, data, i);
			rndr->make.emphasis(ob, work, c, rndr->make.opaque);
			bufrelease(work);
			return i + 1; } }
	return 0; }


/* parse_emph2 • parsing single emphase */
static size_t
parse_emph2(struct buf *ob, struct render *rndr,
			char *data, size_t size, char c) {
	size_t i = 0, len;
	struct buf *work;

	if (!rndr->make.double_emphasis) return 0;
	
	while (i < size) {
		len = find_emph_char(data + i, size - i, c);
		if (!len) return 0;
		i += len;
		if (i + 1 < size && data[i] == c && data[i + 1] == c
		&& i && data[i - 1] != ' '
		&& data[i - 1] != '\t' && data[i - 1] != '\n') {
			work = bufnew(WORK_UNIT);
			parse_inline(work, rndr, data, i);
			rndr->make.double_emphasis(ob, work, c,
				rndr->make.opaque);
			bufrelease(work);
			return i + 2; }
		i += 1; }
	return 0; }


/* parse_emph3 • parsing single emphase */
/* finds the first closing tag, and delegates to the other emph */
static size_t
parse_emph3(struct buf *ob, struct render *rndr,
			char *data, size_t size, char c) {
	size_t i = 0, len;

	while (i < size) {
		len = find_emph_char(data + i, size - i, c);
		if (!len) return 0;
		i += len;

		/* skip whitespace preceded symbols */
		if (data[i] != c || data[i - 1] == ' '
		|| data[i - 1] == '\t' || data[i - 1] == '\n')
			continue;

		if (i + 2 < size && data[i + 1] == c && data[i + 2] == c
		&& rndr->make.triple_emphasis) {
			/* triple symbol found */
			struct buf *work = bufnew(WORK_UNIT);
			parse_inline(work, rndr, data, i);
			rndr->make.triple_emphasis(ob, work, c,
							rndr->make.opaque);
			bufrelease(work);
			return i + 3; }
		else if (i + 1 < size && data[i + 1] == c) {
			/* double symbol found, handing over to emph1 */
			len = parse_emph1(ob, rndr, data - 2, size + 2, c);
			if (!len) return 0;
			else return len - 2; }
		else {
			/* single symbol found, handing over to emph2 */
			len = parse_emph2(ob, rndr, data - 1, size + 1, c);
			if (!len) return 0;
			else return len - 1; } }
	return 0; }


/* char_emphasis • single and double emphasis parsing */
static size_t
char_emphasis(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	char c = data[0];
	size_t ret;
	if (size > 2 && data[1] != c) {
		/* whitespace cannot follow an opening emphasis */
		if (data[1] == ' ' || data[1] == '\t' || data[1] == '\n'
		|| (ret = parse_emph1(ob, rndr, data + 1, size - 1, c)) == 0)
			return 0;
		return ret + 1; }
	if (size > 3 && data[1] == c && data[2] != c) {
		if (data[2] == ' ' || data[2] == '\t' || data[2] == '\n'
		|| (ret = parse_emph2(ob, rndr, data + 2, size - 2, c)) == 0)
			return 0;
		return ret + 2; }
	if (size > 4 && data[1] == c && data[2] == c && data[3] != c) {
		if (data[3] == ' ' || data[3] == '\t' || data[3] == '\n'
		|| (ret = parse_emph3(ob, rndr, data + 3, size - 3, c)) == 0)
			return 0;
		return ret + 3; }
	return 0; }


/* char_linebreak • '\n' preceded by two spaces (assuming linebreak != 0) */
static size_t
char_linebreak(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	if (offset < 2 || data[-1] != ' ' || data[-2] != 2) return 0;
	/* removing the last space from ob and rendering */
	if (ob->size && ob->data[ob->size - 1] == ' ') ob->size -= 1;
	rndr->make.linebreak(ob, rndr->make.opaque);
	return 1; }


/* char_codespan • '`' parsing a code span (assuming codespan != 0) */
static size_t
char_codespan(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	size_t end, nb = 0, i, f_begin, f_end;
	struct buf *work = 0;

	/* counting the number of backticks in the delimiter */
	while (nb < size && data[nb] == '`') nb += 1;

	/* finding the next delimiter */
	i = 0;
	for (end = nb; end < size && i < nb; end += 1)
		if (data[end] == '`') i += 1;
		else i = 0;
	if (i < nb && end >= size) return 0; /* no matching delimiter */

	/* trimming outside whitespaces */
	f_begin = nb;
	while (f_begin < end && (data[f_begin] == ' ' || data[f_begin] == '\t'))
		f_begin += 1;
	f_end = end - nb;
	while (f_end > nb && (data[f_end-1] == ' ' || data[f_end-1] == '\t'))
		f_end -= 1;

	/* real code span */
	if (f_begin < f_end) {
		work = bufnew(WORK_UNIT);
		html_escape(work, data + f_begin, f_end - f_begin); }
	rndr->make.codespan(ob, work, rndr->make.opaque);
	bufrelease(work);
	return end; }


/* char_escape • '\\' backslash escape */
static size_t
char_escape(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	if (size > 1) html_escape(ob, data + 1, 1);
	return 2; }


/* char_entity • '&' escaped when it doesn't belong to an entity */
/* valid entities are assumed to be anything mathing &#?[A-Za-z0-9]+; */
static size_t
char_entity(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	size_t end = 1;
	if (end < size && data[end] == '#') end += 1;
	while (end < size
	&& ((data[end] >= '0' && data[end] <= '9')
	||  (data[end] >= 'a' && data[end] <= 'z')
	||  (data[end] >= 'A' && data[end] <= 'Z')))
		end += 1;
	/* an '&' will always be put */
	bufputc(ob, '&');
	/* adding the "amp;" part if needed */
	if (end >= size || data[end] != ';')
		BUFPUTSL(ob, "amp;");
	return 1; }


/* char_langle_esc • '<' always escaped (no tag processing) */
static size_t
char_langle_esc(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	BUFPUTSL(ob, "&lt;");
	return 1; }


/* char_langle_tag • '<' when tags or autolinks are allowed */
static size_t
char_langle_tag(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	enum mkd_autolink altype = MKDA_NOT_AUTOLINK;
	size_t end = tag_length(data, size, &altype);
	struct buf work = { data, end, 0, 0, 0 };
	if (end) {
		if (rndr->make.autolink && altype != MKDA_NOT_AUTOLINK) {
			struct buf *wk = bufnew(WORK_UNIT);
			attr_escape(wk, data + 1, end - 2);
			rndr->make.autolink(ob, wk, altype, rndr->make.opaque);
			bufrelease(wk); }
		else if (rndr->make.raw_html_tag)
			rndr->make.raw_html_tag(ob, &work, rndr->make.opaque);
		return end; }
	else {
		BUFPUTSL(ob, "&lt;");
		return 1; } }


/* char_rangle • '>': always escaped when encountered outside of a tag */
static size_t
char_rangle(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	BUFPUTSL(ob, "&gt;");
	return 1; }


/* char_link • '[': parsing a link or an image */
static size_t
char_link(struct buf *ob, struct render *rndr,
				char *data, size_t offset, size_t size) {
	int is_img = (offset && data[-1] == '!');
	size_t i = 1, txt_e, link_b = 0, link_e = 0, title_b = 0, title_e = 0;
	struct buf *content = 0;
	struct buf *link = 0;
	struct buf *title = 0;

	/* checking whether the correct renderer exists */
	if ((is_img && !rndr->make.image) || (!is_img && !rndr->make.link))
		return 0;

	/* looking for the end of the first part: [^\]\] */
	while (i < size && (data[i] != ']' || data[i - 1] == '\\')) i += 1;
	if (i >= size) return 0;
	txt_e = i;
	i += 1;

	/* skip any amount of whitespace or newline */
	/* (this is much more laxist than original markdown syntax) */
	while (i < size
	&& (data[i] == ' ' || data[i] == '\t' || data[i] == '\n'))
		i += 1;
	if (i >= size) return 0;

	/* inline style link */
	if (data[i] == '(') {
		/* skipping initial whitespace */
		i += 1;
		while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
		link_b = i;

		/* looking for link end: ' " ) */
		while (i < size
		&& data[i] != '\'' && data[i] != '"' && data[i] != ')')
			i += 1;
		if (i >= size) return 0;
		link_e = i;

		/* looking for title end if present */
		if (data[i] == '\'' || data[i] == '"') {
			i += 1;
			title_b = i;
			while (i < size
			&& data[i] != '\'' && data[i] != '"' && data[i] != ')')
				i += 1;
			if (i >= size) return 0;
			if (data[i] == ')') {
				title_b = 0;
				link_e = i; }
			else { /* allow only whitespace after the title */
				title_e = i;
				i += 1;
				while (i < size
				&& (data[i] == ' ' || data[i] == '\t'))
					i += 1;
				if (i >= size || data[i] != ')')
					return 0; } }

		/* remove whitespace at the end of the link */
		while (link_e > link_b
		&& (data[link_e - 1] == ' ' || data[link_e - 1] == '\t'))
			link_e -= 1;

		/* building escaped link and title */
		if (link_e > link_b) {
			link = bufnew(WORK_UNIT);
			attr_escape(link, data + link_b, link_e - link_b); }
		if (title_e > title_b) {
			title = bufnew(WORK_UNIT);
			attr_escape(title, data + title_b, title_e - title_b);}

		i += 1; }

	/* reference style link */
	else if (data[i] == '[') {
		struct buf id = { 0, 0, 0, 0, 0 };
		struct link_ref *lr;

		/* looking for the id */
		i += 1;
		link_b = i;
		while (i < size && data[i] != ']') i += 1;
		if (i >= size) return 0;
		link_e = i;

		/* finding the link_ref */
		if (link_b == link_e) {
			id.data = data + 1;
			id.size = txt_e - 1; }
		else {
			id.data = data + link_b;
			id.size = link_e - link_b; }
		lr = arr_sorted_find(&rndr->refs, &id, cmp_link_ref);
		if (!lr) return 0;

		/* keeping link and title from link_ref */
		bufset(&link, lr->link);
		bufset(&title, lr->title);
		i += 1; }


	/* invalid link */
	else return 0;

	/* building content: img alt is escaped, link content is parsed */
	if (txt_e > 1) {
		content = bufnew(WORK_UNIT);
		if (is_img) attr_escape(content, data + 1, txt_e - 1);
		else parse_inline(content, rndr, data + 1, txt_e - 1); }

	/* calling the relevant rendering function */
	if (is_img) {
		if (ob->size && ob->data[ob->size - 1] == '!') ob->size -= 1;
		rndr->make.image(ob, link, title, content, rndr->make.opaque); }
	else rndr->make.link(ob, link, title, content, rndr->make.opaque);

	/* cleanup */
	bufrelease(link);
	bufrelease(title);
	bufrelease(content);
	return i; }



/*********************************
 * BLOCK-LEVEL PARSING FUNCTIONS *
 *********************************/

/* is_empty • returns whether the line is blank */
static int
is_empty(char *data, size_t size) {
	size_t i;
	for (i = 0; i < size && data[i] != '\n'; i += 1)
		if (data[i] != ' ' && data[i] != '\t') return 0;
	return 1; }


/* is_hrule • returns whether a line is a horizontal rule */
static int
is_hrule(char *data, size_t size) {
	size_t i = 0, n = 0;
	char c;

	/* skipping initial spaces */
	if (size < 3) return 0;
	if (data[0] == ' ') { i += 1;
	if (data[1] == ' ') { i += 1;
	if (data[2] == ' ') { i += 1; } } }

	/* looking at the hrule char */
	if (i + 2 >= size
	|| (data[i] != '*' && data[i] != '-' && data[i] != '_'))
		return 0;
	c = data[i];

	/* the whole line must be the char or whitespace */
	while (i < size && data[i] != '\n') {
		if (data[i] == c) n += 1;
		else if (data[i] != ' ' && data[i] != '\t')
			return 0;
		i += 1; }

	return n >= 3; }


/* is_headerline • returns whether the line is a setext-style hdr underline */
static int
is_headerline(char *data, size_t size) {
	size_t i = 0;

	/* test of level 1 header */
	if (data[i] == '=') {
		for (i = 1; i < size && data[i] == '='; i += 1);
		while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
		return (i >= size || data[i] == '\n') ? 1 : 0; }

	/* test of level 2 header */
	if (data[i] == '-') {
		for (i = 1; i < size && data[i] == '-'; i += 1);
		while (i < size && (data[i] == ' ' || data[i] == '\t')) i += 1;
		return (i >= size || data[i] == '\n') ? 2 : 0; }

	return 0; }


/* prefix_quote • returns blockquote prefix length */
static size_t
prefix_quote(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == '>') {
		if (i + 1 < size && (data[i + 1] == ' ' || data[i+1] == '\t'))
			return i + 2;
		else return i + 1; }
	else return 0; }


/* prefix_code • returns prefix length for block code*/
static size_t
prefix_code(char *data, size_t size) {
	if (size > 0 && data[0] == '\t') return 1;
	if (size > 3 && data[0] == ' ' && data[1] == ' '
			&& data[2] == ' ' && data[3] == ' ') return 4;
	return 0; }

/* prefix_li • returns blank prefix length inside list items */
static size_t
prefix_li(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == '\t') return 1;
	if (i < size && data[i] == ' ') { i += 1;
	if (i < size && data[i] == ' ') { i += 1;
	if (i < size && data[i] == ' ') { i += 1;
	if (i < size && data[i] == ' ') { i += 1; } } } }
	return i; }

/* prefix_oli • returns ordered list item prefix */
static size_t
prefix_oli(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i >= size || data[i] < '0' || data[i] > '9') return 0;
	while (i < size && data[i] >= '0' && data[i] <= '9') i += 1;
	if (i + 1 >= size || data[i] != '.'
	|| (data[i + 1] != ' ' && data[i + 1] != '\t')) return 0;
	return i + 1; }


/* prefix_uli • returns ordered list item prefix */
static size_t
prefix_uli(char *data, size_t size) {
	size_t i = 0;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i < size && data[i] == ' ') i += 1;
	if (i + 1 >= size
	|| (data[i] != '*' && data[i] != '+' && data[i] != '-')
	|| (data[i + 1] != ' ' && data[i + 1] != '\t'))
		return 0;
	return i + 1; }


/* parse_block • parsing of one block, returning next char to parse */
static void parse_block(struct buf *ob, struct render *rndr,
			char *data, size_t size);


/* parse_blockquote • hanldes parsing of a blockquote fragment */
static size_t
parse_blockquote(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t beg, end = 0, pre, work_size = 0;
	char *work_data = 0;
	struct buf *out = bufnew(WORK_UNIT);

	beg = 0;
	while (beg < size) {
		for (end = beg + 1; end < size && data[end - 1] != '\n';
							end += 1);
		pre = prefix_quote(data + beg, end - beg);
		if (pre) beg += pre; /* skipping prefix */
		else if (is_empty(data + beg, end - beg)
		&& (end >= size || (prefix_quote(data + end, size - end) == 0
					&& !is_empty(data + end, size - end))))
			/* empty line followed by non-quote line */
			break;
		if (beg < end) { /* copy into the in-place working buffer */
			/* bufput(work, data + beg, end - beg); */
			if (!work_data)
				work_data = data + beg;
			else if (data + beg != work_data + work_size)
				memmove(work_data + work_size, data + beg,
						end - beg);
			work_size += end - beg; }
		beg = end; }

	parse_block(out, rndr, work_data, work_size);
	rndr->make.blockquote(ob, out, rndr->make.opaque);
	bufrelease(out);
	return end; }


/* parse_blockquote • hanldes parsing of a regular paragraph */
static size_t
parse_paragraph(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t i = 0, end = 0;
	int level = 0;
	struct buf work = { data, 0, 0, 0, 0 }; /* volatile working buffer */

	while (i < size) {
		for (end = i + 1; end < size && data[end - 1] != '\n';
								end += 1);
		if (is_empty(data + i, size - i)
		|| (level = is_headerline(data + i, size - i)) != 0)
			break;
		if (data[i] == '#'
		|| is_hrule(data + i, size - i)) {
			end = i;
			break; }
		i = end; }

	work.size = i;
	while (work.size && data[work.size - 1] == '\n')
		work.size -= 1;
	if (!level) {
		struct buf *tmp = bufnew(WORK_UNIT);
		parse_inline(tmp, rndr, work.data, work.size);
		rndr->make.paragraph(ob, tmp, rndr->make.opaque);
		bufrelease(tmp); }
	else {
		if (work.size) {
			size_t beg;
			i = work.size;
			work.size -= 1;
			while (work.size && data[work.size] != '\n')
				work.size -= 1;
			beg = work.size + 1;
			while (work.size && data[work.size - 1] == '\n')
				work.size -= 1;
			if (work.size) {
				struct buf *tmp = bufnew(WORK_UNIT);
				parse_inline(tmp, rndr, work.data, work.size);
				rndr->make.paragraph(ob, tmp, rndr->make.opaque);
				bufrelease(tmp);
				work.data += beg;
				work.size = i - beg; }
			else work.size = i; }
		rndr->make.header(ob, &work, level, rndr->make.opaque); }
	return end; }


/* parse_blockquote • hanldes parsing of a block-level code fragment */
static size_t
parse_blockcode(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t beg, end, pre;
	struct buf *work = bufnew(WORK_UNIT);

	beg = 0;
	while (beg < size) {
		for (end = beg + 1; end < size && data[end - 1] != '\n';
							end += 1);
		pre = prefix_code(data + beg, end - beg);
		if (pre) beg += pre; /* skipping prefix */
		else if (!is_empty(data + beg, end - beg))
			/* non-empty non-prefixed line breaks the pre */
			break;
		if (beg < end)
			/* verbatim copy to the working buffer,
				escaping entities */
			html_escape(work, data + beg, end - beg);
		beg = end; }

	while (work->size && work->data[work->size - 1] == '\n')
		work->size -= 1;
	bufputc(work, '\n');
	rndr->make.blockcode(ob, work, rndr->make.opaque);
	bufrelease(work);
	return beg; }


/* parse_listitem • parsing of a single list item */
/*	assuming initial prefix is already removed */
static size_t
parse_listitem(struct buf *ob, struct render *rndr,
			char *data, size_t size, int *flags) {
	struct buf *work = bufnew(WORK_UNIT);
	size_t beg = 0, end, pre;

	while (beg < size) {
		for (end = beg + 1; end < size && data[end - 1] != '\n';
							end += 1);
		if (is_empty(data + beg, end - beg)) {
			if (end < size
			&& !is_empty(data + end, size - end)) {
				if (prefix_oli(data + end, size - end)
				||  prefix_uli(data + end, size - end))
					*flags |= MKD_LI_BLOCK;
				if (!prefix_li(data + end, size - end)) {
					beg = end;
					break; }
				else *flags |= MKD_LI_BLOCK; } }
		pre = prefix_li(data + beg, end - beg);
		if (pre) beg += pre;
		else if (prefix_oli(data + beg, end - beg)
		||  prefix_uli(data + beg, end - beg))
			break;
		if (beg < end)
			bufput(work, data + beg, end - beg);
		beg = end; }

	if (*flags & MKD_LI_BLOCK) {
		struct buf *wk2 = bufnew(WORK_UNIT);
		parse_block(wk2, rndr, work->data, work->size);
		bufrelease(work);
		work = wk2; }
	rndr->make.listitem(ob, work, *flags, rndr->make.opaque);
	bufrelease(work);
	return beg; }


/* parse_list • parsing ordered or unordered list block */
static size_t
parse_list(struct buf *ob, struct render *rndr,
			char *data, size_t size, int flags) {
	struct buf *work = bufnew(WORK_UNIT);
	size_t i = 0, pre;

	while (i < size) {
		pre = prefix_oli(data + i, size - i);
		if (!pre) pre = prefix_uli(data + i, size - i);
		if (!pre) break;
		i += pre;
		i += parse_listitem(work, rndr, data + i, size - i, &flags); }

	rndr->make.list(ob, work, flags, rndr->make.opaque);
	bufrelease(work);
	return i; }


/* parse_atxheader • parsing of atx-style headers */
static size_t
parse_atxheader(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	int level = 0;
	size_t i, end, skip;
	struct buf work = { data, 0, 0, 0, 0 };

	if (!size || data[0] != '#') return 0;
	while (level < size && level < 6 && data[level] == '#') level += 1;
	for (i = level; i < size && (data[i] == ' ' || data[i] == '\t');
							i += 1);
	work.data = data + i;
	for (end = i; end < size && data[end] != '\n'; end += 1);
	skip = end;
	while (end && data[end - 1] == '#') end -= 1;
	while (end && (data[end - 1] == ' ' || data[end - 1] == '\t')) end -= 1;
	work.size = end - i;
	rndr->make.header(ob, &work, level, rndr->make.opaque);
	return skip; }



/* parse_block • parsing of one block, returning next char to parse */
static void
parse_block(struct buf *ob, struct render *rndr,
			char *data, size_t size) {
	size_t beg, end;
	char *txt_data;
	beg = 0;
	while (beg < size) {
		txt_data = data + beg;
		end = size - beg;
		if (data[beg] == '#')
			beg += parse_atxheader(ob, rndr, txt_data, end);
		else if (is_empty(txt_data, end)) {
			while (beg < size && data[beg] != '\n') beg += 1;
			beg += 1; }
		else if (is_hrule(txt_data, end)) {
			rndr->make.hrule(ob, rndr->make.opaque);
			while (beg < size && data[beg] != '\n') beg += 1;
			beg += 1; }
		else if (prefix_quote(txt_data, end))
			beg += parse_blockquote(ob, rndr, txt_data, end);
		else if (prefix_code(txt_data, end))
			beg += parse_blockcode(ob, rndr, txt_data, end);
		else if (prefix_uli(txt_data, end))
			beg += parse_list(ob, rndr, txt_data, end, 0);
		else if (prefix_oli(txt_data, end))
			beg += parse_list(ob, rndr, txt_data, end,
						MKD_LIST_ORDERED);
		else
			beg += parse_paragraph(ob, rndr, txt_data, end); } }



/*********************
 * REFERENCE PARSING *
 *********************/

/* is_ref • returns whether a line is a reference or not */
static int
is_ref(char *data, size_t beg, size_t end, size_t *last, struct array *refs) {
	int n;
	size_t i = beg;
	size_t id_offset, id_end;
	size_t link_offset, link_end;
	size_t title_offset, title_end;
	size_t line_end;
	struct link_ref *lr;
	struct buf id = { 0, 0, 0, 0, 0 }; /* volatile buf for id search */

	/* up to 3 optional leading spaces */
	while (i < beg + 3 && i < end && data[i] == ' ') i += 1;
	if (i >= end || i >= beg + 3) return 0;

	/* id part: anything but a newline between brackets */
	if (data[i] != '[') return 0;
	i += 1;
	id_offset = i;
	while (i < end && data[i] != '\n' && data[i] != '\r' && data[i] != ']')
		i += 1;
	if (i >= end || data[i] != ']') return 0;
	id_end = i;

	/* spacer: colon (space | tab)* newline? (space | tab)* */
	i += 1;
	if (i >= end || data[i] != ':') return 0;
	i += 1;
	while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1;
	if (i < end && (data[i] == '\n' || data[i] == '\r')) {
		i += 1;
		if (i < end && data[i] == '\r' && data[i - 1] == '\n') i += 1; }
	while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1;
	if (i >= end) return 0;

	/* link: whitespace-free sequence, optionally between angle brackets */
	if (data[i] == '<') i += 1;
	link_offset = i;
	while (i < end && data[i] != ' ' && data[i] != '\t'
			&& data[i] != '\n' && data[i] != '\r') i += 1;
	if (data[i - 1] == '>') link_end = i - 1;
	else link_end = i;

	/* optional spacer: (space | tab)* (newline | '\'' | '"' | '(' ) */
	while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1;
	if (i < end && data[i] != '\n' && data[i] != '\r'
			&& data[i] != '\'' && data[i] != '"' && data[i] != '(')
		return 0;
	line_end = 0;
	/* computing end-of-line */
	if (i >= end || data[i] == '\r' || data[i] == '\n') line_end = i;
	if (i + 1 < end && data[i] == '\n' && data[i + 1] == '\r')
		line_end = i + 1;

	/* optional (space|tab)* spacer after a newline */
	if (line_end) {
		i = line_end + 1;
		while (i < end && (data[i] == ' ' || data[i] == '\t')) i += 1; }

	/* optional title: any non-newline sequence enclosed in '"()
					alone on its line */
	title_offset = title_end = 0;
	if (i + 1 < end
	&& (data[i] == '\'' || data[i] == '"' || data[i] == '(')) {
		i += 1;
		title_offset = i;
		/* looking for EOL */
		while (i < end && data[i] != '\n' && data[i] != '\r') i += 1;
		if (i + 1 < end && data[i] == '\n' && data[i + 1] == '\r')
			title_end = i + 1;
		else	title_end = i;
		/* stepping back */
		i -= 1;
		while (i > title_offset && (data[i] == ' ' || data[i] == '\t'))
			i -= 1;
		if (i > title_offset
		&& (data[i] == '\'' || data[i] == '"' || data[i] == ')')) {
			line_end = title_end;
			title_end = i; } }
	if (!line_end) return 0; /* garbage after the link */

	/* a valid ref has been found, filling-in return structures */
	if (last) *last = line_end;
	if (!refs) return 1;
	id.data = data + id_offset;
	id.size = id_end - id_offset;
	n = arr_sorted_find_i(refs, &id, cmp_link_ref);
	if (arr_insert(refs, 1, n) && (lr = arr_item(refs, n)) != 0) {
		struct buf *work = bufnew(WORK_UNIT);
		lr->id = bufnew(id_end - id_offset);
		bufput(lr->id, data + id_offset, id_end - id_offset);
		attr_escape(work, data + link_offset, link_end - link_offset);
		lr->link = bufdup(work, 1);
		if (title_end > title_offset) {
			work->size = 0;
			attr_escape(work, data + title_offset,
						title_end - title_offset);
			lr->title = bufdup(work, 1); }
		else lr->title = 0;
		bufrelease(work); }
	return 1; }



/**********************
 * EXPORTED FUNCTIONS *
 **********************/

/* markdown • parses the input buffer and renders it into the output buffer */
void
markdown(struct buf *ob, struct buf *ib, const struct mkd_renderer *rndrer) {
	struct link_ref *lr;
	struct buf *text = bufnew(TEXT_UNIT);
	size_t i, beg, end;
	struct render rndr;

	/* filling the render structure */
	if (!rndrer) return;
	rndr.make = *rndrer;
	arr_init(&rndr.refs, sizeof (struct link_ref));
	for (i = 0; i < 256; i += 1) rndr.active_char[i] = 0;
	if ((rndr.make.emphasis || rndr.make.double_emphasis
						|| rndr.make.triple_emphasis)
	&& rndr.make.emph_chars)
		for (i = 0; rndr.make.emph_chars[i]; i += 1)
			rndr.active_char[(unsigned char)rndr.make.emph_chars[i]]
				= char_emphasis;
	if (rndr.make.codespan) rndr.active_char['`'] = char_codespan;
	if (rndr.make.linebreak) rndr.active_char['\n'] = char_linebreak;
	if (rndr.make.image || rndr.make.link)
		rndr.active_char['['] = char_link;
	rndr.active_char['<'] = (rndr.make.raw_html_tag || rndr.make.autolink)
				? char_langle_tag : char_langle_esc;
	rndr.active_char['>'] = char_rangle;
	rndr.active_char['&'] = char_entity;
	rndr.active_char['\\'] = char_escape;

	/* first pass: looking for references, copying everything else */
	beg = 0;
	while (beg < ib->size) /* iterating over lines */
		if (is_ref(ib->data, beg, ib->size, &end, &rndr.refs))
			beg = end;
		else { /* skipping to the next line */
			end = beg;
			while (end < ib->size
			&& ib->data[end] != '\n' && ib->data[end] != '\r')
				end += 1;
			/* adding the line body if present */
			if (end > beg) bufput(text, ib->data + beg, end - beg);
			while (end < ib->size
			&& (ib->data[end] == '\n' || ib->data[end] == '\r')) {
				/* add one \n per newline */
				if (ib->data[end] == '\n'
				|| (end + 1 < ib->size
						&& ib->data[end + 1] != '\n'))
					bufputc(text, '\n');
				end += 1; }
			beg = end; }

	/* adding a final newline if not already present */
	if (!text->size) return;
	if (text->data[text->size - 1] != '\n'
	&&  text->data[text->size - 1] != '\r')
		bufputc(text, '\n');

	/* second pass: actual rendering */
	parse_block(ob, &rndr, text->data, text->size);

#if 0
/* debug: printing the reference list */
BUFPUTSL(ob, "(refs");
lr = rndr.refs.base;
for (i = 0; i < rndr.refs.size; i += 1) {
	BUFPUTSL(ob, "\n\t(\"");
	bufput(ob, lr[i].id->data, lr[i].id->size);
	BUFPUTSL(ob, "\" \"");
	bufput(ob, lr[i].link->data, lr[i].link->size);
	if (lr[i].title) {
		BUFPUTSL(ob, "\" \"");
		bufput(ob, lr[i].title->data, lr[i].title->size); }
	BUFPUTSL(ob, "\")"); }
BUFPUTSL(ob, ")\n");
#endif

	/* clean-up */
	bufrelease(text);
	lr = rndr.refs.base;
	for (i = 0; i < rndr.refs.size; i += 1) {
		bufrelease(lr[i].id);
		bufrelease(lr[i].link);
		bufrelease(lr[i].title); } }

/* vim: set filetype=c: */

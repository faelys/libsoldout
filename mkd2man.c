/* mkd2man.c - man-page-formatted output from markdown text */

/*
 * Copyright (c) 2009-2015, Baptiste Daroussin and Natacha Porté
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

#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define READ_UNIT 1024
#define OUTPUT_UNIT 64


/****************************
 * MARKDOWN TO MAN RENDERER *
 ****************************/

/* usage • print the option list */

void
usage(FILE *out, const char *name) {
	fprintf(out, "Usage: %s [-h] [-d <date>] [-s <section> ] [ -t <title> ] [input-file]\n\n",
	    name);
	fprintf(out, "\t-d, --date\n"
	    "\t\tSet the date of the manpage (default: now),\n"
	    "\t-h, --help\n"
	    "\t\tDisplay this help text and exit without further processing\n"
	    "\t-s, --section\n"
	    "\t\tSet the section of the manpage (default: 1)\n"
	    "\t-t, --title\n"
	    "\t\tSet the title of the manpage (default: filename)\n"); }

struct metadata {
	char *title;
	char *date;
	int section;
};

static void
man_text_escape(struct buf *ob, char *src, size_t size) {
	size_t  i = 0, org;
	while (i < size) {
		/* copying directly unescaped characters */
		org = i;
		while (i < size && src[i] != '-')
			i += 1;
		if (i > org) bufput(ob, src + org, i - org);

		/* escaping */
		if (i >= size) break;
		else if (src[i] == '-') BUFPUTSL(ob, "\\-");
		i += 1; } }

static void
man_prolog(struct buf *ob, void *opaque) {
	struct metadata *m = (struct metadata *)opaque;
	bufprintf(ob,
		".\\\" Generated by mkd2man\n"
		".Dd %s\n"
		".Dt %s %d\n"
		".Os",
		m->date,
		m->title,
		m->section
		); }

static void
man_epilog(struct buf *ob, void *opaque) {
		BUFPUTSL(ob, "\n"); }

static void
man_blockcode(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, ".Bd -literal\n");
	if (text) man_text_escape(ob, text->data, text->size);
	BUFPUTSL(ob, ".Ed"); }

static void
man_blockquote(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, ".Eo\n");
	if (text) man_text_escape(ob, text->data, text->size);
	BUFPUTSL(ob, "\n.Ec"); }

static int
man_codespan(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, ".Bd -literal\n");
	if (text) man_text_escape(ob, text->data, text->size);
	BUFPUTSL(ob, ".Ed");
	return 1; }

static void
man_header(struct buf *ob, struct buf *text, int level, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	switch(level) {
		case 1:
			BUFPUTSL(ob,".Sh ");
			break;
		case 2:
			BUFPUTSL(ob, ".Ss ");
			break;
		case 3:
			BUFPUTSL(ob, ".Pp\n.Em ");
			break;
	}
	if (text) bufput(ob, text->data, text->size);
}

static int
man_double_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "\\fB");
	bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "\\fP");
	return 1; }

static int
man_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	if (!text || !text->size) return 0;
	BUFPUTSL(ob, "\\fI");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, "\\fP");
	return 1; }

static int
man_linebreak(struct buf *ob, void *opaque) {
	BUFPUTSL(ob, ".br");
	return 1; }

static void
man_paragraph(struct buf *ob, struct buf *text, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	BUFPUTSL(ob, ".Pp\n");
	if (text) bufput(ob, text->data, text->size); }

static void
man_list(struct buf *ob, struct buf *text, int flags, void *opaque) {
	if (ob->size) bufputc(ob, '\n');
	if (flags & MKD_LIST_ORDERED)
		BUFPUTSL(ob,".Bl -enum\n");
	else
		BUFPUTSL(ob,".Bl -bullet\n");
	if (text) bufput(ob, text->data, text->size);
	BUFPUTSL(ob, ".El"); }

static void
man_listitem(struct buf *ob, struct buf *text, int flags, void *opaque) {
	BUFPUTSL(ob, ".It\n");
	if (text) {
		while (text->size && text->data[text->size - 1] == '\n')
			text->size -= 1;
		bufput(ob, text->data, text->size); }
	BUFPUTSL(ob, "\n"); }

static void
man_normal_text(struct buf *ob, struct buf *text, void *opaque) {
	if (text) man_text_escape(ob, text->data, text->size); }


/* renderer structure */
struct mkd_renderer to_man = {
	/* document-level callbacks */
	man_prolog,
	man_epilog,

	/* block-level callbacks */
	man_blockcode,
	man_blockquote,
	NULL,
	man_header,
	NULL,
	man_list,
	man_listitem,
	man_paragraph,
	NULL,
	NULL,
	NULL,

	/* span-level callbacks */
	NULL,
	man_codespan,
	man_double_emphasis,
	man_emphasis,
	NULL,
	man_linebreak,
	NULL,
	NULL,
	NULL,

	/* low-level callbacks */
	NULL,
	man_normal_text,

	/* renderer data */
	64,
	"*_",
	NULL };



/*****************
 * MAIN FUNCTION *
 *****************/

/* main • main function, interfacing STDIO with the parser */
int
main(int argc, char **argv) {
	struct buf *ib, *ob;
	size_t ret;
	FILE *in = stdin;
	int ch, argerr, help, i;
	char *tmp;
	char datebuf[64];
	time_t ttm;
	struct tm *tm;
	struct stat st;
	struct metadata man_metadata;

	struct option longopts[] = {
		{ "date",	no_argument,		0, 	'd' },
		{ "help",	required_argument,	0,	'h' },
		{ "section",	required_argument,	0,	's' },
		{ "title",	required_argument,	0,	't' },
		{ 0,		0,			0,	0}
	};

	man_metadata.section = 1;
	man_metadata.title = NULL;
	man_metadata.date = NULL;
	/* opening the file if given from the command line */
	argerr = help = 0;
	while (!argerr &&
	    (ch = getopt_long(argc, argv, "d:hs:t:", longopts, 0)) != -1)
		switch (ch) {
			case 'd':
				man_metadata.date = optarg;
				break;
			case 'h':
				argerr = help = 1;
				break;
			case 's':
				if (strlen(optarg) != 1 &&
				    strspn(optarg, "123456789") != 1) {
					argerr = 1;
					break; }
				man_metadata.section = (int)strtol(optarg, (char **)NULL, 10);
				break;
			case 't':
				man_metadata.title = optarg;
				break;
			default:
				argerr = 1; }
	if (argerr) {
		usage(help ? stdout : stderr, argv[0]);
		return help ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	argc -= optind;
	argv += optind;

	if (argc > 0) {
		in = fopen(argv[0], "r");
		if (!in) {
			fprintf(stderr,"Unable to open input file \"%s\": %s\n",
				argv[1], strerror(errno));
			return 1; } }

	if (!man_metadata.date) {
			if (in == stdin || stat(argv[0], &st) == -1) {
				ttm = time(NULL);
				tm = localtime(&ttm); }
			else
				tm = localtime(&st.st_mtime);
			strftime(datebuf, sizeof(datebuf), "%B %d, %Y", tm);
			man_metadata.date = datebuf;
		}

	if (in == stdin && !man_metadata.title) {
		fprintf(stderr, "When reading from stdin the title should be "
		    "specified is expected\n");
		return 1; }

	if (!man_metadata.title) {
		tmp = strrchr(argv[0], '/');
		man_metadata.title = strrchr(argv[0], '/');
		if (!tmp)
			tmp = argv[0];
		else
			tmp++;
		man_metadata.title = tmp;
		tmp = strrchr(man_metadata.title, '.');
		if (tmp)
			*tmp = '\0'; }

	/* Ensure the title is uppercase */
	for (i = 0; i < strlen(man_metadata.title); i++)
		man_metadata.title[i] = toupper(man_metadata.title[i]);

	/* reading everything */
	ib = bufnew(READ_UNIT);
	bufgrow(ib, READ_UNIT);
	while ((ret = fread(ib->data + ib->size, 1,
			ib->asize - ib->size, in)) > 0) {
		ib->size += ret;
		bufgrow(ib, ib->size + READ_UNIT); }
	if (in != stdin) fclose(in);

	to_man.opaque = &man_metadata;
	/* performing markdown to man */
	ob = bufnew(OUTPUT_UNIT);
	markdown(ob, ib, &to_man);

	/* writing the result to stdout */
	ret = fwrite(ob->data, 1, ob->size, stdout);
	if (ret < ob->size)
		fprintf(stderr, "Warning: only %zu output byte written, "
				"out of %zu\n",
				ret,
				ob->size);

	/* cleanup */
	bufrelease(ib);
	bufrelease(ob);
	return 0; }

/* vim: set filetype=c: */

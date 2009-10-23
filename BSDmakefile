# Makefile

# Copyright (c) 2009, Natacha Porté
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

DEPDIR=depends
ALLDEPS=$(DEPDIR)/all
CFLAGS=-c -g -O3 -Wall -Werror
LDFLAGS=-g -O3 -Wall -Werror
CC=gcc

all:		markdown

.PHONY:		all clean


# Main project links

markdown:	main.o markdown.o array.o buffer.o renderers.o
	$(CC) $(LDFLAGS) $(.ALLSRC) -o $(.TARGET)

benchmark:	benchmark.o markdown.o array.o buffer.o renderers.o
	$(CC) $(LDFLAGS) $(.ALLSRC) -o $(.TARGET)

clean:
	rm -f *.o
	rm -f markdown
	rm -rf $(DEPDIR)


# dependencies

.sinclude "$(ALLDEPS)"


# generic object compilations

.c.o:
	@mkdir -p $(DEPDIR)
	@touch $(ALLDEPS)
	@$(CC) -MM $(.IMPSRC) > $(DEPDIR)/$(.PREFIX).d
	@grep -q "$(.PREFIX).d" $(ALLDEPS) \
			|| echo ".include \"$(.PREFIX).d\"" >> $(ALLDEPS)
	$(CC) $(CFLAGS) -o $(.TARGET) $(.IMPSRC)

.m.o:
	@mkdir -p $(DEPDIR)
	@touch $(ALLDEPS)
	@$(CC) -MM $(.IMPSRC) > depends/$(.PREFIX).d
	@grep -q "$(.PREFIX).d" $(ALLDEPS) \
			|| echo ".include \"$(.PREFIX).d\"" >> $(ALLDEPS)
	$(CC) $(CFLAGS) -o $(.TARGET) $(.IMPSRC)

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

DEPDIR	 = depends
ALLDEPS	 = $(DEPDIR)/all

AR	?= ar
CC	?= cc
CFLAGS	?= -g -O3 -Wall -Werror
LDFLAGS	?=

all:		libsoldout.a libsoldout.so mkd2html mkd2latex mkd2man

.PHONY:		all amal clean


# amalgamation
amal:
	@./make-amal


# libraries

libsoldout.a:	markdown.o array.o buffer.o renderers.o
	$(AR) rs $@ $^

libsoldout.so:	libsoldout.so.1
	ln -s $^ $@

libsoldout.so.1:	markdown.o array.o buffer.o renderers.o
	$(CC) $(LDFLAGS) -shared -Wl,-soname=$@ \
		$^ -o $@


# executables

mkd2html:	mkd2html.o libsoldout.so
	$(CC) $(LDFLAGS) $^ -o $@

mkd2latex:	mkd2latex.o libsoldout.so
	$(CC) $(LDFLAGS) $^ -o $@

mkd2man:	mkd2man.o libsoldout.so
	$(CC) $(LDFLAGS) $^ -o $@


# Housekeeping

benchmark:	benchmark.o libsoldout.so
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o
	rm -f libsoldout.a libsoldout.so libsoldout.so.*
	rm -f mkd2html mkd2latex mkd2man benchmark
	rm -rf $(DEPDIR)


# dependencies

-include "$(ALLDEPS)"


# generic object compilations

.c.o:
	@mkdir -p $(DEPDIR)
	@touch $(ALLDEPS)
	@$(CC) -MM $< > $(DEPDIR)/$*.d
	@grep -q "$*.d" $(ALLDEPS) \
			|| echo "include \"$*.d\"" >> $(ALLDEPS)
	$(CC) $(CFLAGS) -std=c99 -fPIC -c -o $@ $<

BINARIES = forftanks designer.cgi
HTML = forf.html procs.html intro.html designer.html chord.html
WWW = style.css grunge.png designer.js figures.js tanks.js nav.html.inc

CFLAGS = -Wall

all: $(BINARIES) $(HTML)

install:
	install -d $(DESTDIR)/usr/bin
	install run-tanks $(DESTDIR)/usr/bin
	install forftanks $(DESTDIR)/usr/bin
	install $(SCRIPTS) $(DESTDIR)/usr/bin


	install -d $(DESTDIR)/usr/lib/tanks
	install designer.cgi $(DESTDIR)/usr/lib/tanks
	install $(HTML) $(DESTDIR)/usr/lib/tanks
	install $(WWW) $(DESTDIR)/usr/lib/tanks
	cp -r examples $(DESTDIR)/usr/lib/tanks/examples

forftanks: forftanks.o ctanks.o forf.o
forftanks: LDLIBS = -lm

forftanks.o: forf.h ctanks.h
forf.o: forf.c forf.h
ctanks.o: ctanks.h

%.html: %.html.m4
	m4 $< > $@

.PHONY: clean
clean:
	rm -f *.o next-round round-*.html results-*.txt
	rm -f $(BINARIES) $(HTML)

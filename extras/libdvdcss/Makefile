###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.opts

BASE_A := libdvdcss.a
BASE_SO := libdvdcss.so
MAJOR_SO := libdvdcss.so.$(shell echo $(LIBDVDCSS_VERSION) | cut -f1 -d.)
FULL_SO := libdvdcss.so.$(LIBDVDCSS_VERSION)

#
# Objects
#

LIBDVDCSS_O = libdvdcss.o css.o ioctl.o
ALL_OBJ = $(LIBDVDCSS_O)

include ../../Makefile.modules

$(LIBDVDCSS_O): %.o: .dep/%.d
$(LIBDVDCSS_O): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

../../lib/$(FULL_SO): $(LIBDVDCSS_O)
	$(CC) $(PCFLAGS) -Wl,-soname -Wl,$(MAJOR_SO) -o $@ $^ $(PLCFLAGS)
	rm -f ../../lib/$(BASE_SO) && ln -s $(FULL_SO) ../../lib/$(BASE_SO)
	rm -f ../../lib/$(MAJOR_SO) && ln -s $(FULL_SO) ../../lib/$(MAJOR_SO)

../../lib/$(BASE_A): $(LIBDVDCSS_O)
	ar r $@ $^
	$(RANLIB) $@

all: ../../lib/$(FULL_SO) ../../lib/$(BASE_A)

install:
	mkdir -p $(DESTDIR)$(includedir)/videolan
	$(INSTALL) -m 644 videolan/dvdcss.h $(DESTDIR)$(includedir)/videolan
	mkdir -p $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 ../../lib/$(BASE_A) $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 ../../lib/$(FULL_SO) $(DESTDIR)$(libdir)
	rm -f $(DESTDIR)$(libdir)/$(BASE_SO) && ln -s $(FULL_SO) $(DESTDIR)$(libdir)/$(BASE_SO)
	rm -f $(DESTDIR)$(libdir)/$(MAJOR_SO) && ln -s $(FULL_SO) $(DESTDIR)$(libdir)/$(MAJOR_SO)

uninstall:
	rm -f $(DESTDIR)$(includedir)/videolan/dvdcss.h
	rm -f $(DESTDIR)$(libdir)/$(BASE_A)
	rm -f $(DESTDIR)$(libdir)/$(BASE_SO)
	rm -f $(DESTDIR)$(libdir)/$(MAJOR_SO)
	rm -f $(DESTDIR)$(libdir)/$(FULL_SO)


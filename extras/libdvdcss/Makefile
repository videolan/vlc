###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.opts

LIBDVDCSS_VERSION = 0.0.1
LIBDVDCSS_MAJOR = 0

#
# Objects
#

LIBDVDCSS_O = libdvdcss.o css.o ioctl.o
ALL_OBJ = $(LIBDVDCSS_O)

include ../../Makefile.modules

$(LIBDVDCSS_O): %.o: .dep/%.d
$(LIBDVDCSS_O): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

../../lib/libdvdcss.so.$(LIBDVDCSS_VERSION): $(LIBDVDCSS_O)
	$(CC) $(PCFLAGS) -Wl,-soname -Wl,libdvdcss.so.$(LIBDVDCSS_MAJOR) -o $@ $^ $(PLCFLAGS)
	rm -f ../../lib/libdvdcss.so && ln -s libdvdcss.so.$(LIBDVDCSS_VERSION) ../../lib/libdvdcss.so
	rm -f ../../lib/libdvdcss.so.$(LIBDVDCSS_MAJOR) && ln -s libdvdcss.so.$(LIBDVDCSS_VERSION) ../../lib/libdvdcss.so.$(LIBDVDCSS_MAJOR)

../../lib/libdvdcss.a: $(LIBDVDCSS_O)
	ar r $@ $^
	$(RANLIB) $@

all: ../../lib/libdvdcss.so.$(LIBDVDCSS_VERSION) ../../lib/libdvdcss.a

install:
	mkdir -p $(DESTDIR)$(includedir)/videolan
	$(INSTALL) -m 644 videolan/dvdcss.h $(DESTDIR)$(includedir)/videolan
	mkdir -p $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 ../../lib/libdvdcss.a $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 ../../lib/libdvdcss.so.$(LIBDVDCSS_VERSION) $(DESTDIR)$(libdir)
	rm -f $(DESTDIR)$(libdir)/libdvdcss.so && ln -s libdvdcss.so.$(LIBDVDCSS_VERSION) $(DESTDIR)$(libdir)/libdvdcss.so
	rm -f $(DESTDIR)$(libdir)/libdvdcss.so.$(LIBDVDCSS_MAJOR) && ln -s libdvdcss.so.$(LIBDVDCSS_VERSION) $(DESTDIR)$(libdir)/libdvdcss.so.$(LIBDVDCSS_MAJOR)


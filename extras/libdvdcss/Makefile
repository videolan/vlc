###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.opts

#
# Objects
#

LIBDVDCSS_O = libdvdcss.o css.o ioctl.o
ALL_OBJ = $(LIBDVDCSS_O)

include ../../Makefile.modules

$(LIBDVDCSS_O): %.o: .dep/%.d
$(LIBDVDCSS_O): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

../../lib/libdvdcss.so: $(LIBDVDCSS_O)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS)

../../lib/libdvdcss.a: $(LIBDVDCSS_O)
	ar r $@ $^
	$(RANLIB) $@

#all: ../../lib/libdvdcss.so ../../lib/libdvdcss.a
all: ../../lib/libdvdcss.a


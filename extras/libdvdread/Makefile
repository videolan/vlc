###############################################################################
# vlc (VideoLAN Client) dvdread module Makefile
# (c)2001 VideoLAN
###############################################################################

LIBDVDREAD_VERSION := 0.9.2
CFLAGS_LIBDVDREAD := -I. -DHAVE_CONFIG_H
LIB_LIBDVDREAD := -ldl

BASE_A := libdvdread.a
BASE_SO := libdvdread.so
MAJOR_SO := libdvdread.so.$(shell echo $(LIBDVDREAD_VERSION) | cut -f1 -d.)
FULL_SO := libdvdread.so.$(LIBDVDREAD_VERSION)
ifneq (,$(SOFLAGS))
ALL_SOFLAGS := $(SOFLAGS)$(MAJOR_SO)
SOFLAGS :=
endif

#
# Objects
#

OBJ_C = dvdread.o dvd_udf.o nav_print.o nav_read.o ifo_print.o ifo_read.o

PLUGIN_OBJ = $(OBJ_C) $(OBJ_LIBDVDREAD)
BUILTIN_OBJ = $(OBJ_C:%.o=DVDREAD_%.o)

ALL_OBJ = $(PLUGIN_OBJ) $(BUILTIN_OBJ)

include ../../Makefile.modules

$(OBJ_C): %.o: .dep/%.d
$(OBJ_C): %.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LIBDVDREAD) $(PCFLAGS) -c -o $@ $<

$(BUILTIN_OBJ): DVDREAD_%.o: .dep/%.d
$(BUILTIN_OBJ): DVDREAD_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LIBDVDREAD) $(PCFLAGS) -c -o $@ $<

../../lib/$(FULL_SO): $(OBJ_C) $(OBJ_LIBDVDREAD)
	$(CC) $(PCFLAGS) $(ALL_SOFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_LIBDVDREAD)
	rm -f ../../lib/$(BASE_SO) && ln -s $(FULL_SO) ../../lib/$(BASE_SO)
	rm -f ../../lib/$(MAJOR_SO) && ln -s $(FULL_SO) ../../lib/$(MAJOR_SO)

../../lib/$(BASE_A): libdvdcss $(BUILTIN_OBJ) $(OBJ_LIBDVDREAD)
	ar r $@ $(BUILTIN_OBJ)
	$(RANLIB) $@

#
# Virtual targets
#
all: ../../lib/$(BASE_A)

libdvdcss:
	@cd ../../ && $(MAKE) libdvdcss

#install:
#	mkdir -p $(DESTDIR)$(includedir)/videolan
#	$(INSTALL) -m 644 videolan/dvdcss.h $(DESTDIR)$(includedir)/videolan
#	mkdir -p $(DESTDIR)$(libdir)
#	-$(INSTALL) -m 644 ../../lib/$(BASE_A) $(DESTDIR)$(libdir)
#	-$(INSTALL) -m 644 ../../lib/$(FULL_SO) $(DESTDIR)$(libdir)
#	rm -f $(DESTDIR)$(libdir)/$(BASE_SO) && ln -s $(FULL_SO) $(DESTDIR)$(libdir)/$(BASE_SO)
#	rm -f $(DESTDIR)$(libdir)/$(MAJOR_SO) && ln -s $(FULL_SO) $(DESTDIR)$(libdir)/$(MAJOR_SO)
#
#uninstall:
#	rm -f $(DESTDIR)$(includedir)/videolan/dvdcss.h
#	rm -f $(DESTDIR)$(libdir)/$(BASE_A)
#	rm -f $(DESTDIR)$(libdir)/$(BASE_SO)
#	rm -f $(DESTDIR)$(libdir)/$(MAJOR_SO)
#	rm -f $(DESTDIR)$(libdir)/$(FULL_SO)


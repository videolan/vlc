###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

BASE_A := libdvdcss.a
BASE_SO := libdvdcss.so
MAJOR_SO := libdvdcss.so.$(shell echo $(LIBDVDCSS_VERSION) | cut -f1 -d.)
FULL_SO := libdvdcss.so.$(LIBDVDCSS_VERSION)
ifneq (,$(SOFLAGS))
ALL_SOFLAGS := $(SOFLAGS)$(MAJOR_SO)
SOFLAGS :=
endif

#
# C headers directories
#
CFLAGS := -I../../include -I../../extras $(CFLAGS)

#
# Objects
#
OBJ_C = libdvdcss.o css.o ioctl.o
ALL_OBJ = $(OBJ_C) $(OBJ_LIBDVDCSS)

# 
# Standard dependencies
#
C_DEP := $(ALL_OBJ:%.o=.dep/%.d)

#
# Virtual targets
#
all: ../../lib/$(FULL_SO) ../../lib/$(BASE_A)

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o
	rm -rf .dep

install:
	mkdir -p $(DESTDIR)$(includedir)/videolan
	$(INSTALL) -m 644 videolan/dvdcss.h $(DESTDIR)$(includedir)/videolan
	mkdir -p $(DESTDIR)$(libdir)
	-$(INSTALL) -m 644 ../../lib/$(BASE_A) $(DESTDIR)$(libdir)
	-$(INSTALL) -m 644 ../../lib/$(FULL_SO) $(DESTDIR)$(libdir)
	rm -f $(DESTDIR)$(libdir)/$(BASE_SO) && ln -s $(FULL_SO) $(DESTDIR)$(libdir)/$(BASE_SO)
	rm -f $(DESTDIR)$(libdir)/$(MAJOR_SO) && ln -s $(FULL_SO) $(DESTDIR)$(libdir)/$(MAJOR_SO)

uninstall:
	rm -f $(DESTDIR)$(includedir)/videolan/dvdcss.h
	rm -f $(DESTDIR)$(libdir)/$(BASE_A)
	rm -f $(DESTDIR)$(libdir)/$(BASE_SO)
	rm -f $(DESTDIR)$(libdir)/$(MAJOR_SO)
	rm -f $(DESTDIR)$(libdir)/$(FULL_SO)

FORCE:

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(C_DEP): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(OBJ_C): %.o: .dep/%.d
$(OBJ_C): %.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LIBDVDCSS) $(PCFLAGS) -c -o $@ $<

bsdi_dvdioctl.o: ../BSDI_dvdioctl/dvdioctl.c
	$(CC) $(CFLAGS) $(CFLAGS_LIBDVDCSS) $(PCFLAGS) -c -o $@ $<

../../lib/$(FULL_SO): $(OBJ_C) $(OBJ_LIBDVDCSS)
	$(CC) $(PCFLAGS) $(ALL_SOFLAGS) -o $@ $^ $(PLDFLAGS) $(LIB_LIBDVDCSS)
	rm -f ../../lib/$(BASE_SO) && ln -s $(FULL_SO) ../../lib/$(BASE_SO)
	rm -f ../../lib/$(MAJOR_SO) && ln -s $(FULL_SO) ../../lib/$(MAJOR_SO)

../../lib/$(BASE_A): $(OBJ_C) $(OBJ_LIBDVDCSS)
	ar r $@ $^
	$(RANLIB) $@


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
# C headers directories
#
CFLAGS := -I../../include -I../../extras $(CFLAGS)

#
# Objects
#

OBJ_C = dvdread.o dvd_udf.o nav_print.o nav_read.o ifo_print.o ifo_read.o
ALL_OBJ = $(OBJ_C) $(OBJ_LIBDVDREAD)

#
# Standard dependencies
# 
C_DEP := $(ALL_OBJ:%.o=.dep/%.d)

#
# Virtual targets
#
all: ../../lib/$(BASE_A)

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o
	rm -rf .dep

FORCE:

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(C_DEP): %.d: FORCE
	        @$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(OBJ_C): %.o: .dep/%.d
$(OBJ_C): %.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LIBDVDREAD) $(PCFLAGS) -c -o $@ $<

../../lib/$(FULL_SO): $(OBJ_C) $(OBJ_LIBDVDREAD)
	$(CC) $(PCFLAGS) $(ALL_SOFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_LIBDVDREAD)
	rm -f ../../lib/$(BASE_SO) && ln -s $(FULL_SO) ../../lib/$(BASE_SO)
	rm -f ../../lib/$(MAJOR_SO) && ln -s $(FULL_SO) ../../lib/$(MAJOR_SO)

../../lib/$(BASE_A): $(OBJ_C) $(OBJ_LIBDVDREAD)
	ar r $@ $^
	$(RANLIB) $@


###############################################################################
# vlc (VideoLAN Client) dvdread module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_DVDREAD = dvdread.o input_dvdread.o $(OBJ_DVDREAD)
BUILTIN_DVDREAD = $(PLUGIN_DVDREAD:%.o=BUILTIN_%.o)

LIBDVDCSS_OBJ = ../../lib/libdvdcss.a ../../lib/libdvdcss.so
LIBDVDREAD_OBJ = ../../lib/libdvdread.a ../../lib/libdvdread.so

ALL_OBJ = $(PLUGIN_DVDREAD) $(BUILTIN_DVDREAD)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_DVDREAD): %.o: .dep/%.d
$(PLUGIN_DVDREAD): %.o: %.c
	$(CC) $(CFLAGS_DVD) $(CFLAGS_DVDREAD) $(CFLAGS) $(PCFLAGS) -DPLUGIN -c -o $@ $<

$(BUILTIN_DVDREAD): BUILTIN_%.o: .dep/%.d
$(BUILTIN_DVDREAD): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS_DVD) $(CFLAGS_DVDREAD) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#

ifeq (1,$(NEED_LIBDVDCSS))
../dvdread.so: $(LIBDVDCSS_OBJ) $(LIBDVDREAD_OBJ) $(PLUGIN_DVDREAD)
	$(CC) -o $@ $(PLUGIN_DVDREAD) $(PLCFLAGS) $(LIB_DVD_PLUGIN) $(LIB_DVDREAD_PLUGIN)

../dvdread.a: $(LIBDVDCSS_OBJ) $(LIBDVDREAD_OBJ) $(BUILTIN_DVDREAD)
	ar r $@ $(BUILTIN_DVDREAD)
	$(RANLIB) $@
else
../dvdread.so: $(LIBDVDREAD_OBJ) $(PLUGIN_DVDREAD)
	$(CC) -o $@ $(PLUGIN_DVD) $(PLCFLAGS) $(LIB_DVD_PLUGIN) $(LIB_DVDREAD_PLUGIN)

../dvdread.a: $(LIBDVDREAD_OBJ) $(BUILTIN_DVDREAD)
	ar r $@ $(BUILTIN_DVDREAD)
	$(RANLIB) $@
endif

$(LIBDVDCSS_OBJ): libdvdcss

libdvdcss:
	@cd ../../ && $(MAKE) libdvdcss

$(LIBDVDREAD_OBJ): libdvdread

libdvdread:
	@cd ../../ && $(MAKE) libdvdread

###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_DVD = dvd.o input_dvd.o dvd_ifo.o dvd_udf.o dvd_summary.o $(OBJ_DVD)
BUILTIN_DVD = $(PLUGIN_DVD:%.o=BUILTIN_%.o)

LIBDVDCSS_OBJ = ../../lib/libdvdcss.a ../../lib/libdvdcss.so

ALL_OBJ = $(PLUGIN_DVD) $(BUILTIN_DVD)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_DVD): %.o: .dep/%.d
$(PLUGIN_DVD): %.o: %.c
	$(CC) $(CFLAGS_DVD) $(CFLAGS) $(PCFLAGS) -DPLUGIN -c -o $@ $<

$(BUILTIN_DVD): BUILTIN_%.o: .dep/%.d
$(BUILTIN_DVD): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS_DVD) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#

# If we build our own libdvdcss
ifeq (1,$(NEED_LIBDVDCSS))
../dvd.so: $(LIBDVDCSS_OBJ) $(PLUGIN_DVD)
	$(CC) -o $@ $(PLUGIN_DVD) $(PLCFLAGS) $(LIB_DVD_PLUGIN)

../dvd.a: $(LIBDVDCSS_OBJ) $(BUILTIN_DVD)
	ar r $@ $(BUILTIN_DVD)
	$(RANLIB) $@

# If we rely on an existing libdvdcss
else
../dvd.so: $(PLUGIN_DVD)
	$(CC) -o $@ $(PLUGIN_DVD) $(PLCFLAGS) $(LIB_DVD_PLUGIN)

../dvd.a: $(BUILTIN_DVD)
	ar r $@ $(BUILTIN_DVD)
	$(RANLIB) $@
endif

$(LIBDVDCSS_OBJ): libdvdcss

libdvdcss:
	@cd ../../ && $(MAKE) libdvdcss


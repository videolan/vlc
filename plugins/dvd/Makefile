###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_DVD = dvd.o input_dvd.o dvd_netlist.o dvd_ifo.o dvd_udf.o dvd_summary.o
BUILTIN_DVD = $(PLUGIN_DVD:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_DVD) $(BUILTIN_DVD)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_DVD): %.o: .dep/%.d
$(PLUGIN_DVD): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_DVD) -DPLUGIN -c -o $@ $<

$(BUILTIN_DVD): BUILTIN_%.o: .dep/%.d
$(BUILTIN_DVD): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_DVD) -DBUILTIN -c -o $@ $<

#
# Real targets
#

ifeq (1,$(LOCAL_LIBDVDCSS))
../../lib/dvd.so: libdvdcss $(PLUGIN_DVD)
	$(CC) $(PCFLAGS) -o $@ $(PLUGIN_DVD) $(PLCFLAGS) ../../lib/libdvdcss.a $(LIB_DVD)

../../lib/dvd.a: libdvdcss $(BUILTIN_DVD)
	ar r $@ $(BUILTIN_DVD)
	$(RANLIB) $@
else
../../lib/dvd.so: $(PLUGIN_DVD)
	$(CC) $(PCFLAGS) -o $@ $(PLUGIN_DVD) $(PLCFLAGS) $(LIB_DVD)

../../lib/dvd.a: $(BUILTIN_DVD)
	ar r $@ $(BUILTIN_DVD)
	$(RANLIB) $@
endif

libdvdcss:
	cd ../../ && $(MAKE) libdvdcss


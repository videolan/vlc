###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = dvd.o input_dvd.o dvd_netlist.o dvd_ioctl.o dvd_ifo.o dvd_udf.o dvd_css.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/dvd.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/dvd.a: $(BUILTIN_C)
	ar r $@ $^


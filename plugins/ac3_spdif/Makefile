###############################################################################
# vlc (VideoLAN Client) AC3 spdif audio decoder module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
PLUGIN_C = ac3_spdif.o ac3_iec958.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)
#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../ac3_spdif.so: $(PLUGIN_C)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../ac3_spdif.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@


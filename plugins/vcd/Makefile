###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_VCD = vcd.o input_vcd.o linux_cdrom_tools.o
BUILTIN_VCD = $(PLUGIN_VCD:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_VCD) $(BUILTIN_VCD)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_VCD): %.o: .dep/%.d
$(PLUGIN_VCD): %.o: %.c
	$(CC) $(CFLAGS_VCD) $(CFLAGS) $(PCFLAGS) -DPLUGIN -c -o $@ $<

$(BUILTIN_VCD): BUILTIN_%.o: .dep/%.d
$(BUILTIN_VCD): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS_VCD) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#

../vcd.so: $(PLUGIN_VCD)
	$(CC) -o $@ $(PLUGIN_VCD) $(PLCFLAGS)

../vcd.a: $(BUILTIN_VCD)
	ar r $@ $(BUILTIN_VCD)
	$(RANLIB) $@



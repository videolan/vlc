###############################################################################
# vlc (VideoLAN Client) dvd module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_DVD = dvd.o input_dvd.o dvd_netlist.o dvd_ioctl.o dvd_ifo.o dvd_udf.o dvd_css.o
BUILTIN_DVD = $(PLUGIN_DVD:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_DVD) $(BUILTIN_DVD)

objects := $(ALL_OBJ)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.so *.a *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep

$(PLUGIN_DVD): %.o: .dep/%.d
$(PLUGIN_DVD): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(BUILTIN_DVD): %-BUILTIN.o: .dep/%.d
$(BUILTIN_DVD): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/dvd.so: $(PLUGIN_DVD)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/dvd.a: $(BUILTIN_DVD)
	ar r $@ $^


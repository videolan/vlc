###############################################################################
# vlc (VideoLAN Client) dummy module makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_DUMMY = dummy.o aout_dummy.o vout_dummy.o intf_dummy.o
BUILTIN_DUMMY = $(PLUGIN_DUMMY:%.o=%-BUILTIN.o)

STD_PLUGIN_OBJ = $(PLUGIN_DUMMY)
STD_BUILTIN_OBJ = $(BUILTIN_DUMMY)
ALL_OBJ = $(STD_PLUGIN_OBJ) $(STD_BUILTIN_OBJ)

objects := $(STD_PLUGIN_OBJ) $(STD_BUILTIN_OBJ)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(STD_PLUGIN_OBJ): %.o: .dep/%.d
$(STD_PLUGIN_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(STD_BUILTIN_OBJ): %-BUILTIN.o: .dep/%.d
$(STD_BUILTIN_OBJ): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/dummy.so: $(PLUGIN_DUMMY)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/dummy.a: $(BUILTIN_DUMMY)
	ar r $@ $^


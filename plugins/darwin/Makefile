###############################################################################
# vlc (VideoLAN Client) Darwin module makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_DARWIN = darwin.o
BUILTIN_DARWIN = $(PLUGIN_DARWIN:%.o=%-BUILTIN.o)

STD_PLUGIN_OBJ = $(PLUGIN_DARWIN)
STD_BUILTIN_OBJ = $(BUILTIN_DARWIN)
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
../../lib/darwin.so: $(PLUGIN_DARWIN)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -framework CoreAudio

../../lib/darwin.a: $(BUILTIN_DARWIN)
	ar r $@ $^


###############################################################################
# vlc (VideoLAN Client) ggi module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_GGI = ggi.o vout_ggi.o
BUILTIN_GGI = $(PLUGIN_GGI:%.o=%-BUILTIN.o)

STD_PLUGIN_OBJ = $(PLUGIN_GGI)
STD_BUILTIN_OBJ = $(BUILTIN_GGI)
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
	rm -f *.o *.moc *.bak *.so *.a *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep

$(STD_PLUGIN_OBJ): %.o: .dep/%.d
$(STD_PLUGIN_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(STD_BUILTIN_OBJ): %-BUILTIN.o: .dep/%.d
$(STD_BUILTIN_OBJ): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/ggi.so: $(PLUGIN_GGI)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GGI)

../../lib/ggi.a: $(BUILTIN_GGI)
	ar r $@ $^


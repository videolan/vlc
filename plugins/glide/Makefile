###############################################################################
# vlc (VideoLAN Client) glide module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_GLIDE = glide.o vout_glide.o
BUILTIN_GLIDE = $(PLUGIN_GLIDE:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_GLIDE) $(BUILTIN_GLIDE)

objects := $(PLUGIN_GLIDE) $(BUILTIN_GLIDE)
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

$(PLUGIN_GLIDE): %.o: .dep/%.d
$(PLUGIN_GLIDE): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -I/usr/include/glide -c -o $@ $<

$(BUILTIN_GLIDE): %-BUILTIN.o: .dep/%.d
$(BUILTIN_GLIDE): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/glide -c -o $@ $<

#
# Real targets
#
../../lib/glide.so: $(PLUGIN_GLIDE)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GLIDE)

../../lib/glide.a: $(BUILTIN_GLIDE)
	ar r $@ $^


###############################################################################
# vlc (VideoLAN Client) glide module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_GLIDE = glide.o vout_glide.o
BUILTIN_GLIDE = $(PLUGIN_GLIDE:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_GLIDE) $(BUILTIN_GLIDE)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_GLIDE): %.o: .dep/%.d
$(PLUGIN_GLIDE): %.o: %.c
	$(CC) $(CFLAGS) -DPLUGIN $(PCFLAGS) -I/usr/include/glide -c -o $@ $<

$(BUILTIN_GLIDE): BUILTIN_%.o: .dep/%.d
$(BUILTIN_GLIDE): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/glide -c -o $@ $<

#
# Real targets
#

../../plugins/glide.so: $(PLUGIN_GLIDE)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GLIDE)

../../plugins/glide.a: $(BUILTIN_GLIDE)
	ar r $@ $^
	$(RANLIB) $@


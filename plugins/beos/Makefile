###############################################################################
# vlc (VideoLAN Client) BeOS module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_CPP = beos.o aout_beos.o vout_beos.o intf_beos.o InterfaceWindow.o DrawingTidbits.o TransportButton.o PlayListWindow.o MediaControlView.o
BUILTIN_CPP = $(PLUGIN_CPP:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_CPP) $(BUILTIN_CPP)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../beos.so: $(PLUGIN_CPP)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_BEOS)

../beos.a: $(BUILTIN_CPP)
	ar r $@ $^
	$(RANLIB) $@


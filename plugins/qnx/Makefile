###############################################################################
# vlc (VideoLAN Client) QNX RTOS module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = qnx.o aout_qnx.o vout_qnx.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../qnx.so: $(PLUGIN_C)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_QNX) 

../qnx.a: $(BUILTIN_C)
	ar r $@ $^

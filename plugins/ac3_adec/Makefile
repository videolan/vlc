###############################################################################
# vlc (VideoLAN Client) AC3 audio decoder module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
PLUGIN_C = ac3_adec.o ac3_decoder.o ac3_parse.o ac3_exponent.o ac3_bit_allocate.o ac3_mantissa.o ac3_rematrix.o ac3_imdct.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)
#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../ac3_adec.so: $(PLUGIN_C)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../ac3_adec.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@


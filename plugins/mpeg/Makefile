###############################################################################
# vlc (VideoLAN Client) mpeg module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_PS = ps.o input_ps.o
PLUGIN_TS = ts.o input_ts.o
BUILTIN_PS = $(PLUGIN_PS:%.o=%-BUILTIN.o)
BUILTIN_TS = $(PLUGIN_TS:%.o=%-BUILTIN.o)

STD_PLUGIN_OBJ = $(PLUGIN_PS) $(PLUGIN_TS)
STD_BUILTIN_OBJ = $(BUILTIN_PS) $(BUILTIN_TS)
ALL_OBJ = $(STD_PLUGIN_OBJ) $(STD_BUILTIN_OBJ)

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

$(STD_PLUGIN_OBJ): %.o: .dep/%.d
$(STD_PLUGIN_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(STD_BUILTIN_OBJ): %-BUILTIN.o: .dep/%.d
$(STD_BUILTIN_OBJ): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/ps.so: $(PLUGIN_PS)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/ps.a: $(BUILTIN_PS)
	ar r $@ $^

../../lib/ts.so: $(PLUGIN_TS)
ifneq (,$(findstring darwin,$(SYS)))
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -framework AGL -framework Carbon
else
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 
endif

../../lib/ts.a: $(BUILTIN_TS)
	ar r $@ $^


################################################################################
# vlc (VideoLAN Client) main makefile
# (c)1998 VideoLAN
################################################################################
# This makefile is the main makefile for the VideoLAN client.
################################################################################

################################################################################
# Configuration
################################################################################

#CC = gcc
#SHELL = /bin/sh

################################################################################
# Settings and other variables
################################################################################

#
# C headers directories
#
INCLUDE += -Iinclude
INCLUDE += -I/usr/X11R6/include/X11

#
# Libraries
#
LIB += -L/usr/X11R6/lib
LIB += -lX11
LIB += -lXext 
LIB += -lpthread
LIB += -lXpm

#
# C compiler flags: compilation
#
CCFLAGS += $(INCLUDE)
CCFLAGS += -Wall
CCFLAGS += -D_REENTRANT
CCFLAGS += -D_GNU_SOURCE

# Optimizations : don't compile debug versions with them
CCFLAGS += -O6
CCFLAGS += -ffast-math -funroll-loops -fargument-noalias-global
#CCFLAGS += -fomit-frame-pointer -s
#LCFLAGS += -s

# Platform-specific optimizations
# Optimizations for x86 familiy :
CCFLAGS += -malign-double
CCFLAGS += -march=pentiumpro
#CCFLAGS += -march=pentium

# MMX support :
CFLAGS += -DHAVE_MMX
ASM_OBJ =   video_decoder_ref/idctmmx.o \
		    video_decoder_ref/yuv12-rgb16.o

#Optimizations for PowerPC :
#CCFLAGS += -mcpu=604e -mmultiple -mhard-float -mstring

#
# C compiler flags: dependancies
#
DCFLAGS += $(INCLUDE)
DCFLAGS += -MM

#
# C compiler flags: linking
#
LCFLAGS += $(LIB)
LCFLAGS += -Wall

#
# C compiler flags: functions flow
#
FCFLAGS += $(INCLUDE)
FCFLAGS += -A
FCFLAGS += -P
FCFLAGS += -v
FCFLAGS	+= -a
FCFLAGS += -X errno.h
FCFLAGS += -X fcntl.h
FCFLAGS += -X signal.h
FCFLAGS += -X stdio.h
FCFLAGS += -X stdlib.h
FCFLAGS += -X string.h
FCFLAGS += -X unistd.h
FCFLAGS += -X sys/ioctl.h
FCFLAGS += -X sys/stat.h
FCFLAGS += -X X11/Xlib.h
FFILTER = grep -v "intf_.*Msg.*\.\.\."

#
# C compiler flags: common flags
#
# CFLAGS

#
# Additionnal debugging flags
#
# Debugging settings: electric fence, debuging symbols and profiling support. 
# Note that electric fence and accurate profiling are quite uncompatible.
#CCFLAGS += -g
#CCFLAGS += -pg
#LCFLAGS += -g
#LCFLAGS += -pg
#LIB += -ldmalloc
#LIB += -lefence

#################################################################################
# Objects and files
#################################################################################

#
# Objects
# 
interface_obj =  		interface/main.o \
						interface/interface.o \
						interface/intf_msg.o \
						interface/intf_cmd.o \
						interface/intf_ctrl.o \
						interface/control.o \
						interface/xconsole.o 

input_obj =         		input/input_vlan.o \
						input/input_file.o \
						input/input_netlist.o \
						input/input_network.o \
						input/input_ctrl.o \
						input/input_pcr.o \
						input/input_psi.o \
						input/input.o

audio_output_obj = 		audio_output/audio_output.o \
						audio_output/audio_dsp.o

video_output_obj = 		video_output/video_output.o \
						video_output/video_x11.o \
						video_output/video_graphics.o 

audio_decoder_obj =		audio_decoder/audio_decoder.o \
						audio_decoder/audio_math.o

#generic_decoder_obj =		generic_decoder/generic_decoder.o

video_decoder_obj =		video_decoder_ref/video_decoder.o \
						video_decoder_ref/display.o \
						video_decoder_ref/getblk.o \
						video_decoder_ref/gethdr.o \
						video_decoder_ref/getpic.o \
						video_decoder_ref/getvlc.o \
						video_decoder_ref/idct.o \
						video_decoder_ref/motion.o \
						video_decoder_ref/mpeg2dec.o \
						video_decoder_ref/recon.o \
						video_decoder_ref/spatscal.o

misc_obj =			misc/mtime.o \
						misc/xutils.o \
						misc/rsc_files.o \
						misc/netutils.o

C_OBJ = $(interface_obj) \
		$(input_obj) \
		$(audio_output_obj) \
		$(video_output_obj) \
		$(audio_decoder_obj) \
		$(generic_decoder_obj) \
		$(video_decoder_obj) \
		$(vlan_obj) \
		$(misc_obj) \

#
# Other lists of files
#
sources := $(C_OBJ:%.o=%.c)
dependancies := $(sources:%.c=dep/%.d)

# All symbols must be exported
export

################################################################################
# Targets
################################################################################

#
# Virtual targets
#
all: vlc

clean:
	rm -f $(C_OBJ) $(ASM_OBJ)

distclean: clean
	rm -f **/*.o **/*~ *.log
	rm -f vlc gmon.out core Documentation/cflow
	rm -rf dep

FORCE:

#
# Real targets
#
vlc: $(C_OBJ) $(ASM_OBJ)
	$(CC) $(LCFLAGS) $(CFLAGS) -o $@ $(C_OBJ) $(ASM_OBJ)

Documentation/cflow: $(sources)
	cflow $(FCFLAGS) $(CFLAGS) $(sources) | $(FFILTER) > $@

#
# Generic rules (see below)
#
$(dependancies): %.d: FORCE
	@make -s --no-print-directory -f Makefile.dep $@

$(C_OBJ): %.o: dep/%.d

$(C_OBJ): %.o: %.c
	$(CC) $(CCFLAGS) $(CFLAGS) -c -o $@ $<
$(ASM_OBJ): %.o: %.S
	$(CC) -c -o $@ $<

################################################################################
# Note on generic rules and dependancies
################################################################################

# Note on dependancies: each .c file is associated with a .d file, which
# depends of it. The .o file associated with a .c file depends of the .d, of the 
# .c itself, and of Makefile. The .d files are stored in a separate dep/ 
# directory.
# The dep directory should be ignored by CVS.

# Note on inclusions: depending of the target, the dependancies files must 
# or must not be included. The problem is that if we ask make to include a file,
# and this file does not exist, it is made before it can be included. In a 
# general way, a .d file should be included if and only if the corresponding .o 
# needs to be re-made.

# Two makefiles are used: the main one (this one) has regular generic rules,
# except for .o files, for which it calls the object Makefile. Dependancies
# are not included in this file.
# The object Makefile known how to make a .o from a .c, and includes
# dependancies for the target, but only those required.

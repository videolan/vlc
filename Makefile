################################################################################
# vlc (VideoLAN Client) main makefile
# (c)1998 VideoLAN
################################################################################
# This makefile is the main makefile for the VideoLAN client.
################################################################################

################################################################################
# Configuration
################################################################################

# Environment
#CC=gcc
#SHELL=/bin/sh

# Video output settings
VIDEO=X11
#VIDEO=DGA (not yet supported)
#VIDEO=FB
#VIDEO=GGI
#VIDEO=BEOS (not yet supported)

# Target architecture and optimization
#ARCH=
ARCH=MMX
#ARCH=PPC
#ARCH=SPARC

# Target operating system
SYS=LINUX
#SYS=BSD
#SYS=BEOS

# Decoder choice - ?? old decoder will be removed soon
#DECODER=old
DECODER=new

# Debugging mode on or off
DEBUG=1

#----------------- do not change anything below this line ----------------------

################################################################################
# Configuration pre-processing
################################################################################

# Program version - may only be changed by the project leader
PROGRAM_VERSION = 1.0-dev

# PROGRAM_OPTIONS is an identification string of the compilation options
PROGRAM_OPTIONS = $(VIDEO) $(ARCH) $(SYS)
ifneq ($(DEBUG),)
PROGRAM_OPTIONS += DEBUG
endif

# PROGRAM_BUILD is a complete identification of the build
PROGRAM_BUILD = `date -R` $(USER)@`hostname`

# DEFINE will contain some of the constants definitions decided in Makefile, 
# including VIDEO_xx and ARCH_xx. It will be passed to C compiler.
DEFINE += -DVIDEO_$(VIDEO) 
DEFINE += -DARCH_$(ARCH)
DEFINE += -DSYS_$(SYS)
DEFINE += -DPROGRAM_VERSION="\"$(PROGRAM_VERSION)\""
DEFINE += -DPROGRAM_OPTIONS="\"$(PROGRAM_OPTIONS)\""
DEFINE += -DPROGRAM_BUILD="\"$(PROGRAM_BUILD)\""
ifneq ($(DEBUG),)
DEFINE += -DDEBUG
endif

# video is a lowercase version of VIDEO used for filenames
video = $(shell echo $(VIDEO) | tr 'A-Z' 'a-z')

################################################################################
# Tunning and other variables - do not change anything except if you know
# exactly what you are doing
################################################################################

#
# C headers directories
#
INCLUDE += -Iinclude

ifeq ($(VIDEO),X11)
INCLUDE += -I/usr/X11R6/include
endif

#
# Libraries
#
LIB += -lpthread
LIB += -lm

ifeq ($(VIDEO),X11)
LIB += -L/usr/X11R6/lib
LIB += -lX11
LIB += -lXext 
endif
ifeq ($(VIDEO),GGI)
LIB += -lggi
endif

#
# C compiler flags: compilation
#
CCFLAGS += $(DEFINE) $(INCLUDE)
CCFLAGS += -Wall
CCFLAGS += -D_REENTRANT
CCFLAGS += -D_GNU_SOURCE

# Optimizations : don't compile debug versions with them
CCFLAGS += -O6
CCFLAGS += -ffast-math -funroll-loops -fargument-noalias-global
CCFLAGS += -fomit-frame-pointer

# Optimizations for x86 familiy, without MMX
ifeq ($(ARCH),)
CCFLAGS += -malign-double
CCFLAGS += -march=pentiumpro
#CCFLAGS += -march=pentium
endif

# Optimization for x86 with MMX support
ifeq ($(ARCH),MMX)
CCFLAGS += -malign-double
CCFLAGS += -march=pentiumpro
endif

# Optimizations for PowerPC
ifeq ($(ARCH),PPC)
CCFLAGS += -mcpu=604e -mmultiple -mhard-float -mstring
endif

# Optimizations for Sparc
ifeq ($(ARCH),SPARC)
CCFLAGS += -mhard-float
endif

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
#LCFLAGS += -s

#
# C compiler flags: common flags
#

# Optimizations for x86 with MMX support
ifeq ($(ARCH),MMX)
CFLAGS += -DHAVE_MMX
endif

#
# Additionnal debugging flags
#

# Debugging support
ifneq ($(DEBUG),)
CFLAGS += -g
#CFLAGS += -pg
endif

#################################################################################
# Objects and files
#################################################################################

#
# C Objects
# 
interface_obj =  		interface/main.o \
						interface/interface.o \
						interface/intf_msg.o \
						interface/intf_cmd.o \
						interface/intf_ctrl.o \
						interface/control.o \
						interface/intf_console.o \
						interface/intf_$(video).o

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
						video_output/video_$(video).o \
						video_output/video_yuv.o

ac3_decoder_obj =		ac3_decoder/ac3_decoder.o \
						ac3_decoder/ac3_parse.o \
						ac3_decoder/ac3_exponent.o \
						ac3_decoder/ac3_bit_allocate.o \
						ac3_decoder/ac3_mantissa.o \
						ac3_decoder/ac3_rematrix.o \
						ac3_decoder/ac3_imdct.o \
						ac3_decoder/ac3_downmix.o

audio_decoder_obj =		audio_decoder/audio_decoder.o \
						audio_decoder/audio_math.o

subtitle_decoder_obj =		subtitle_decoder/subtitle_decoder.o

#??generic_decoder_obj =		generic_decoder/generic_decoder.o
# remeber to add it to OBJ 

ifeq ($(DECODER),old)
CFLAGS += -DOLD_DECODER
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
else
video_parser_obj = 		video_parser/video_parser.o \
						video_parser/vpar_headers.o \
						video_parser/vpar_blocks.o \
						video_parser/vpar_motion.o \
						video_parser/vpar_synchro.o \
						video_parser/video_fifo.o

video_decoder_obj =		video_decoder/video_decoder.o \
						video_decoder/vdec_motion.o \
			                        video_decoder/vdec_idct.o
endif

misc_obj =			misc/mtime.o \
						misc/rsc_files.o \
						misc/netutils.o

C_OBJ = $(interface_obj) \
		$(input_obj) \
		$(audio_output_obj) \
		$(video_output_obj) \
		$(ac3_decoder_obj) \
		$(audio_decoder_obj) \
		$(subtitle_decoder_obj) \
		$(generic_decoder_obj) \
		$(video_parser_obj) \
		$(video_decoder_obj) \
		$(vlan_obj) \
		$(misc_obj)

#
# Assembler Objects
# 
ifeq ($(ARCH),MMX)
ifeq ($(DECODER),old)
ASM_OBJ = 			video_decoder_ref/idctmmx.o \
						video_output/video_yuv_mmx.o
else
ASM_OBJ = 			video_decoder/idctmmx.o \
						video_output/video_yuv_mmx.o
endif
endif

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
	rm -f vlc gmon.out core
	rm -rf dep

show:
	@echo "Command line for C objects:"
	@echo $(CC) $(CCFLAGS) $(CFLAGS) -c -o "<dest.o>" "<src.c>"
	@echo
	@echo "Command line for assembler objects:"
	@echo $(CC) $(CFLAGS) -c -o "<dest.o>" "<src.S>"

FORCE:

#
# Real targets
#
vlc: $(C_OBJ) $(ASM_OBJ)
	$(CC) $(LCFLAGS) $(CFLAGS) -o $@ $(C_OBJ) $(ASM_OBJ)	

#
# Generic rules (see below)
#
$(dependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f Makefile.dep $@

$(C_OBJ): %.o: Makefile Makefile.dep
$(C_OBJ): %.o: dep/%.d
$(C_OBJ): %.o: %.c
	@echo "compiling $*.c"
	@$(CC) $(CCFLAGS) $(CFLAGS) -c -o $@ $<

$(ASM_OBJ): %.o: Makefile Makefile.dep
$(ASM_OBJ): %.o: %.S
	@echo "assembling $*.S"
	@$(CC) $(CFLAGS) -c -o $@ $<

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

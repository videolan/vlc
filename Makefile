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
CC=egcc
SHELL=/bin/sh

# Audio output settings
AUDIO = dsp
# Not yet supported
#AUDIO += esd
# Fallback method that should always work
AUDIO += dummy

# Video output settings
VIDEO = x11
#VIDEO += fb
#VIDEO += ggi
#VIDEO += glide
# Not yet supported
#VIDEO += beos
#VIDEO += dga
# Fallback method that should always work
VIDEO += dummy

# Target architecture
ARCH=X86
#ARCH=PPC
#ARCH=SPARC

# Target operating system
SYS=LINUX
#SYS=BSD
#SYS=BEOS

# For x86 architecture, choose MMX support
MMX=YES
#MMX=NO

# Decoder choice - ?? old decoder will be removed soon
#DECODER=old
DECODER=new

# Debugging mode on or off (set to 1 to activate)
DEBUG=0

#----------------- do not change anything below this line ----------------------

################################################################################
# Configuration pre-processing
################################################################################

# Program version - may only be changed by the project leader
PROGRAM_VERSION = 0.95.0

# audio options
audio := $(shell echo $(AUDIO) | tr 'A-Z' 'a-z')
AUDIO := $(shell echo $(AUDIO) | tr 'a-z' 'A-Z')

# video options
video := $(shell echo $(VIDEO) | tr 'A-Z' 'a-z')
VIDEO := $(shell echo $(VIDEO) | tr 'a-z' 'A-Z')

# PROGRAM_OPTIONS is an identification string of the compilation options
PROGRAM_OPTIONS = $(ARCH) $(SYS)
ifeq ($(DEBUG),1)
PROGRAM_OPTIONS += DEBUG
endif

# PROGRAM_BUILD is a complete identification of the build
PROGRAM_BUILD = `date -R` $(USER)@`hostname`

# DEFINE will contain some of the constants definitions decided in Makefile, 
# including ARCH_xx and SYS_xx. It will be passed to C compiler.
DEFINE += -DARCH_$(ARCH)
DEFINE += -DSYS_$(SYS)
DEFINE += -DAUDIO_OPTIONS="\"$(audio)\""
DEFINE += -DVIDEO_OPTIONS="\"$(video)\""
DEFINE += -DPROGRAM_VERSION="\"$(PROGRAM_VERSION)\""
DEFINE += -DPROGRAM_OPTIONS="\"$(PROGRAM_OPTIONS)\""
DEFINE += -DPROGRAM_BUILD="\"$(PROGRAM_BUILD)\""
ifeq ($(DEBUG),1)
DEFINE += -DDEBUG
endif

################################################################################
# Tuning and other variables - do not change anything except if you know
# exactly what you are doing
################################################################################

#
# C headers directories
#
INCLUDE += -Iinclude

#
# Libraries
#
LIB += -lpthread
LIB += -lm
LIB += -ldl

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

# Optimizations for x86 familiy
ifeq ($(ARCH),X86)
CCFLAGS += -malign-double
CCFLAGS += -march=pentiumpro
#CCFLAGS += -march=pentium
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

# Eventual MMX optimizations for x86
ifeq ($(ARCH),X86)
ifeq ($(MMX), YES)
CFLAGS += -DHAVE_MMX
endif
endif

#
# Additionnal debugging flags
#

# Debugging support
ifeq ($(DEBUG),1)
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
						interface/intf_console.o

input_obj =         		input/input_vlan.o \
						input/input_file.o \
						input/input_netlist.o \
						input/input_network.o \
						input/input_ctrl.o \
						input/input_pcr.o \
						input/input_psi.o \
						input/input.o

audio_output_obj = 		audio_output/audio_output.o

video_output_obj = 		video_output/video_output.o \
						video_output/video_text.o \
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

spu_decoder_obj =		spu_decoder/spu_decoder.o

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
						video_parser/vpar_synchro.o \
						video_parser/video_fifo.o

video_decoder_obj =		video_decoder/video_decoder.o \
						video_decoder/vdec_motion.o \
			                        video_decoder/vdec_idct.o
endif

misc_obj =			misc/mtime.o \
						misc/rsc_files.o \
						misc/netutils.o \
						misc/decoder_fifo.o

C_OBJ = $(interface_obj) \
		$(input_obj) \
		$(audio_output_obj) \
		$(video_output_obj) \
		$(ac3_decoder_obj) \
		$(audio_decoder_obj) \
		$(spu_decoder_obj) \
		$(generic_decoder_obj) \
		$(video_parser_obj) \
		$(video_decoder_obj) \
		$(vlan_obj) \
		$(misc_obj)

#
# Assembler Objects
# 
ifeq ($(ARCH),X86)
ifeq ($(MMX), YES)
ifeq ($(DECODER),new)
ASM_OBJ = 			video_decoder/vdec_idctmmx.o \
						video_output/video_yuv_mmx.o
else
ASM_OBJ = 			video_decoder_ref/vdec_idctmmx.o \
						video_output/video_yuv_mmx.o
endif
endif
endif

#
# Plugins
#
interface_plugin =	$(video:%=interface/intf_%.so)
audio_plugin =		$(audio:%=audio_output/aout_%.so)
video_plugin = 		$(video:%=video_output/vout_%.so)

PLUGIN_OBJ = $(interface_plugin) \
                $(audio_plugin) \
                $(video_plugin) \

#
# Other lists of files
#
sources := $(C_OBJ:%.o=%.c) $(PLUGIN_OBJ:%.so=%.c)
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
	rm -f $(C_OBJ) $(ASM_OBJ) $(PLUGIN_OBJ)

distclean: clean
	rm -f **/*.o **/*.so **/*~ *.log
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
vlc: $(C_OBJ) $(ASM_OBJ) $(PLUGIN_OBJ)
	$(CC) $(LCFLAGS) $(CFLAGS) --export-dynamic -rdynamic -o $@ $(C_OBJ) $(ASM_OBJ)	

#
# Generic rules (see below)
#
$(dependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f Makefile.dep $@

$(C_OBJ): %.o: Makefile.dep
$(C_OBJ): %.o: dep/%.d
$(C_OBJ): %.o: %.c
	@echo "compiling $*.o from $*.c"
	@$(CC) $(CCFLAGS) $(CFLAGS) -c -o $@ $<

$(ASM_OBJ): %.o: Makefile.dep
$(ASM_OBJ): %.o: %.S
	@echo "assembling $*.o from $*.S"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(PLUGIN_OBJ): %.so: Makefile.dep
$(PLUGIN_OBJ): %.so: dep/%.d

# audio plugins
audio_output/aout_dummy.so \
	audio_output/aout_dsp.so: %.so: %.c
		@echo "compiling $*.so from $*.c"
		@$(CC) $(CCFLAGS) $(CFLAGS) -shared -o $@ $<

audio_output/aout_esd.so: %.so: %.c
		@echo "compiling $*.so from $*.c"
		@$(CC) $(CCFLAGS) $(CFLAGS) -laudiofile -lesd -shared -o $@ $<

# video plugins
interface/intf_dummy.so \
	video_output/vout_dummy.so \
	interface/intf_fb.so \
	video_output/vout_fb.so: %.so: %.c
		@echo "compiling $*.so from $*.c"
		@$(CC) $(CCFLAGS) $(CFLAGS) -shared -o $@ $<

interface/intf_x11.so \
	video_output/vout_x11.so: %.so: %.c
		@echo "compiling $*.so from $*.c"
		@$(CC) $(CCFLAGS) $(CFLAGS) -I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -lXext -shared -o $@ $<

interface/intf_glide.so \
	video_output/vout_glide.so: %.so: %.c
		@echo "compiling $*.so from $*.c"
		@$(CC) $(CCFLAGS) $(CFLAGS) -I/usr/include/glide -lglide2x -shared -o $@ $<

interface/intf_ggi.so \
	video_output/vout_ggi.so: %.so: %.c
		@echo "compiling $*.so from $*.c"
		@$(CC) $(CCFLAGS) $(CFLAGS) -lggi -shared -o $@ $<


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

###############################################################################
# vlc (VideoLAN Client) main Makefile - (c)1998 VideoLAN
###############################################################################

-include Makefile.opts

###############################################################################
# Objects and files
###############################################################################

# 
# All possible plugin directories, needed for make clean
#
PLUGINS_DIR :=	alsa beos darwin dsp dummy \
		dvd esd fb ggi glide gtk \
		downmix idct imdct \
		macosx mga \
		motion \
		mpeg null qt sdl \
		text x11 yuv

#
# All possible plugin objects
#
PLUGINS_TARGETS := alsa/alsa beos/beos darwin/darwin dsp/dsp dummy/dummy \
		dvd/dvd esd/esd fb/fb ggi/ggi glide/glide gtk/gnome gtk/gtk \
		downmix/downmix downmix/downmixsse downmix/downmix3dn \
		idct/idct idct/idctclassic idct/idctmmx idct/idctmmxext \
		imdct/imdct imdct/imdct3dn imdct/imdctsse \
		macosx/macosx mga/mga \
		motion/motion motion/motionmmx motion/motionmmxext \
		mpeg/es mpeg/ps mpeg/ts null/null qt/qt sdl/sdl \
		text/ncurses text/rc x11/x11 x11/xvideo yuv/yuv yuv/yuvmmx

#
# C Objects
# 
INTERFACE =	src/interface/main.o \
		src/interface/interface.o \
		src/interface/intf_msg.o \
		src/interface/intf_playlist.o \
		src/interface/intf_channels.o \
		src/interface/intf_urldecode.o \

INPUT =		src/input/input.o \
		src/input/input_ext-dec.o \
		src/input/input_ext-intf.o \
		src/input/input_dec.o \
		src/input/input_programs.o \
		src/input/input_netlist.o \
		src/input/input_clock.o \
		src/input/mpeg_system.o

AUDIO_OUTPUT = 	src/audio_output/audio_output.o \
		src/audio_output/aout_ext-dec.o \
		src/audio_output/aout_u8.o \
		src/audio_output/aout_s8.o \
		src/audio_output/aout_u16.o \
		src/audio_output/aout_s16.o \
	        src/audio_output/aout_spdif.o

VIDEO_OUTPUT = 	src/video_output/video_output.o \
		src/video_output/video_text.o \
		src/video_output/video_spu.o \
		src/video_output/video_yuv.o

AC3_DECODER =	src/ac3_decoder/ac3_decoder_thread.o \
		src/ac3_decoder/ac3_decoder.o \
		src/ac3_decoder/ac3_parse.o \
		src/ac3_decoder/ac3_exponent.o \
		src/ac3_decoder/ac3_bit_allocate.o \
		src/ac3_decoder/ac3_mantissa.o \
		src/ac3_decoder/ac3_rematrix.o \
		src/ac3_decoder/ac3_imdct.o

AC3_SPDIF = src/ac3_spdif/ac3_spdif.o \
	        src/ac3_spdif/ac3_iec958.o

LPCM_DECODER =	src/lpcm_decoder/lpcm_decoder_thread.o

AUDIO_DECODER =	src/audio_decoder/audio_decoder.o \
		src/audio_decoder/adec_generic.o \
		src/audio_decoder/adec_layer1.o \
		src/audio_decoder/adec_layer2.o \
		src/audio_decoder/adec_math.o

SPU_DECODER =	src/spu_decoder/spu_decoder.o

#GEN_DECODER =	src/generic_decoder/generic_decoder.o

VIDEO_PARSER = 	src/video_parser/video_parser.o \
		src/video_parser/vpar_headers.o \
		src/video_parser/vpar_blocks.o \
		src/video_parser/vpar_synchro.o \
		src/video_parser/video_fifo.o

VIDEO_DECODER =	src/video_decoder/video_decoder.o

MISC =		src/misc/mtime.o \
		src/misc/tests.o \
		src/misc/modules.o \
		src/misc/netutils.o

C_OBJ =		$(INTERFACE) \
		$(INPUT) \
		$(VIDEO_OUTPUT) \
		$(AUDIO_OUTPUT) \
		$(AC3_DECODER) \
        $(AC3_SPDIF) \
		$(LPCM_DECODER) \
		$(AUDIO_DECODER) \
		$(SPU_DECODER) \
		$(GEN_DECODER) \
		$(VIDEO_PARSER) \
		$(VIDEO_DECODER) \
		$(MISC)

#
# Misc Objects
# 
ifeq ($(GETOPT),1)
C_OBJ += extras/GNUgetopt/getopt.o extras/GNUgetopt/getopt1.o 
endif

ifeq ($(SYS),beos)
CPP_OBJ =	src/misc/beos_specific.o
endif

ifneq (,$(findstring darwin,$(SYS)))
C_OBJ +=	src/misc/darwin_specific.o
endif

#
# Generated header
#
H_OBJ =		include/modules_builtin.h

#
# Other lists of files
#
C_DEP := $(C_OBJ:%.o=.dep/%.d)
CPP_DEP := $(CPP_OBJ:%.o=.dep/%.dpp)

#
# Translate plugin names
#
ifneq (,$(PLUGINS))
PLUGIN_OBJ := $(shell for i in $(PLUGINS) ; do echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.*/\('$$i'\) .*@lib/\1.so@' -e 's@^ .*@@' ; done)
endif
ifneq (,$(BUILTINS))
BUILTIN_OBJ := $(shell for i in $(BUILTINS) ; do echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.*/\('$$i'\) .*@lib/\1.a@' -e 's@^ .*@@' ; done)
endif

# All symbols must be exported
export

###############################################################################
# Targets
###############################################################################

#
# Virtual targets
#
all: vlc ${ALIASES} plugins vlc.app

clean:
	for dir in $(PLUGINS_DIR) ; do \
		( cd plugins/$${dir} && $(MAKE) clean ) ; done
	rm -f plugins/*/*.o plugins/*/*.moc plugins/*/*.bak
	rm -f $(C_OBJ) $(CPP_OBJ)
	rm -f src/*/*.o extras/*/*.o
	rm -f lib/*.so lib/*.a vlc gnome-vlc gvlc kvlc qvlc
	rm -Rf vlc.app

distclean: clean
	rm -f **/*.o **/*~ *.log
	rm -f Makefile.opts
	rm -f include/defs.h include/config.h include/modules_builtin.h
	rm -f config*status config*cache config*log
	rm -f gmon.out core build-stamp
	rm -Rf .dep
	rm -f .gdb_history

install:
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) vlc $(DESTDIR)$(bindir)
ifneq (,$(ALIASES))
	for alias in $(ALIASES) ; do if test $$alias ; then rm -f $(DESTDIR)$(bindir)/$$alias && ln -s vlc $(DESTDIR)$(bindir)/$$alias ; fi ; done
endif
	mkdir -p $(DESTDIR)$(libdir)/videolan/vlc
ifneq (,$(PLUGINS))
	$(INSTALL) -m 644 $(PLUGINS:%=lib/%.so) $(DESTDIR)$(libdir)/videolan/vlc
endif
	mkdir -p $(DESTDIR)$(datadir)/videolan
	$(INSTALL) -m 644 share/*.psf $(DESTDIR)$(datadir)/videolan
	$(INSTALL) -m 644 share/*.png $(DESTDIR)$(datadir)/videolan
	$(INSTALL) -m 644 share/*.xpm $(DESTDIR)$(datadir)/videolan

show:
	@echo CC: $(CC)
	@echo CFLAGS: $(CFLAGS)
	@echo DCFLAGS: $(DCFLAGS)
	@echo LCFLAGS: $(LCFLAGS)
	@echo C_OBJ: $(C_OBJ)
	@echo CPP_OBJ: $(CPP_OBJ)
	@echo objects: $(objects)
	@echo cppobjects: $(cppobjects)
	@echo PLUGIN_OBJ: $(PLUGIN_OBJ)
	@echo BUILTIN_OBJ: $(BUILTIN_OBJ)


# ugliest of all, but I have no time to do it -- sam
snapshot: Makefile.opts
	rm -Rf /tmp/vlc-${PROGRAM_VERSION}* /tmp/vlc-${PROGRAM_VERSION}nocss*
	# copy archive in /tmp
	find -type d | grep -v CVS | grep -v '\.dep' | while read i ; \
		do mkdir -p /tmp/vlc-${PROGRAM_VERSION}/$$i ; \
	done
	find debian -mindepth 1 -maxdepth 1 -type d | \
		while read i ; do rm -Rf /tmp/vlc-${PROGRAM_VERSION}/$$i ; done
	# .c .h .in .cpp .glade
	find include src plugins -type f -name '*.[chig]*' | while read i ; \
		do cp $$i /tmp/vlc-${PROGRAM_VERSION}/$$i ; done
	# Makefiles
	find . plugins -type f -name Makefile | while read i ; \
		do cp $$i /tmp/vlc-${PROGRAM_VERSION}/$$i ; done
	# extra files
	cp -a extras/* /tmp/vlc-${PROGRAM_VERSION}/extras
	cp -a doc/* /tmp/vlc-${PROGRAM_VERSION}/doc
	find /tmp/vlc-${PROGRAM_VERSION}/extras \
		/tmp/vlc-${PROGRAM_VERSION}/doc \
		-type d -name CVS | while read i ; \
			do rm -Rf $$i ; \
		done
	# copy misc files
	cp vlc.spec AUTHORS COPYING ChangeLog INSTALL README TODO todo.pl \
		Makefile.opts.in Makefile.dep Makefile.modules \
		configure configure.in install-sh config.sub config.guess \
			/tmp/vlc-${PROGRAM_VERSION}/
	for file in control control-css vlc-gtk.menu vlc.copyright vlc.docs \
		changelog changelog-css rules rules-css vlc.dirs vlc.desktop \
		gvlc.desktop gnome-vlc.desktop vlc.menu ; do \
			cp debian/$$file /tmp/vlc-${PROGRAM_VERSION}/debian/ ; \
		done
	for file in default8x16.psf default8x9.psf vlc_beos.rsrc vlc.icns ; do \
		cp share/$$file /tmp/vlc-${PROGRAM_VERSION}/share/ ; done
	for icon in vlc gvlc qvlc gnome-vlc kvlc ; do \
		cp share/$$icon.xpm share/$$icon.png \
			/tmp/vlc-${PROGRAM_VERSION}/share/ ; done

	# build css-enabled archives
	(cd /tmp ; tar cf vlc-${PROGRAM_VERSION}.tar vlc-${PROGRAM_VERSION} ; \
		bzip2 -f -9 < vlc-${PROGRAM_VERSION}.tar \
			> vlc-${PROGRAM_VERSION}.tar.bz2 ; \
		gzip -f -9 vlc-${PROGRAM_VERSION}.tar )
	mv /tmp/vlc-${PROGRAM_VERSION}.tar.gz \
		/tmp/vlc-${PROGRAM_VERSION}.tar.bz2 ..

	# clean up
	rm -Rf /tmp/vlc-${PROGRAM_VERSION}*

.PHONY: vlc.app
vlc.app:
ifneq (,$(findstring darwin,$(SYS)))
	rm -Rf vlc.app
	mkdir -p vlc.app/Contents/Resources
	mkdir -p vlc.app/Contents/MacOS/lib
	mkdir -p vlc.app/Contents/MacOS/share
	$(INSTALL) -m 644 extras/MacOSX_app/Contents/Info.plist vlc.app/Contents/
	$(INSTALL) -m 644 extras/MacOSX_app/Contents/PkgInfo vlc.app/Contents/
	$(INSTALL) vlc vlc.app/Contents/MacOS/
	$(INSTALL) share/vlc.icns vlc.app/Contents/Resources/
ifneq (,$(PLUGINS))
	$(INSTALL) $(PLUGINS:%=lib/%.so) vlc.app/Contents/MacOS/lib
endif
	$(INSTALL) -m 644 share/*.psf vlc.app/Contents/MacOS/share
endif

FORCE:

#
# GTK/Gnome aliases - don't add too many aliases which could bloat
# the namespace
#
gnome-vlc gvlc kvlc qvlc: vlc
	rm -f $@ && ln -s vlc $@

#
# Generic rules (see below)
#
$(C_DEP): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f Makefile.dep $@

$(CPP_DEP): %.dpp: FORCE
	@$(MAKE) -s --no-print-directory -f Makefile.dep $@

$(H_OBJ): Makefile.opts Makefile.dep Makefile
	rm -f $@ && cp $@.in $@
ifneq (,$(BUILTINS))
	for i in $(BUILTINS) ; do \
		echo "int module_"$$i"_InitModule( module_t* );" >> $@ ; \
		echo "int module_"$$i"_ActivateModule( module_t* );" >> $@ ; \
		echo "int module_"$$i"_DeactivateModule( module_t* );" >> $@ ; \
	done
endif
	echo "" >> $@ ;
	printf "#define ALLOCATE_ALL_BUILTINS() do { " >> $@ ;
ifneq (,$(BUILTINS))
	for i in $(BUILTINS) ; do \
		printf "ALLOCATE_BUILTIN("$$i"); " >> $@ ; \
	done
endif
	echo "} while( 0 );" >> $@ ;
	echo "" >> $@ ;

$(C_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(C_OBJ): %.o: .dep/%.d
$(C_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(CPP_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(CPP_OBJ): %.o: .dep/%.dpp
$(CPP_OBJ): %.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

#
# Main application target
#
vlc: Makefile.opts Makefile.dep Makefile $(H_OBJ) $(C_OBJ) $(CPP_OBJ) $(BUILTIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $(C_OBJ) $(CPP_OBJ) $(BUILTIN_OBJ) $(LCFLAGS)
ifeq ($(SYS),beos)
	rm -f ./lib/_APP_
	ln -s ../vlc ./lib/_APP_
	xres -o $@ ./share/vlc_beos.rsrc
	mimeset -f $@
endif

#
# Plugins target
#
plugins: Makefile.modules Makefile.opts Makefile.dep Makefile $(PLUGIN_OBJ)
$(PLUGIN_OBJ): FORCE
	cd $(shell echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.* \([^/]*/\)'$(@:lib/%.so=%)' .*@plugins/\1@' -e 's@^ .*@@') && $(MAKE) $(@:%=../../%)

#
# Built-in modules target
#
builtins: Makefile.modules Makefile.opts Makefile.dep Makefile $(BUILTIN_OBJ)
$(BUILTIN_OBJ): FORCE
	cd $(shell echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.* \([^/]*/\)'$(@:lib/%.a=%)' .*@plugins/\1@' -e 's@^ .*@@') && $(MAKE) $(@:%=../../%)


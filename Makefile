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
PLUGINS_DIR :=	alsa beos darwin directx dsp dummy dvd esd fb ggi glide gtk downmix idct imdct macosx mga motion mpeg qt sdl text x11 yuv

#
# All possible plugin objects
#
PLUGINS_TARGETS := alsa/alsa beos/beos darwin/darwin directx/directx dsp/dsp dummy/dummy dummy/null dvd/dvd esd/esd fb/fb ggi/ggi glide/glide gtk/gnome gtk/gtk downmix/downmix downmix/downmixsse downmix/downmix3dn idct/idct idct/idctclassic idct/idctmmx idct/idctmmxext imdct/imdct imdct/imdct3dn imdct/imdctsse macosx/macosx mga/mga motion/motion motion/motionmmx motion/motionmmxext mpeg/es mpeg/ps mpeg/ts qt/qt sdl/sdl text/ncurses text/rc x11/x11 x11/xvideo yuv/yuv yuv/yuvmmx

#
# C Objects
# 
INTERFACE := main interface intf_msg intf_playlist intf_channels
INPUT := input input_ext-dec input_ext-intf input_dec input_programs input_netlist input_clock mpeg_system
VIDEO_OUTPUT := video_output video_text video_spu video_yuv
AUDIO_OUTPUT := audio_output aout_ext-dec aout_u8 aout_s8 aout_u16 aout_s16 aout_spdif
AC3_DECODER := ac3_decoder_thread ac3_decoder ac3_parse ac3_exponent ac3_bit_allocate ac3_mantissa ac3_rematrix ac3_imdct
AC3_SPDIF := ac3_spdif ac3_iec958
LPCM_DECODER := lpcm_decoder_thread
AUDIO_DECODER := audio_decoder adec_generic adec_layer1 adec_layer2 adec_math
SPU_DECODER := spu_decoder
#GEN_DECODER := generic_decoder
VIDEO_PARSER := video_parser vpar_headers vpar_blocks vpar_synchro video_fifo
VIDEO_DECODER := video_decoder
MISC := mtime tests modules netutils

C_OBJ :=	$(INTERFACE:%=src/interface/%.o) \
		$(INPUT:%=src/input/%.o) \
		$(VIDEO_OUTPUT:%=src/video_output/%.o) \
		$(AUDIO_OUTPUT:%=src/audio_output/%.o) \
		$(AC3_DECODER:%=src/ac3_decoder/%.o) \
		$(AC3_SPDIF:%=src/ac3_spdif/%.o) \
		$(LPCM_DECODER:%=src/lpcm_decoder/%.o) \
		$(AUDIO_DECODER:%=src/audio_decoder/%.o) \
		$(SPU_DECODER:%=src/spu_decoder/%.o) \
		$(GEN_DECODER:%=src/generic_decoder/%.o) \
		$(VIDEO_PARSER:%=src/video_parser/%.o) \
		$(VIDEO_DECODER:%=src/video_decoder/%.o) \
		$(MISC:%=src/misc/%.o)

#
# Misc Objects
# 
ifeq ($(GETOPT),1)
C_OBJ += extras/GNUgetopt/getopt.o extras/GNUgetopt/getopt1.o 
endif

ifeq ($(SYS),beos)
CPP_OBJ :=	src/misc/beos_specific.o
endif

ifneq (,$(findstring darwin,$(SYS)))
C_OBJ +=	src/misc/darwin_specific.o
endif

ifneq (,$(findstring mingw32,$(SYS)))
RESOURCE_OBJ :=	share/vlc_win32_rc.o
endif

#
# Generated header
#
H_OBJ :=	src/misc/modules_builtin.h

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

clean: libdvdcss-clean plugins-clean vlc-clean
	rm -f src/*/*.o extras/*/*.o
	rm -f lib/*.so lib/*.so.* lib/*.a

libdvdcss-clean:
	cd extras/libdvdcss && $(MAKE) clean

plugins-clean:
	for dir in $(PLUGINS_DIR) ; do \
		( cd plugins/$${dir} && $(MAKE) clean ) ; done
	rm -f plugins/*/*.o plugins/*/*.moc plugins/*/*.bak

vlc-clean:
	rm -f $(C_OBJ) $(CPP_OBJ)
	rm -f vlc gnome-vlc gvlc kvlc qvlc
	rm -Rf vlc.app

distclean: clean
	rm -f **/*.o **/*~ *.log
	rm -f Makefile.opts
	rm -f include/defs.h include/config.h include/modules_builtin.h
	rm -f src/misc/modules_builtin.h
	rm -f config*status config*cache config*log
	rm -f gmon.out core build-stamp
	rm -Rf .dep
	rm -f .gdb_history

install: libdvdcss-install vlc-install plugins-install

vlc-install:
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) vlc $(DESTDIR)$(bindir)
ifneq (,$(ALIASES))
	for alias in $(ALIASES) ; do if test $$alias ; then rm -f $(DESTDIR)$(bindir)/$$alias && ln -s vlc $(DESTDIR)$(bindir)/$$alias ; fi ; done
endif
	mkdir -p $(DESTDIR)$(datadir)/videolan
	$(INSTALL) -m 644 share/*.psf $(DESTDIR)$(datadir)/videolan
	$(INSTALL) -m 644 share/*.png $(DESTDIR)$(datadir)/videolan
	$(INSTALL) -m 644 share/*.xpm $(DESTDIR)$(datadir)/videolan

plugins-install:
	mkdir -p $(DESTDIR)$(libdir)/videolan/vlc
ifneq (,$(PLUGINS))
	$(INSTALL) -m 644 $(PLUGINS:%=lib/%.so) $(DESTDIR)$(libdir)/videolan/vlc
endif

libdvdcss-install:
	cd extras/libdvdcss && $(MAKE) install

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
snapshot: clean Makefile.opts
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
	cp vlc.spec AUTHORS COPYING ChangeLog INSTALL INSTALL.libdvdcss \
		README TODO todo.pl \
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

libdvdcss-snapshot: clean Makefile.opts
	rm -Rf /tmp/libdvdcss-${LIBDVDCSS_VERSION}* \
		/tmp/libdvdcss-${LIBDVDCSS_VERSION}nocss*
	# copy archive in /tmp
	find include extras doc lib -type d | grep -v CVS | grep -v '\.dep' | \
		while read i ; do \
			mkdir -p /tmp/libdvdcss-${LIBDVDCSS_VERSION}/$$i ; \
		done
	# .c .h .in .cpp .glade
	find include extras -type f -name '*.[chig]*' | while read i ; \
		do cp $$i /tmp/libdvdcss-${LIBDVDCSS_VERSION}/$$i ; done
	# Makefiles
	sed -e 's#^install:#install-unused:#' \
		-e 's#^clean:#clean-unused:#' \
		-e 's#^all:.*#all: libdvdcss#' \
		-e 's#^libdvdcss-install:#install:#' \
		-e 's#^libdvdcss-clean:#clean:#' \
		< Makefile > /tmp/libdvdcss-${LIBDVDCSS_VERSION}/Makefile
	# extra files
	cp -a extras/* /tmp/libdvdcss-${LIBDVDCSS_VERSION}/extras
	cp -a doc/* /tmp/libdvdcss-${LIBDVDCSS_VERSION}/doc
	find /tmp/libdvdcss-${LIBDVDCSS_VERSION}/extras \
		/tmp/libdvdcss-${LIBDVDCSS_VERSION}/doc \
		-type d -name CVS | while read i ; \
			do rm -Rf $$i ; \
		done
	# copy misc files
	cp AUTHORS COPYING ChangeLog INSTALL INSTALL.libdvdcss README \
		TODO todo.pl Makefile.opts.in Makefile.dep Makefile.modules \
		configure configure.in install-sh config.sub config.guess \
			/tmp/libdvdcss-${LIBDVDCSS_VERSION}/

	# build css-enabled archives
	(cd /tmp ; tar cf libdvdcss-${LIBDVDCSS_VERSION}.tar \
		libdvdcss-${LIBDVDCSS_VERSION} ; \
		bzip2 -f -9 < libdvdcss-${LIBDVDCSS_VERSION}.tar \
			> libdvdcss-${LIBDVDCSS_VERSION}.tar.bz2 ; \
		gzip -f -9 libdvdcss-${LIBDVDCSS_VERSION}.tar )
	mv /tmp/libdvdcss-${LIBDVDCSS_VERSION}.tar.gz \
		/tmp/libdvdcss-${LIBDVDCSS_VERSION}.tar.bz2 ..

	# clean up
	rm -Rf /tmp/libdvdcss-${LIBDVDCSS_VERSION}*

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
ifneq (,$(BUILTINS))
	echo "" >> $@ ;
	printf "#define ALLOCATE_ALL_BUILTINS() do { " >> $@ ;
	for i in $(BUILTINS) ; do \
		printf "ALLOCATE_BUILTIN("$$i"); " >> $@ ; \
	done
	echo "} while( 0 );" >> $@ ;
endif
	echo "" >> $@ ;

$(C_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(C_OBJ): %.o: .dep/%.d
$(C_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(CPP_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(CPP_OBJ): %.o: .dep/%.dpp
$(CPP_OBJ): %.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

$(RESOURCE_OBJ): %.o: Makefile.dep Makefile
ifneq (,(findstring mingw32,$(SYS)))
$(RESOURCE_OBJ): %.o: %.rc
	$(WINDRES) -i $< -o $@
endif

#
# Main application target
#
vlc: Makefile.opts Makefile.dep Makefile $(H_OBJ) $(C_OBJ) $(CPP_OBJ) $(BUILTIN_OBJ) $(RESOURCE_OBJ)
	$(CC) $(CFLAGS) -o $@ $(C_OBJ) $(CPP_OBJ) $(BUILTIN_OBJ) $(RESOURCE_OBJ) $(LCFLAGS) $(LIB)
ifeq ($(SYS),beos)
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

#
# libdvdcss target
#
libdvdcss:
	cd extras/libdvdcss && $(MAKE)


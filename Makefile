###############################################################################
# vlc (VideoLAN Client) main Makefile - (c)1998 VideoLAN
###############################################################################

ifeq ($(shell [ ! -r Makefile.opts ] && echo 1),)
    include Makefile.opts
endif

###############################################################################
# Objects and files
###############################################################################

# 
# All possible plugin directories, needed for make clean
#
PLUGINS_DIR :=	alsa \
		beos \
		darwin \
		directx \
		dsp \
		dummy \
		dvd \
		esd \
		fb \
		ggi \
		glide \
		gtk \
		downmix \
		idct \
		imdct \
		kde \
		macosx \
		mga \
		motion \
		mpeg \
		qt \
		sdl \
		text \
		vcd \
		x11 \
		yuv

#
# All possible plugin objects
#
PLUGINS_TARGETS := alsa/alsa \
		beos/beos \
		darwin/darwin \
		directx/directx \
		dsp/dsp \
		dummy/dummy \
		dummy/null \
		dvd/dvd \
		esd/esd \
		fb/fb \
		ggi/ggi \
		glide/glide \
		gtk/gnome \
		gtk/gtk \
		downmix/downmix \
		downmix/downmixsse \
		downmix/downmix3dn \
		idct/idct \
		idct/idctclassic \
		idct/idctmmx \
		idct/idctmmxext \
		idct/idctaltivec \
		imdct/imdct \
		imdct/imdct3dn \
		imdct/imdctsse \
		kde/kde \
		macosx/macosx \
		macosx/macosx_qt \
		mga/mga \
		motion/motion \
		motion/motionmmx \
		motion/motionmmxext \
		motion/motion3dnow \
		motion/motionaltivec \
		mpeg/es \
		mpeg/ps \
		mpeg/ts \
		qt/qt \
		sdl/sdl \
		text/ncurses \
		text/rc \
		vcd/vcd \
		x11/x11 \
		x11/xvideo \
		yuv/yuv \
		yuv/yuvmmx

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
VIDEO_DECODER := video_parser vpar_headers vpar_blocks vpar_synchro vpar_pool video_decoder
MISC := mtime tests modules netutils iso_lang

C_OBJ :=	$(INTERFACE:%=src/interface/%.o) \
		$(INPUT:%=src/input/%.o) \
		$(VIDEO_OUTPUT:%=src/video_output/%.o) \
		$(AUDIO_OUTPUT:%=src/audio_output/%.o) \
		$(AC3_DECODER:%=src/ac3_decoder/%.o) \
		$(AC3_SPDIF:%=src/ac3_spdif/%.o) \
		$(LPCM_DECODER:%=src/lpcm_decoder/%.o) \
		$(AUDIO_DECODER:%=src/audio_decoder/%.o) \
		$(SPU_DECODER:%=src/spu_decoder/%.o) \
		$(VIDEO_DECODER:%=src/video_decoder/%.o) \
		$(MISC:%=src/misc/%.o)

#
# Misc Objects
# 
ifeq ($(NEED_GETOPT),1)
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

VLC_OBJ := $(C_OBJ) $(CPP_OBJ) $(BUILTIN_OBJ) $(RESOURCE_OBJ)

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
PLUGIN_OBJ := $(shell for i in $(PLUGINS) ; do echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.*/\('$$i'\) .*@plugins/\1.so@' -e 's@^ .*@@' ; done)
endif
ifneq (,$(BUILTINS))
BUILTIN_OBJ := $(shell for i in $(BUILTINS) ; do echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.*/\('$$i'\) .*@plugins/\1.a@' -e 's@^ .*@@' ; done)
endif

#
# Misc variables
#
VLC_QUICKVERSION := $(shell grep '^ *VLC_VERSION=' configure.in | cut -f2 -d=)
LIBDVDCSS_QUICKVERSION := $(shell grep '^ *LIBDVDCSS_VERSION=' configure.in | cut -f2 -d=)


# All symbols must be exported
export

###############################################################################
# Targets
###############################################################################

#
# Virtual targets
#
all: Makefile.opts vlc ${ALIASES} vlc.app

Makefile.opts:
	@echo "**** No configuration found, running ./configure..."
	./configure
	$(MAKE) $(MAKECMDGOALS)
	exit

show:
	@echo CC: $(CC)
	@echo CFLAGS: $(CFLAGS)
	@echo DCFLAGS: $(DCFLAGS)
	@echo LCFLAGS: $(LCFLAGS)
	@echo PCFLAGS: $(PCFLAGS)
	@echo PLCFLAGS: $(PLCFLAGS)
	@echo C_OBJ: $(C_OBJ)
	@echo CPP_OBJ: $(CPP_OBJ)
	@echo PLUGIN_OBJ: $(PLUGIN_OBJ)
	@echo BUILTIN_OBJ: $(BUILTIN_OBJ)

#
# Cleaning rules
#
clean: libdvdcss-clean plugins-clean vlc-clean
	rm -f src/*/*.o extras/*/*.o
	rm -f lib/*.so* lib/*.a
	rm -f plugins/*.so plugins/*.a
	rm -rf extras/MacOSX/build

libdvdcss-clean:
	-cd extras/libdvdcss && $(MAKE) clean

plugins-clean:
	for dir in $(PLUGINS_DIR) ; do \
		( cd plugins/$${dir} && $(MAKE) clean ) ; done
	rm -f plugins/*/*.o plugins/*/*.moc plugins/*/*.bak

vlc-clean:
	rm -f $(C_OBJ) $(CPP_OBJ)
	rm -f vlc gnome-vlc gvlc kvlc qvlc vlc.exe
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

#
# Install/uninstall rules
#
install: libdvdcss-install vlc-install plugins-install

uninstall: libdvdcss-uninstall vlc-uninstall plugins-uninstall

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

vlc-uninstall:
	rm -f $(DESTDIR)$(bindir)/vlc
ifneq (,$(ALIASES))
	for alias in $(ALIASES) ; do if test $$alias ; then rm -f $(DESTDIR)$(bindir)/$$alias ; fi ; done
endif
	rm -f $(DESTDIR)$(datadir)/videolan/*.psf
	rm -f $(DESTDIR)$(datadir)/videolan/*.png
	rm -f $(DESTDIR)$(datadir)/videolan/*.xpm

plugins-install:
	mkdir -p $(DESTDIR)$(libdir)/videolan/vlc
ifneq (,$(PLUGINS))
	$(INSTALL) -m 644 $(PLUGINS:%=plugins/%.so) $(DESTDIR)$(libdir)/videolan/vlc
endif

plugins-uninstall:
	rm -f $(DESTDIR)$(libdir)/videolan/vlc/*.so

libdvdcss-install:
	-cd extras/libdvdcss && $(MAKE) install

libdvdcss-uninstall:
	-cd extras/libdvdcss && $(MAKE) uninstall

#
# Package generation rules
#
snapshot-common:
	# Check that tmp isn't in the way
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi
	# Copy directory structure in tmp
	find -type d | grep -v '\(\.dep\|snapshot\|CVS\)' | while read i ; \
		do mkdir -p tmp/vlc/$$i ; \
	done
	rm -Rf tmp/vlc/tmp
	find debian -mindepth 1 -maxdepth 1 -type d | \
		while read i ; do rm -Rf tmp/vlc/$$i ; done
	# Copy .c .h .in .cpp and .glade files
	find include src plugins -type f -name '*.[chig]*' | while read i ; \
		do cp $$i tmp/vlc/$$i ; done
	# Copy plugin Makefiles
	find plugins -type f -name Makefile | while read i ; \
		do cp $$i tmp/vlc/$$i ; done
	# Copy extra programs and documentation
	cp -a extras/* tmp/vlc/extras
	cp -a doc/* tmp/vlc/doc
	find tmp/vlc/extras tmp/vlc/doc \
		-type d -name CVS -o -name '.*' -o -name '*.[o]' | \
			while read i ; do rm -Rf $$i ; done
	# Copy misc files
	cp vlc.spec AUTHORS COPYING TODO todo.pl ChangeLog* README* INSTALL* \
		Makefile Makefile.opts.in Makefile.dep Makefile.modules \
		configure configure.in install-sh config.sub config.guess \
			tmp/vlc/
	# Copy Debian control files
	for file in debian/*dirs debian/*docs debian/*menu debian/*desktop \
		debian/*copyright ; do cp $$file tmp/vlc/debian ; done
	for file in control changelog rules ; do \
		cp debian/$$file tmp/vlc/debian/ ; done
	# Copy fonts and icons
	for file in share/*png share/*xpm share/*psf ; do \
		cp $$file tmp/vlc/share ; done
	for file in vlc_beos.rsrc vlc.icns gvlc_win32.ico vlc_win32_rc.rc ; do \
			cp share/$$file tmp/vlc/share/ ; done

snapshot: snapshot-common
	# Build archives
	F=vlc-${VLC_QUICKVERSION}; \
	mv tmp/vlc tmp/$$F; (cd tmp ; tar cf $$F.tar $$F); \
	bzip2 -f -9 < tmp/$$F.tar > $$F.tar.bz2; \
	gzip -f -9 tmp/$$F.tar ; mv tmp/$$F.tar.gz .
	# Clean up
	rm -Rf tmp

snapshot-nocss: snapshot-common
	# Remove libdvdcss
	rm -Rf tmp/vlc/extras/libdvdcss
	rm -f tmp/vlc/*.libdvdcss
	# Fix debian information
	rm -f tmp/vlc/debian/libdvdcss*
	rm -f tmp/vlc/debian/control
	sed -e 's#^ DVDs# unencrypted DVDs#' < debian/control \
		| awk '{if(gsub("Package: libdvdcss",$$0))a=1;if(a==0)print $$0;if(a==1&&$$0=="")a=0}' \
		> tmp/vlc/debian/control
	rm -f tmp/vlc/debian/rules
	sed -e 's#^\(export LIBDVDCSS_FLAGS=\).*#\1"--without-dvdcss"#' < debian/rules \
		| awk '{if($$0=="# libdvdcss start")a=1;if(a==0)print $$0;if($$0=="# libdvdcss stop")a=0}' \
		> tmp/vlc/debian/rules
	chmod +x tmp/vlc/debian/rules
	# Build css-disabled archives
	F=vlc-${VLC_QUICKVERSION}; G=vlc-${VLC_QUICKVERSION}-nocss; \
	mv tmp/vlc tmp/$$F; (cd tmp ; tar cf $$G.tar $$F); \
	bzip2 -f -9 < tmp/$$G.tar > $$G.tar.bz2; \
	gzip -f -9 tmp/$$G.tar ; mv tmp/$$G.tar.gz .
	# Clean up
	rm -Rf tmp

libdvdcss-snapshot: snapshot-common
	# Remove vlc sources and icons, doc, debian directory...
	rm -Rf tmp/vlc/src tmp/vlc/share tmp/vlc/plugins tmp/vlc/doc
	rm -Rf tmp/vlc/extras/GNUgetopt tmp/vlc/extras/MacOSX
	rm -Rf tmp/vlc/debian
	# Remove useless headers
	rm -f tmp/vlc/include/*
	for file in defs.h.in config.h.in common.h int_types.h ; \
		do cp include/$$file tmp/vlc/include/ ; done
	# Remove misc files (??? - maybe not really needed)
	rm -f tmp/vlc/vlc.spec tmp/vlc/INSTALL-win32.txt
	mv tmp/vlc/INSTALL.libdvdcss tmp/vlc/INSTALL
	mv tmp/vlc/README.libdvdcss tmp/vlc/README
	mv tmp/vlc/ChangeLog.libdvdcss tmp/vlc/ChangeLog
	# Fix Makefile
	rm -f tmp/vlc/Makefile
	sed -e 's#^install:#install-unused:#' \
		-e 's#^uninstall:#uninstall-unused:#' \
		-e 's#^clean:#clean-unused:#' \
		-e 's#^all:.*#all: libdvdcss#' \
		-e 's#^libdvdcss-install:#install:#' \
		-e 's#^libdvdcss-uninstall:#uninstall:#' \
		-e 's#^libdvdcss-clean:#clean:#' \
		< Makefile > tmp/vlc/Makefile
	# Build archives
	F=libdvdcss-${LIBDVDCSS_QUICKVERSION}; \
	mv tmp/vlc tmp/$$F; (cd tmp ; tar cf $$F.tar $$F); \
	bzip2 -f -9 < tmp/$$F.tar > $$F.tar.bz2; \
	gzip -f -9 tmp/$$F.tar ; mv tmp/$$F.tar.gz .
	# Clean up
	rm -Rf tmp

deb:
	dpkg-buildpackage -rfakeroot -us -uc

#
# Gtk/Gnome/* aliases and OS X application
#
gnome-vlc gvlc kvlc qvlc: vlc
	rm -f $@ && ln -s vlc $@

.PHONY: vlc.app
vlc.app: Makefile.opts
ifneq (,$(findstring darwin,$(SYS)))
	rm -Rf vlc.app
	cd extras/MacOSX ; pbxbuild | grep -v '^ ' | grep -v '^\t'
	cp -r extras/MacOSX/build/vlc.bundle ./vlc.app
	$(INSTALL) -d vlc vlc.app/Contents/MacOS/share
	$(INSTALL) -d vlc vlc.app/Contents/MacOS/plugins
	$(INSTALL) vlc vlc.app/Contents/MacOS/
ifneq (,$(PLUGINS))
	$(INSTALL) $(PLUGINS:%=plugins/%.so) vlc.app/Contents/MacOS/plugins
endif
	$(INSTALL) -m 644 share/*.psf vlc.app/Contents/MacOS/share
endif

FORCE:

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
	echo "" >> $@ ;
	printf "#define ALLOCATE_ALL_BUILTINS() do { " >> $@ ;
	for i in $(BUILTINS) ; do \
		printf "ALLOCATE_BUILTIN("$$i"); " >> $@ ; \
	done
	echo "} while( 0 );" >> $@ ;
	echo "" >> $@ ;
endif

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
vlc: Makefile.opts Makefile.dep Makefile $(H_OBJ) $(VLC_OBJ) $(BUILTIN_OBJ) plugins
	$(CC) $(CFLAGS) -o $@ $(VLC_OBJ) $(BUILTIN_OBJ) $(LCFLAGS)
ifeq ($(SYS),beos)
	xres -o $@ ./share/vlc_beos.rsrc
	mimeset -f $@
endif

#
# Plugins target
#
plugins: Makefile.modules Makefile.opts Makefile.dep Makefile $(PLUGIN_OBJ)
$(PLUGIN_OBJ): FORCE
	cd $(shell echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.* \([^/]*/\)'$(@:plugins/%.so=%)' .*@plugins/\1@' -e 's@^ .*@@') && $(MAKE) $(@:plugins/%=../%)

#
# Built-in modules target
#
builtins: Makefile.modules Makefile.opts Makefile.dep Makefile $(BUILTIN_OBJ)
$(BUILTIN_OBJ): FORCE
	cd $(shell echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.* \([^/]*/\)'$(@:plugins/%.a=%)' .*@plugins/\1@' -e 's@^ .*@@') && $(MAKE) $(@:plugins/%=../%)

#
# libdvdcss target
#
libdvdcss: Makefile.opts
	cd extras/libdvdcss && $(MAKE)

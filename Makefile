###############################################################################
# vlc (VideoLAN Client) main Makefile - (c)1998 VideoLAN
###############################################################################

ifeq ($(shell [ ! -r Makefile.opts ] && echo 1),)
    include Makefile.opts
endif
ifeq ($(shell [ ! -r Makefile.config ] && echo 1),)
    include Makefile.config
endif

###############################################################################
# Objects and files
###############################################################################

# 
# All possible plugin directories, needed for make clean
#
PLUGINS_DIR :=	a52 \
		aa \
		ac3_adec \
		ac3_spdif \
		access \
		alsa \
		arts \
		avi \
		beos \
		chroma \
		directx \
		downmix \
		dsp \
		dummy \
		dvd \
		dvdread \
		esd \
		fb \
		ffmpeg \
		filter \
		fx \
		ggi \
		glide \
		gtk \
		idct \
		imdct \
		kde \
		lirc \
		lpcm_adec \
		macosx \
		mad \
		memcpy \
		mga \
		motion \
		mpeg_system \
		mpeg_adec \
		mpeg_vdec \
		network \
		ogg \
		qnx \
		qt \
		satellite \
		sdl \
		spudec \
		text \
		vcd \
		win32 \
		x11

PLUGINS_TARGETS := a52/a52 \
		aa/aa \
		ac3_adec/ac3_adec \
		ac3_spdif/ac3_spdif \
		access/file \
		access/udp \
		access/http \
		alsa/alsa \
		arts/arts \
		avi/avi \
		beos/beos \
		chroma/chroma_i420_rgb \
		chroma/chroma_i420_rgb_mmx \
		chroma/chroma_i420_yuy2 \
		chroma/chroma_i420_yuy2_mmx \
		chroma/chroma_i422_yuy2 \
		chroma/chroma_i422_yuy2_mmx \
		chroma/chroma_i420_ymga \
		chroma/chroma_i420_ymga_mmx \
		directx/directx \
		downmix/downmix \
		downmix/downmixsse \
		downmix/downmix3dn \
		dsp/dsp \
		dummy/dummy \
		dummy/null \
		dvd/dvd \
		dvdread/dvdread \
		esd/esd \
		fb/fb \
		ffmpeg/ffmpeg \
		filter/filter_deinterlace \
		filter/filter_transform \
		filter/filter_invert \
		filter/filter_distort \
		filter/filter_wall \
		filter/filter_clone \
		filter/filter_crop \
		fx/fx_scope \
		ggi/ggi \
		glide/glide \
		gtk/gnome \
		gtk/gtk \
		idct/idct \
		idct/idctclassic \
		idct/idctmmx \
		idct/idctmmxext \
		idct/idctaltivec \
		imdct/imdct \
		imdct/imdct3dn \
		imdct/imdctsse \
		kde/kde \
		lirc/lirc \
		lpcm_adec/lpcm_adec \
		macosx/macosx \
		mad/mad \
		memcpy/memcpy \
		memcpy/memcpymmx \
		memcpy/memcpymmxext \
		memcpy/memcpy3dn \
		memcpy/memcpyaltivec \
		mga/mga \
		mga/xmga \
		motion/motion \
		motion/motionmmx \
		motion/motionmmxext \
		motion/motion3dnow \
		motion/motionaltivec \
        mpeg_system/mpeg_audio \
		mpeg_system/mpeg_es \
		mpeg_system/mpeg_ps \
		mpeg_system/mpeg_ts \
		mpeg_system/mpeg_ts_dvbpsi \
		mpeg_adec/mpeg_adec \
		mpeg_vdec/mpeg_vdec \
		network/ipv4 \
		network/ipv6 \
		ogg/vorbis \
		qnx/qnx \
		qt/qt \
		satellite/satellite \
		sdl/sdl \
		spudec/spudec \
		text/logger \
		text/ncurses \
		text/rc \
		vcd/vcd \
		win32/waveout \
		win32/intfwin \
		x11/x11 \
		x11/xvideo

#
# C Objects
# 
INTERFACE := main interface intf_msg intf_playlist intf_eject
INPUT := input input_ext-plugins input_ext-dec input_ext-intf input_dec input_programs input_clock mpeg_system
VIDEO_OUTPUT := video_output video_text vout_pictures vout_subpictures
AUDIO_OUTPUT := audio_output aout_ext-dec aout_pcm aout_spdif
MISC := mtime modules configuration netutils iso_lang

C_OBJ :=	$(INTERFACE:%=src/interface/%.o) \
		$(INPUT:%=src/input/%.o) \
		$(VIDEO_OUTPUT:%=src/video_output/%.o) \
		$(AUDIO_OUTPUT:%=src/audio_output/%.o) \
		$(MISC:%=src/misc/%.o)

#
# Misc Objects
# 
ifeq ($(NEED_GETOPT),1)
C_OBJ += extras/GNUgetopt/getopt.o extras/GNUgetopt/getopt1.o 
endif

ifeq ($(NEED_SYMBOLS),1)
C_OBJ += src/misc/symbols.o
endif

ifeq ($(SYS),beos)
CPP_OBJ :=	src/misc/beos_specific.o
endif

ifneq (,$(findstring darwin,$(SYS)))
C_OBJ +=	src/misc/darwin_specific.o
endif

ifneq (,$(findstring mingw32,$(SYS)))
C_OBJ +=	src/misc/win32_specific.o
RESOURCE_OBJ :=	share/vlc_win32_rc.o
endif

VLC_OBJ := $(C_OBJ) $(CPP_OBJ) $(M_OBJ) $(BUILTIN_OBJ) $(RESOURCE_OBJ)

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
VERSION := $(shell grep '^ *VERSION=' configure.in | head -1 | sed 's/"//g' | cut -f2 -d=)

# All symbols must be exported
export

###############################################################################
# Targets
###############################################################################

#
# Virtual targets
#
all: Makefile.opts vlc ${ALIASES} vlc.app plugins po

Makefile.opts:
	@echo "**** No configuration found, please run ./configure"
	@exit 1
#	./configure
#	$(MAKE) $(MAKECMDGOALS)
#	exit	

show:
	@echo CC: $(CC)
	@echo CFLAGS: $(CFLAGS)
	@echo LDFLAGS: $(LDFLAGS)
	@echo plugins_CFLAGS: $(plugins_CFLAGS)
	@echo plugins_LDFLAGS: $(plugins_LDFLAGS)
	@echo builtins_CFLAGS: $(builtins_CFLAGS)
	@echo builtins_LDFLAGS: $(builtins_LDFLAGS)
	@echo C_OBJ: $(C_OBJ)
	@echo CPP_OBJ: $(CPP_OBJ)
	@echo PLUGIN_OBJ: $(PLUGIN_OBJ)
	@echo BUILTIN_OBJ: $(BUILTIN_OBJ)

#
# Cleaning rules
#
clean: plugins-clean po-clean vlc-clean
	rm -f src/*/*.o extras/*/*.o
	rm -f lib/*.so* lib/*.a
	rm -f plugins/*.so plugins/*.a plugins/*.lib plugins/*.tds
	rm -rf extras/MacOSX/build

po-clean:
	-cd po && $(MAKE) clean

plugins-clean:
	for dir in $(PLUGINS_DIR) ; do \
		( cd plugins/$${dir} \
			&& $(MAKE) -f ../../Makefile.modules clean ) ; done
	rm -f plugins/*/*.o plugins/*/*.lo plugins/*/*.moc plugins/*/*.bak

vlc-clean:
	rm -f $(C_OBJ) $(CPP_OBJ)
	rm -f vlc gnome-vlc gvlc kvlc qvlc vlc.exe
	rm -Rf vlc.app

distclean: clean
	-cd po && $(MAKE) maintainer-clean
	rm -f **/*.o **/*~ *.log
	rm -f Makefile.opts Makefile.config
	rm -f include/defs.h include/modules_builtin.h
	rm -f src/misc/modules_builtin.h
	rm -f config*status config*cache config*log conftest*
	rm -f gmon.out core build-stamp
	rm -Rf .dep
	rm -f .gdb_history

#
# Install/uninstall rules
#
install: vlc-install plugins-install po-install

uninstall: vlc-uninstall plugins-uninstall po-uninstall

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

po-install:
	-cd po && $(MAKE) install

po-uninstall:
	-cd po && $(MAKE) uninstall

#
# Package generation rules
#
dist:
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
	# Copy .c .h .in .cpp .m and .glade files
	find include src plugins -type f -name '*.[bcdhigmrst]*' | while read i ; \
		do cp $$i tmp/vlc/$$i ; done
	# Grmbl... special case...
	for i in API BUGS DESIGN TODO ; \
		do cp plugins/mad/$$i tmp/vlc/plugins/mad ; done
	# Copy plugin Makefiles
	find plugins -type f -name Makefile | while read i ; \
		do cp $$i tmp/vlc/$$i ; done
	# Copy extra programs and documentation
	cp -a extras/* tmp/vlc/extras
	cp -a doc/* tmp/vlc/doc
	find tmp/vlc/extras tmp/vlc/doc \
		-type d -name CVS -o -name '.*' -o -name '*.[o]' | \
			while read i ; do rm -Rf $$i ; done
	# Copy gettext stuff
	cp po/ChangeLog po/vlc.pot po/*.po tmp/vlc/po
	for i in Makefile.in.in POTFILES.in ; do cp po/$$i tmp/vlc/po ; done
	# Copy misc files
	cp FAQ AUTHORS COPYING TODO todo.pl ChangeLog* README* INSTALL* \
		ABOUT-NLS BUGS plugins/LIST vlc.spec \
		Makefile Makefile.*.in Makefile.dep Makefile.modules \
		configure configure.in install-sh install-win32 macosx-dmg \
		config.sub config.guess aclocal.m4 mkinstalldirs \
			tmp/vlc/
	# Copy Debian control files
	for file in debian/*dirs debian/*docs debian/*menu debian/*desktop \
		debian/*copyright ; do cp $$file tmp/vlc/debian ; done
	for file in control changelog rules ; do \
		cp debian/$$file tmp/vlc/debian/ ; done
	# Copy ipkg control files
	for file in control rules patch ; do \
		cp ipkg/$$file tmp/vlc/ipkg/ ; done
	# Copy fonts and icons
	for file in share/*vlc* share/*psf; do \
		cp $$file tmp/vlc/share ; done
	# Build archives
	F=vlc-${VERSION}; \
	mv tmp/vlc tmp/$$F; (cd tmp ; tar czf ../$$F.tar.gz $$F); \
	# Clean up
	rm -Rf tmp

package-win32:
	# XXX: this rule is probably only useful to you if you have exactly
	# the same setup as me. Contact sam@zoy.org if you need to use it.
	#
	# Check that tmp isn't in the way
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi
	# Create installation script
	cp install-win32 tmp/nsi
	# Copy relevant files
	cp vlc.exe tmp/ 
	$(STRIP) tmp/vlc.exe
	cp INSTALL.win32 tmp/INSTALL.txt ; unix2dos tmp/INSTALL.txt
	for file in AUTHORS COPYING ChangeLog README FAQ TODO ; \
			do cp $$file tmp/$${file}.txt ; \
			unix2dos tmp/$${file}.txt ; done
	mkdir tmp/plugins
	cp $(PLUGINS:%=plugins/%.so) tmp/plugins/ 
	# don't include these two
	#rm -f tmp/plugins/gtk.so tmp/plugins/sdl.so
ifneq (,$(PLUGINS))
	for i in $(PLUGINS) ; do if test $$i != intfwin ; then $(STRIP) tmp/plugins/$$i.so ; fi ; done
endif
	mkdir tmp/share
	for file in default8x16.psf default8x9.psf ; \
		do cp share/$$file tmp/share/ ; done
	# Create package 
	wine ~/.wine/fake_windows/Program\ Files/NSIS/makensis.exe -- /DVERSION=${VERSION} /CD tmp/nsi
	# Clean up
	rm -Rf tmp

package-beos:
	# Check that tmp isn't in the way
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi
	
	# Create dir
	mkdir -p tmp/vlc/share
	# Copy relevant files
	cp vlc tmp/vlc/
	strip tmp/vlc/vlc
	cp AUTHORS COPYING ChangeLog README FAQ TODO tmp/vlc/
	for file in default8x16.psf default8x9.psf ; \
		do cp share/$$file tmp/vlc/share/ ; done
	mkdir tmp/vlc/plugins
	cp $(PLUGINS:%=plugins/%.so) tmp/vlc/plugins/ 
	strip $(PLUGINS:%=tmp/vlc/plugins/%.so)
	# Create package 
	mv tmp/vlc tmp/vlc-${VERSION}
	(cd tmp ; find vlc-${VERSION} | \
	zip -9 -@ vlc-${VERSION}-BeOS-x86.zip )
	mv tmp/vlc-${VERSION}-BeOS-x86.zip .
	# Clean up
	rm -Rf tmp

package-macosx:
	# Check that tmp isn't in the way
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi

	# Copy relevant files 
	cp -R vlc.app tmp/
	cp AUTHORS COPYING ChangeLog README README.MacOSX.rtf FAQ TODO tmp/

	# Create disk image 
	./macosx-dmg 0 "vlc-${VERSION}" tmp/* 

	# Clean up
	rm -Rf tmp

#
# Gtk/Gnome/* aliases and OS X application
#
gnome-vlc gvlc kvlc qvlc: vlc
	rm -f $@ && ln -s vlc $@

.PHONY: vlc.app
vlc.app: vlc plugins
ifneq (,$(findstring darwin,$(SYS)))
	rm -Rf vlc.app
	cd extras/MacOSX ; pbxbuild | grep -v '^ ' | grep -v '^\t' | grep -v "^$$"
	cp -r extras/MacOSX/build/vlc.bundle ./vlc.app
	$(INSTALL) -d vlc.app/Contents/MacOS/share
	$(INSTALL) -d vlc.app/Contents/MacOS/plugins
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
$(H_OBJ): Makefile.opts Makefile.dep Makefile
#	@echo "regenerating $@"
	@rm -f $@ && cp $@.in $@
ifneq (,$(BUILTINS))
	@for i in $(BUILTINS) ; do \
		echo "int InitModule__MODULE_"$$i"( module_t* );" >>$@; \
		echo "int ActivateModule__MODULE_"$$i"( module_t* );" >>$@; \
		echo "int DeactivateModule__MODULE_"$$i"( module_t* );" >>$@; \
	done
	@echo "" >> $@ ;
endif
	@echo "#define ALLOCATE_ALL_BUILTINS() \\" >> $@ ;
	@echo "    do \\" >> $@ ;
	@echo "    { \\" >> $@ ;
ifneq (,$(BUILTINS))
	@for i in $(BUILTINS) ; do \
		echo "        ALLOCATE_BUILTIN("$$i"); \\" >> $@ ; \
	done
endif
	@echo "    } while( 0 );" >> $@ ;
	@echo "" >> $@ ;

$(C_DEP): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f Makefile.dep $@

$(CPP_DEP): %.dpp: FORCE
	@$(MAKE) -s --no-print-directory -f Makefile.dep $@

$(C_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(C_OBJ): %.o: $(H_OBJ)
$(C_OBJ): %.o: .dep/%.d
$(C_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(vlc_CFLAGS) -c -o $@ $<

$(CPP_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(CPP_OBJ): %.o: $(H_OBJ)
$(CPP_OBJ): %.o: .dep/%.dpp
$(CPP_OBJ): %.o: %.cpp
	$(CC) $(CFLAGS) $(vlc_CFLAGS) -c -o $@ $<

$(M_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(M_OBJ): %.o: $(H_OBJ)
$(M_OBJ): %.o: .dep/%.dm
$(M_OBJ): %.o: %.m
	$(CC) $(CFLAGS) $(vlc_CFLAGS) -c -o $@ $<

$(RESOURCE_OBJ): %.o: Makefile.dep Makefile
ifneq (,(findstring mingw32,$(SYS)))
$(RESOURCE_OBJ): %.o: %.rc
	$(WINDRES) -i $< -o $@
endif

#
# Main application target
#
vlc: Makefile.config Makefile.opts Makefile.dep Makefile $(VLC_OBJ) $(BUILTIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $(VLC_OBJ) $(BUILTIN_OBJ) $(LDFLAGS) $(vlc_LDFLAGS) $(builtins_LDFLAGS)
ifeq ($(SYS),beos)
	xres -o $@ ./share/vlc_beos.rsrc
	mimeset -f $@
endif

#
# Plugins target
#
plugins: Makefile.modules Makefile.opts Makefile.dep Makefile $(PLUGIN_OBJ)
$(PLUGIN_OBJ): FORCE
	@cd $(shell echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.* \([^/]*/\)'$(@:plugins/%.so=%)' .*@plugins/\1@' -e 's@^ .*@@') && $(MAKE) -f ../../Makefile.modules $(@:plugins/%=../%)

#
# Built-in modules target
#
builtins: Makefile.modules Makefile.opts Makefile.dep Makefile $(BUILTIN_OBJ)
$(BUILTIN_OBJ): FORCE
	@cd $(shell echo " "$(PLUGINS_TARGETS)" " | sed -e 's@.* \([^/]*/\)'$(@:plugins/%.a=%)' .*@plugins/\1@' -e 's@^ .*@@') && $(MAKE) -f ../../Makefile.modules $(@:plugins/%=../%)

#
# gettext target
#
po: FORCE
	@cd po && $(MAKE)


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
PLUGINS_DIR :=	a52 \
		aa \
		ac3_adec \
		ac3_spdif \
		access \
		alsa \
		arts \
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
		filter/filter_deinterlace \
		filter/filter_transform \
		filter/filter_invert \
		filter/filter_distort \
		filter/filter_wall \
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
		mga/mga \
		mga/xmga \
		motion/motion \
		motion/motionmmx \
		motion/motionmmxext \
		motion/motion3dnow \
		motion/motionaltivec \
		mpeg_system/mpeg_es \
		mpeg_system/mpeg_ps \
		mpeg_system/mpeg_ts \
		mpeg_adec/mpeg_adec \
		mpeg_vdec/mpeg_vdec \
		network/ipv4 \
		network/ipv6 \
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
		win32/win32 \
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
VLC_QUICKVERSION := $(shell grep '^ *VLC_VERSION=' configure.in | head -1 | sed 's/"//g' | cut -f2 -d=)
LIBDVDCSS_QUICKVERSION := $(shell grep '^ *LIBDVDCSS_VERSION=' configure.in | head -1 | sed 's/"//g' | cut -f2 -d=)


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
	@echo DCFLAGS: $(DCFLAGS)
	@echo LDFLAGS: $(LDFLAGS)
	@echo PCFLAGS: $(PCFLAGS)
	@echo PLDFLAGS: $(PLDFLAGS)
	@echo C_OBJ: $(C_OBJ)
	@echo CPP_OBJ: $(CPP_OBJ)
	@echo PLUGIN_OBJ: $(PLUGIN_OBJ)
	@echo BUILTIN_OBJ: $(BUILTIN_OBJ)

#
# Cleaning rules
#
clean: libdvdcss-clean libdvdread-clean plugins-clean po-clean vlc-clean
	rm -f src/*/*.o extras/*/*.o
	rm -f lib/*.so* lib/*.a
	rm -f plugins/*.so plugins/*.a
	rm -rf extras/MacOSX/build

libdvdcss-clean:
	-cd extras/libdvdcss && $(MAKE) clean

po-clean:
	-cd po && $(MAKE) clean

libdvdread-clean:
	-cd extras/libdvdread && $(MAKE) clean

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
	rm -f Makefile.opts
	rm -f include/defs.h include/modules_builtin.h
	rm -f src/misc/modules_builtin.h
	rm -f config*status config*cache config*log
	rm -f gmon.out core build-stamp
	rm -Rf .dep
	rm -f .gdb_history

#
# Install/uninstall rules
#
install: libdvdcss-install vlc-install plugins-install po-install

uninstall: libdvdcss-uninstall vlc-uninstall plugins-uninstall po-uninstall

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

libdvdread-install:
	-cd extras/libdvdread && $(MAKE) install

libdvdread-uninstall:
	-cd extras/libdvdread && $(MAKE) uninstall

po-install:
	-cd po && $(MAKE) install

po-uninstall:
	-cd po && $(MAKE) uninstall

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
	# Copy gettext stuff
	cp po/*.po tmp/vlc/po
	for i in Makefile.in.in POTFILES.in ; do cp po/$$i tmp/vlc/po ; done
	# Copy misc files
	cp FAQ AUTHORS COPYING TODO todo.pl ChangeLog* README* INSTALL* \
		Makefile Makefile.opts.in Makefile.dep Makefile.modules \
		configure configure.in install-sh install-win32 vlc.spec \
		config.sub config.guess acconfig.h aclocal.m4 mkinstalldirs \
			tmp/vlc/
	# Copy Debian control files
	for file in debian/*dirs debian/*docs debian/*menu debian/*desktop \
		debian/*copyright ; do cp $$file tmp/vlc/debian ; done
	for file in control changelog rules ; do \
		cp debian/$$file tmp/vlc/debian/ ; done
	# Copy ipkg control files
	for file in control rules ; do \
		cp ipkg/$$file tmp/vlc/ipkg/ ; done
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
	sed -e 's#@VERSION@#'${VLC_QUICKVERSION}'#' < install-win32 > tmp/nsi
	# Copy relevant files
	cp vlc.exe $(PLUGINS:%=plugins/%.so) tmp/ 
	cp INSTALL.win32 tmp/INSTALL.txt ; unix2dos tmp/INSTALL.txt
	for file in AUTHORS COPYING ChangeLog ChangeLog.libdvdcss \
		README README.libdvdcss FAQ TODO ; \
			do cp $$file tmp/$${file}.txt ; \
			unix2dos tmp/$${file}.txt ; done
	for file in iconv.dll libgmodule-1.3-12.dll libgtk-0.dll libgdk-0.dll \
		libgobject-1.3-12.dll libintl-1.dll libglib-1.3-12.dll \
		libgthread-1.3-12.dll SDL.dll README-SDL.txt ; \
			do cp ${DLL_PATH}/$$file tmp/ ; done
	mkdir tmp/share
	for file in default8x16.psf default8x9.psf ; \
		do cp share/$$file tmp/share/ ; done
	# Create package 
	wine ~/.wine/fake_windows/Program\ Files/NSIS/makensis.exe /CD tmp/nsi
	mv tmp/vlc-${VLC_QUICKVERSION}.exe \
		vlc-${VLC_QUICKVERSION}-win32-installer.exe
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
	cp AUTHORS COPYING ChangeLog ChangeLog.libdvdcss \
		README README.libdvdcss FAQ TODO tmp/vlc/
	for file in default8x16.psf default8x9.psf ; \
		do cp share/$$file tmp/vlc/share/ ; done
	# Create package 
	mv tmp/vlc tmp/vlc-${VLC_QUICKVERSION}
	(cd tmp ; find vlc-${VLC_QUICKVERSION} | \
	zip -9 -@ vlc-${VLC_QUICKVERSION}-beos.zip )
	mv tmp/vlc-${VLC_QUICKVERSION}-BeOS-x86.zip .
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
	cp AUTHORS COPYING ChangeLog ChangeLog.libdvdcss \
		README README.libdvdcss FAQ TODO tmp/

	# Create disk image 
	./macosx-dmg 0 "vlc-${VLC_QUICKVERSION}" tmp/* 

	# Clean up
	rm -Rf tmp

libdvdcss-snapshot: snapshot-common
	# Remove vlc sources and icons, doc, debian directory...
	rm -Rf tmp/vlc/src tmp/vlc/share tmp/vlc/plugins tmp/vlc/doc
	rm -Rf tmp/vlc/extras/GNUgetopt tmp/vlc/extras/MacOSX
	rm -Rf tmp/vlc/debian
	rm -Rf tmp/vlc/ipkg
	# Remove useless headers
	rm -f tmp/vlc/include/*
	for file in defs.h.in config.h common.h int_types.h ; \
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
vlc.app: vlc plugins
ifneq (,$(findstring darwin,$(SYS)))
	rm -Rf vlc.app
	cd extras/MacOSX ; pbxbuild | grep -v '^ ' | grep -v '^\t'
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
	$(CC) $(CFLAGS) $(CFLAGS_VLC) -c -o $@ $<

$(CPP_OBJ): %.o: Makefile.opts Makefile.dep Makefile
$(CPP_OBJ): %.o: $(H_OBJ)
$(CPP_OBJ): %.o: .dep/%.dpp
$(CPP_OBJ): %.o: %.cpp
	$(CC) $(CFLAGS) $(CFLAGS_VLC) -c -o $@ $<

$(RESOURCE_OBJ): %.o: Makefile.dep Makefile
ifneq (,(findstring mingw32,$(SYS)))
$(RESOURCE_OBJ): %.o: %.rc
	$(WINDRES) -i $< -o $@
endif

#
# Main application target
#
vlc: Makefile.opts Makefile.dep Makefile $(VLC_OBJ) $(BUILTIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $(VLC_OBJ) $(BUILTIN_OBJ) $(LDFLAGS) $(LIB_VLC) $(LIB_BUILTINS) $(LIB_COMMON)
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
# libdvdcss target
#
libdvdcss: Makefile.opts
	@cd extras/libdvdcss && $(MAKE)

#
# libdvdread target
#
libdvdread: Makefile.opts
	@cd extras/libdvdread && $(MAKE)

#
# gettext target
#
po: FORCE
	@cd po && $(MAKE)


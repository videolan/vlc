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
# C Objects
# 
VLC := vlc
LIBVLC := libvlc
INTERFACE := interface intf_eject
PLAYLIST := playlist
INPUT := input input_ext-plugins input_ext-dec input_ext-intf input_dec input_programs input_clock mpeg_system input_info
VIDEO_OUTPUT := video_output video_text vout_pictures vout_subpictures
AUDIO_OUTPUT := audio_output aout_ext-dec aout_pcm aout_spdif
MISC := mtime modules threads cpu configuration netutils iso_lang messages objects extras

LIBVLC_OBJ :=	$(LIBVLC:%=src/%.o) \
		$(INTERFACE:%=src/interface/%.o) \
		$(PLAYLIST:%=src/playlist/%.o) \
		$(INPUT:%=src/input/%.o) \
		$(VIDEO_OUTPUT:%=src/video_output/%.o) \
		$(AUDIO_OUTPUT:%=src/audio_output/%.o) \
		$(MISC:%=src/misc/%.o)

VLC_OBJ := $(VLC:%=src/%.o)

C_OBJ := $(VLC_OBJ) $(LIBVLC_OBJ)

#
# Misc Objects
# 
ifeq ($(NEED_GETOPT),1)
LIBVLC_OBJ += extras/GNUgetopt/getopt.o extras/GNUgetopt/getopt1.o 
endif

ifeq ($(NEED_SYMBOLS),1)
LIBVLC_OBJ += src/misc/symbols.o
endif

ifeq ($(SYS),beos)
CPP_OBJ :=	src/misc/beos_specific.o
endif

ifneq (,$(findstring darwin,$(SYS)))
LIBVLC_OBJ +=	src/misc/darwin_specific.o
endif

ifneq (,$(findstring mingw32,$(SYS)))
LIBVLC_OBJ +=	src/misc/win32_specific.o
RESOURCE_OBJ :=	share/vlc_win32_rc.o
endif

LIBVLC_OBJ += $(CPP_OBJ) $(M_OBJ) $(BUILTIN_OBJ)
VLC_OBJ += $(RESOURCE_OBJ)

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
PLUGIN_OBJ := $(PLUGINS:%=modules/%.so)
endif
ifneq (,$(BUILTINS))
BUILTIN_OBJ := $(BUILTINS:%=modules/%.a)
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
all: Makefile.opts vlc ${ALIASES} vlc.app plugins po mozilla/libvlcplugin.so

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
clean: plugins-clean po-clean vlc-clean mozilla-clean
	rm -f src/*/*.o extras/*/*.o
	rm -f lib/*.so* lib/*.a
	rm -f plugins/*.so plugins/*.a plugins/*.lib plugins/*.tds
	rm -Rf extras/MacOSX/build

po-clean:
	-cd po && $(MAKE) clean

plugins-clean:
	for dir in $(shell echo $(PLUGIN_OBJ) $(BUILTIN_OBJ) | sed 's@\([^ ]*\)/[^ ]*@\1@g' ) ; do ( PARENT=`pwd` ; cd $${dir} && $(MAKE) -f $$PARENT/Makefile.modules clean PARENT=$$PARENT ) ; done

vlc-clean:
	rm -f $(C_OBJ) $(CPP_OBJ)
	rm -f vlc gnome-vlc gvlc kvlc qvlc vlc.exe
	rm -Rf vlc.app

mozilla-clean:
	-cd mozilla && $(MAKE) clean

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
install: vlc-install plugins-install builtins-install libvlc-install po-install mozilla-install

uninstall: vlc-uninstall plugins-uninstall builtins-uninstall libvlc-uninstall po-uninstall mozilla-uninstall

vlc-install:
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) vlc $(DESTDIR)$(bindir)
ifneq (,$(ALIASES))
	for alias in $(ALIASES) ; do if test $$alias ; then rm -f $(DESTDIR)$(bindir)/$$alias && ln -s vlc $(DESTDIR)$(bindir)/$$alias ; fi ; done
endif
	mkdir -p $(DESTDIR)$(datadir)/vlc
	$(INSTALL) -m 644 share/*.psf $(DESTDIR)$(datadir)/vlc
	$(INSTALL) -m 644 share/*.png $(DESTDIR)$(datadir)/vlc
	$(INSTALL) -m 644 share/*.xpm $(DESTDIR)$(datadir)/vlc

vlc-uninstall:
	rm -f $(DESTDIR)$(bindir)/vlc
ifneq (,$(ALIASES))
	for alias in $(ALIASES) ; do if test $$alias ; then rm -f $(DESTDIR)$(bindir)/$$alias ; fi ; done
endif
	rm -f $(DESTDIR)$(datadir)/vlc/*.psf
	rm -f $(DESTDIR)$(datadir)/vlc/*.png
	rm -f $(DESTDIR)$(datadir)/vlc/*.xpm
	-rmdir $(DESTDIR)$(datadir)/vlc

plugins-install:
	mkdir -p $(DESTDIR)$(libdir)/vlc
ifneq (,$(PLUGINS))
	$(INSTALL) $(PLUGINS:%=modules/%.so) $(DESTDIR)$(libdir)/vlc
endif

plugins-uninstall:
	rm -f $(DESTDIR)$(libdir)/vlc/*.so
	-rmdir $(DESTDIR)$(libdir)/vlc

builtins-install:
	mkdir -p $(DESTDIR)$(libdir)/vlc
ifneq (,$(BUILTINS))
	$(INSTALL) -m 644 $(BUILTINS:%=modules/%.a) $(DESTDIR)$(libdir)/vlc
endif

builtins-uninstall:
	rm -f $(DESTDIR)$(libdir)/vlc/*.a
	-rmdir $(DESTDIR)$(libdir)/vlc

libvlc-install:
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) vlc-config $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(includedir)/vlc
	$(INSTALL) -m 644 include/vlc/*.h $(DESTDIR)$(includedir)/vlc
	mkdir -p $(DESTDIR)$(libdir)
	$(INSTALL) -m 644 lib/libvlc.a $(DESTDIR)$(libdir)

libvlc-uninstall:
	rm -f $(DESTDIR)$(bindir)/vlc-config
	rm -Rf $(DESTDIR)$(includedir)/vlc
	rm -f $(DESTDIR)$(libdir)/libvlc.a

mozilla-install:
ifeq ($(MOZILLA),1)
	-cd mozilla && $(MAKE) install
endif

mozilla-uninstall:
ifeq ($(MOZILLA),1)
	-cd mozilla && $(MAKE) uninstall
endif

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
	find include src modules -type f -name '*.[bcdhigmrst]*' | while read i ; \
		do cp $$i tmp/vlc/$$i ; done
	# Grmbl... special case...
	for i in API BUGS DESIGN TODO ; \
		do cp modules/mad/$$i tmp/vlc/modules/mad ; done
	# Copy plugin Makefiles
	find modules -type f -name Makefile | while read i ; \
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
		ABOUT-NLS BUGS MODULES vlc.spec \
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
	mv tmp/vlc tmp/$$F; (cd tmp ; \
		cd $$F && $(MAKE) distclean && cd .. ; \
		tar czf ../$$F.tar.gz $$F);
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
	mkdir tmp/modules
	cp $(PLUGINS:%=modules/%.so) tmp/modules/ 
	# don't include these two
	#rm -f tmp/modules/gtk.so tmp/modules/sdl.so
ifneq (,$(PLUGINS))
	for i in $(PLUGINS) ; do if test $$i != intfwin ; then $(STRIP) tmp/modules/$$i.so ; fi ; done
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
	xres -o tmp/vlc/vlc ./share/vlc_beos.rsrc
	cp AUTHORS COPYING ChangeLog README FAQ TODO tmp/vlc/
	for file in default8x16.psf default8x9.psf ; \
		do cp share/$$file tmp/vlc/share/ ; done
	mkdir tmp/vlc/modules
	cp $(PLUGINS:%=modules/%.so) tmp/vlc/modules/ 
	strip $(PLUGINS:%=tmp/vlc/modules/%.so)
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
	$(INSTALL) -d vlc.app/Contents/MacOS/modules
	$(INSTALL) vlc vlc.app/Contents/MacOS/
ifneq (,$(PLUGINS))
	$(INSTALL) $(PLUGINS:%=modules/%.so) vlc.app/Contents/MacOS/modules
endif
	$(INSTALL) -m 644 share/*.psf vlc.app/Contents/MacOS/share
endif

FORCE:

#
# Generic rules (see below)
#
src/misc/modules_builtin.h: Makefile.opts Makefile Makefile.config
	@echo "make[$(MAKELEVEL)]: Creating \`$@'"
	@rm -f $@ && cp $@.in $@
ifneq (,$(BUILTINS))
	@for i in $(BUILTINS) ; do \
		echo "int vlc_entry__"`basename $$i`"( module_t* );" >>$@; \
	done
	@echo "" >> $@ ;
endif
	@echo "#define ALLOCATE_ALL_BUILTINS() \\" >> $@ ;
	@echo "    do \\" >> $@ ;
	@echo "    { \\" >> $@ ;
ifneq (,$(BUILTINS))
	@for i in $(BUILTINS) ; do \
		echo "        ALLOCATE_BUILTIN("`basename $$i`"); \\" >> $@ ; \
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
vlc: Makefile.config Makefile.opts Makefile.dep Makefile $(VLC_OBJ) lib/libvlc.a $(BUILTIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $(VLC_OBJ) lib/libvlc.a $(LDFLAGS) $(vlc_LDFLAGS) $(BUILTIN_OBJ) $(builtins_LDFLAGS)
ifeq ($(SYS),beos)
	xres -o $@ ./share/vlc_beos.rsrc
	mimeset -f $@
endif

#
# Main library target
#
lib/libvlc.a: Makefile.opts Makefile.dep Makefile $(LIBVLC_OBJ)
	rm -f $@
	ar rc $@ $(LIBVLC_OBJ)
	$(RANLIB) $@

#
# DO NOT DISTRIBUTE SHARED VERSIONS OF LIBVLC UNTIL THE ABI IS STABLE
# OR BURN IN HELL -- Sam
#
#lib/libvlc.so: Makefile.opts Makefile.dep Makefile $(LIBVLC_OBJ)
#	$(CC) -shared $(LIBVLC_OBJ) $(LDFLAGS) $(vlc_LDFLAGS) -o $@

builtins: Makefile.modules Makefile.opts Makefile.dep Makefile $(BUILTIN_OBJ)
plugins: Makefile.modules Makefile.opts Makefile.dep Makefile $(PLUGIN_OBJ)

modules/%.a modules/%.so: $(H_OBJ) FORCE
	@cd $(shell echo $@ | sed -e 's@\(.*\)/.*@\1@') && $(MAKE) -f $(shell echo $@ | sed -e 's@[^/]*/@../@g' -e 's@\(.*\)/.*@\1@')/Makefile.modules $(shell echo $@ | sed -e 's@.*/@@') PARENT=$(shell echo $@ | sed -e 's@[^/]*/@../@g' -e 's@\(.*\)/.*@\1@')

#
# Mozilla plugin target
#
mozilla/libvlcplugin.so: FORCE
ifeq ($(MOZILLA),1)
	@cd mozilla && $(MAKE) builtins_LDFLAGS="$(builtins_LDFLAGS)"
endif

#
# gettext target
#
po: FORCE
	@cd po && $(MAKE)


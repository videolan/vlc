# gettext
GETTEXT_VERSION := 0.22.5
GETTEXT_URL := $(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz

ifndef HAVE_WINSTORE # FIXME uses sys/socket.h improperly
PKGS += gettext
endif
ifneq ($(filter gnu%,$(subst -, ,$(HOST))),)
# GNU platform should have gettext (?)
PKGS_FOUND += gettext
endif

$(TARBALLS)/gettext-$(GETTEXT_VERSION).tar.gz:
	$(call download_pkg,$(GETTEXT_URL),gettext)

.sum-gettext: gettext-$(GETTEXT_VERSION).tar.gz

GETTEXT_TOOLS_DIRS := gettext-runtime/src gettext-tools/src

gettext: gettext-$(GETTEXT_VERSION).tar.gz .sum-gettext
	$(UNPACK)
	$(call update_autoconfig,build-aux)
	# disable libtextstyle
	sed -i.orig -e 's,gettext-runtime libtextstyle gettext-tools,gettext-runtime gettext-tools,g' $(UNPACK_DIR)/configure
	sed -i.orig -e 's,gettext-runtime libtextstyle gettext-tools,gettext-runtime gettext-tools,g' $(UNPACK_DIR)/Makefile.in
	sed -i.orig -e 's,ENABLE_COLOR 1,ENABLE_COLOR 0,g' $(UNPACK_DIR)/gettext-tools/src/write-catalog.c
	# disable gettext-tools examples configure
	sed -i.orig -e "s,ac_subdirs_all='examples',ac_subdirs_all=," $(UNPACK_DIR)/gettext-tools/configure
	sed -i.orig -e 's, examples",",' $(UNPACK_DIR)/gettext-tools/configure
	# disable gettext-tools tests/samples
	sed -i.orig -e 's,tests system-tests gnulib-tests examples doc,,' $(UNPACK_DIR)/gettext-tools/Makefile.in
	# disable useless gettext-runtime targets
	sed -i.orig -e 's,doc ,,' $(UNPACK_DIR)/gettext-runtime/Makefile.in
	sed -i.orig -e 's,po man m4 tests,,' $(UNPACK_DIR)/gettext-runtime/Makefile.in
	sed -i.orig -e 's,doc ,,' $(UNPACK_DIR)/gettext-runtime/Makefile.in
ifdef HAVE_CROSS_COMPILE
	# disable cross-compiled command line tools that can't be run
	sed -i.orig -e 's,install-binPROGRAMS install-exec-local,,' $(UNPACK_DIR)/gettext-tools/src/Makefile.in
	for subdir in $(GETTEXT_TOOLS_DIRS); do \
	    sed -i.orig -e 's,^bin_PROGRAMS = ,bin_PROGRAMS_disabled = ,g' $(UNPACK_DIR)/$$subdir/Makefile.in && \
	    sed -i.orig -e 's,^noinst_PROGRAMS = ,noinst_PROGRAMS_disabled = ,g' $(UNPACK_DIR)/$$subdir/Makefile.in; done
endif
	$(MOVE)

DEPS_gettext = iconv $(DEPS_iconv) libxml2 $(DEPS_libxml2)

GETTEXT_CONF = \
	--disable-relocatable \
	--disable-java \
	--disable-native-java \
	--disable-csharp \
	--without-emacs \
	--without-included-libxml \
	--with-installed-libtextstyle \
	--without-libtextstyle-prefix \
	--without-git

ifdef HAVE_WIN32
GETTEXT_CONF += --disable-threads
endif
ifdef HAVE_LINUX
GETTEXT_CONF += --disable-libasprintf
endif
ifdef HAVE_MINGW_W64
GETTEXT_CONF += --disable-libasprintf
endif

.gettext: gettext
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(GETTEXT_CONF)
	+$(MAKEBUILD) -C gettext-runtime
ifndef HAVE_ANDROID
	# build libgettextpo first so we can use its textstyle.h and unistd.h (fsync)
	+$(MAKEBUILD) -C gettext-tools -C libgettextpo
	cd $(BUILD_DIR) && cp gettext-tools/libgettextpo/textstyle.h gettext-tools/src/textstyle.h
	cd $(BUILD_DIR) && cp gettext-tools/libgettextpo/unistd.h    gettext-tools/src/unistd.h
	+$(MAKEBUILD) -C gettext-tools
	+$(MAKEBUILD) -C gettext-tools install
else
	# Android 32bits does not have localeconv
	+$(MAKEBUILD) -C gettext-tools/misc
	+$(MAKEBUILD) -C gettext-tools/m4
	+$(MAKEBUILD) -C gettext-tools/misc install
	+$(MAKEBUILD) -C gettext-tools/m4 install
endif
	+$(MAKEBUILD) -C gettext-runtime install
	touch $@

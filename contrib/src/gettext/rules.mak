# gettext
GETTEXT_VERSION := 0.22.5
GETTEXT_URL := $(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz

PKGS += gettext
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
	$(APPLY) $(SRC)/gettext/gettext-0.22.5-gnulib-rename-real-openat.patch
	$(APPLY) $(SRC)/gettext/gettext-0.22.5-gnulib-localtime.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux
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
ifdef HAVE_MACOSX
# Mark functions as missing. Gettext/gnulib checks for functions without
# using headers (which would make them unavailable with
# -Werror=partial-availability), so we need to manually mark them unavailable.
# These are unavailable in macOS 10.7.
GETTEXT_CONF += \
    ac_cv_func_clock_gettime=no \
    ac_cv_func_faccessat=no \
    ac_cv_func_fdopendir=no \
    ac_cv_func_futimens=no \
    ac_cv_func_memset_s=no \
    ac_cv_func_openat=no \
    ac_cv_func_utimensat=no
endif

.gettext: gettext
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GETTEXT_CONF)
	$(MAKE) -C $< -C gettext-runtime
	cd $< && $(MAKE) -C gettext-runtime install
	touch $@

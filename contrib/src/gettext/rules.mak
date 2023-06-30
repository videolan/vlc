# gettext
GETTEXT_VERSION := 0.22
GETTEXT_URL := $(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz

PKGS += gettext
ifneq ($(filter gnu%,$(subst -, ,$(HOST))),)
# GNU platform should have gettext (?)
PKGS_FOUND += gettext
endif

$(TARBALLS)/gettext-$(GETTEXT_VERSION).tar.gz:
	$(call download_pkg,$(GETTEXT_URL),gettext)

.sum-gettext: gettext-$(GETTEXT_VERSION).tar.gz

gettext: gettext-$(GETTEXT_VERSION).tar.gz .sum-gettext
	$(UNPACK)
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
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GETTEXT_CONF)
	$(MAKE) -C $< -C gettext-runtime
ifndef HAVE_ANDROID
	# build libgettextpo first so we can use its textstyle.h
	$(MAKE) -C $< -C gettext-tools -C libgettextpo
	cd $< && cp gettext-tools/libgettextpo/textstyle.h gettext-tools/src/textstyle.h
	$(MAKE) -C $< -C gettext-tools
	$(MAKE) -C $< -C gettext-tools install
else
	# Android 32bits does not have localeconv
	$(MAKE) -C $< -C gettext-tools/misc install
	$(MAKE) -C $< -C gettext-tools/m4 install
endif
	cd $< && $(MAKE) -C gettext-runtime install
	touch $@

# gettext
GETTEXT_VERSION := 0.26
GETTEXT_URL := $(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.xz

PKGS += gettext
ifneq ($(filter gnu%,$(subst -, ,$(HOST))),)
# GNU platform should have gettext (?)
PKGS_FOUND += gettext
endif

$(TARBALLS)/gettext-$(GETTEXT_VERSION).tar.xz:
	$(call download_pkg,$(GETTEXT_URL),gettext)

.sum-gettext: gettext-$(GETTEXT_VERSION).tar.xz

gettext: gettext-$(GETTEXT_VERSION).tar.xz .sum-gettext
	$(UNPACK)
	$(call update_autoconfig,build-aux)
	$(call update_autoconfig,libtextstyle/build-aux)
	# disable useless gettext-runtime targets
	sed -i.orig -e 's,doc ,,' $(UNPACK_DIR)/gettext-runtime/Makefile.am
	sed -i.orig -e 's,intl-java intl-csharp intl-d intl-modula2 ,,' $(UNPACK_DIR)/gettext-runtime/Makefile.am
	sed -i.orig -e 's, tests,,' $(UNPACK_DIR)/gettext-runtime/Makefile.am
	# disable useless gettext-tools configure
	sed -i.orig -e 's,gettext-runtime libtextstyle gettext-tools,gettext-runtime libtextstyle,' $(UNPACK_DIR)/configure.ac
	$(MOVE)

DEPS_gettext = iconv $(DEPS_iconv) libxml2 $(DEPS_libxml2)

GETTEXT_CONF = \
	--disable-relocatable \
	--disable-java \
	--disable-native-java \
	--disable-csharp \
	--disable-d \
	--disable-go \
	--disable-modula2 \
	--disable-openmp \
	--without-emacs \
	--without-included-libxml \
	--without-git \
	--without-cvs

ifdef HAVE_WIN32
GETTEXT_CONF += --disable-threads
endif
ifdef HAVE_LINUX
GETTEXT_CONF += --disable-libasprintf
endif
ifdef HAVE_MINGW_W64
GETTEXT_CONF += --disable-libasprintf
endif

ifeq ($(findstring libxml2,$(PKGS_FOUND)),)
GETTEXT_CONF += --with-libxml2-prefix=$(PREFIX)
endif

.gettext: gettext
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(GETTEXT_CONF)
	+$(MAKEBUILD) -C gettext-runtime bin_PROGRAMS=
	+$(MAKEBUILD) -C gettext-runtime bin_PROGRAMS= install
	touch $@

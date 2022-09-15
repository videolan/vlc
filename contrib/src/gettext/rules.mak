# gettext
GETTEXT_VERSION := 0.21
GETTEXT_URL := $(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz

ifndef HAVE_WINSTORE
PKGS += gettext
endif
ifneq ($(filter gnu%,$(subst -, ,$(HOST))),)
# GNU platform should have gettext (?)
PKGS_FOUND += gettext
endif

$(TARBALLS)/gettext-$(GETTEXT_VERSION).tar.gz:
	$(call download_pkg,$(GETTEXT_URL),gettext)

.sum-gettext: gettext-$(GETTEXT_VERSION).tar.gz

gettext: gettext-$(GETTEXT_VERSION).tar.gz .sum-gettext
	$(UNPACK)
	$(APPLY) $(SRC)/gettext/gettext-0.21-disable-libtextstyle.patch
	$(APPLY) $(SRC)/gettext/obstack-func-ptr.patch
	$(MOVE)

DEPS_gettext = iconv $(DEPS_iconv) libxml2 $(DEPS_libxml2)

GETTEXT_CONF = \
	--disable-relocatable \
	--disable-java \
	--disable-native-java \
	--without-emacs \
	--without-included-libxml

ifdef HAVE_WIN32
GETTEXT_CONF += --disable-threads
endif

.gettext: gettext
	cd $< && cd gettext-runtime && $(AUTORECONF)
	cd $< && cd gettext-tools && $(AUTORECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GETTEXT_CONF)
ifndef HAVE_ANDROID
	cd $< && $(MAKE) install
else
	# Android 32bits does not have localeconv
	cd $< && $(MAKE) -C gettext-runtime install
	cd $< && $(MAKE) -C gettext-tools/intl
	cd $< && $(MAKE) -C gettext-tools/misc install
	cd $< && $(MAKE) -C gettext-tools/m4 install
endif
ifdef HAVE_MACOSX
	# detect libintl correctly in configure for static library
	sed -i.orig  's/$$LIBS $$LIBINTL/$$LIBS $$LIBINTL $$INTL_MACOSX_LIBS/' "$(PREFIX)"/share/aclocal/gettext.m4
endif
	touch $@

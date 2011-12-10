# gettext
GETTEXT_VERSION=0.18.1.1
GETTEXT_URL=$(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz

PKGS += gettext
ifneq ($(filter gnu%,$(subst -, ,$(HOST))),)
# GNU platform should have gettext (?)
PKGS_FOUND += gettext
endif

$(TARBALLS)/gettext-$(GETTEXT_VERSION).tar.gz:
	$(call download,$(GETTEXT_URL))

.sum-gettext: gettext-$(GETTEXT_VERSION).tar.gz

gettext: gettext-$(GETTEXT_VERSION).tar.gz .sum-gettext
	$(UNPACK)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/gettext/gettext-macosx.patch
endif
	$(MOVE)

DEPS_gettext = iconv $(DEPS_iconv)

.gettext: gettext
	#cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-relocatable --disable-java --disable-native-java --without-emacs
	#cd $< && $(MAKE) install
	#cd $< && make -C gettext-runtime/intl && make -C gettext-runtime/intl install && make -C gettext-tools/misc install
	#touch $@

ifdef HAVE_WIN32
	(cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-relocatable --disable-java --disable-native-java --disable-threads)
	(cd $< && make -C gettext-runtime install && make -C gettext-tools/misc install && make -C gettext-tools/m4 install)
else
	(cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-java --disable-native-java --without-emacs)
	(cd $< && make -C gettext-runtime install && make -C gettext-tools/intl && make -C gettext-tools/libgrep && make -C gettext-tools/gnulib-lib && make -C gettext-tools/src install && make -C gettext-tools/misc install && make -C gettext-tools/m4 install)
endif
# Work around another non-sense of autoconf.
ifdef HAVE_WIN32
	(cd $(PREFIX)/include; sed -i.orig '314 c #if 0' libintl.h)
endif
ifdef HAVE_MACOSX
	# detect libintl correctly in configure for static library
	(cd $(PREFIX)/share/aclocal; sed -i.orig  '184s/$$LIBINTL/$$LIBINTL $$INTL_MACOSX_LIBS/' gettext.m4)
endif
	touch $@


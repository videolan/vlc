# gettext
GETTEXT_VERSION=0.18.3
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
	$(MOVE)

DEPS_gettext = iconv $(DEPS_iconv)

.gettext: gettext
	#cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-relocatable --disable-java --disable-native-java --without-emacs
	#cd $< && $(MAKE) install
	#cd $< && $(MAKE) -C gettext-runtime/intl && $(MAKE) -C gettext-runtime/intl install && $(MAKE) -C gettext-tools/misc install
	#touch $@

ifdef HAVE_WIN32
	(cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-relocatable --disable-java --disable-native-java --disable-threads)
	(cd $< && $(MAKE) -C gettext-runtime install && $(MAKE) -C gettext-tools/misc install && $(MAKE) -C gettext-tools/m4 install)
else
	(cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-java --disable-native-java --without-emacs)
	(cd $< && $(MAKE) -C gettext-runtime install && $(MAKE) -C gettext-tools/intl && $(MAKE) -C gettext-tools/libgrep && $(MAKE) -C gettext-tools/gnulib-lib && $(MAKE) -C gettext-tools/src install && $(MAKE) -C gettext-tools/misc install && $(MAKE) -C gettext-tools/m4 install)
endif
ifdef HAVE_MACOSX
	# detect libintl correctly in configure for static library
	sed -i.orig  's/$$LIBS $$LIBINTL/$$LIBS $$LIBINTL $$INTL_MACOSX_LIBS/' "$(PREFIX)"/share/aclocal/gettext.m4
endif
	touch $@


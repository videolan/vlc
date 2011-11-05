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
	(cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-relocatable --disable-java --disable-native-java)
else
	(cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-java --disable-native-java --without-emacs)
endif
ifneq ($(HOST),$(BUILD))
  ifndef HAVE_CYGWIN
    # We'll use the installed gettext and only need to cross-compile libintl, also build autopoint and gettextsize tools need for VLC bootstrap
	(cd $< && make -C gettext-runtime/intl && make -C gettext-runtime/intl install && make -C gettext-tools/misc install)
  else
    # We are compiling for MinGW on Cygwin -- build the full current gettext
	(cd $< && make && make install)
  endif
else
# Build and install the whole gettext
	(cd $< && make && make install)
endif
# Work around another non-sense of autoconf.
ifdef HAVE_WIN32
	(cd $(PREFIX)/include; sed -i.orig '314 c #if 0' libintl.h)
endif
	touch $@


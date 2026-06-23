# zvbi

ZVBI_VERSION := 0.2.44
ZVBI_URL := $(GITHUB)/zapping-vbi/zvbi/archive/refs/tags/v$(ZVBI_VERSION).tar.gz

PKGS += zvbi
ifeq ($(call need_pkg,"zvbi-0.2"),)
PKGS_FOUND += zvbi
endif

$(TARBALLS)/zvbi-$(ZVBI_VERSION).tar.gz:
	$(call download_pkg,$(ZVBI_URL),zvbi)

.sum-zvbi: zvbi-$(ZVBI_VERSION).tar.gz

zvbi: zvbi-$(ZVBI_VERSION).tar.gz .sum-zvbi
	$(UNPACK)
	$(APPLY) $(SRC)/zvbi/zvbi-ssize_max.patch
	$(APPLY) $(SRC)/zvbi/zvbi-fix-static-linking.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/zvbi/zvbi-win32.patch
endif
	$(APPLY) $(SRC)/zvbi/zvbi-fix-clang-support.patch
	# hardcode -liconv instead of the full path
	$(APPLY) $(SRC)/zvbi/0001-configure-hardcode-liconv-instead-of-the-full-path.patch
	# check for pthread_create in pthreads as well
	$(APPLY) $(SRC)/zvbi/0009-fix-Android-usage-of-pthread.patch
	$(APPLY) $(SRC)/zvbi/0001-fix-Windows-API-calls-when-UNICODE-is-defined.patch
	# downgrade gettext requirement to build on older platforms 0.21 is too recent
	sed -i.orig 's,AM_GNU_GETTEXT_VERSION(\[0.21\]),AM_GNU_GETTEXT_VERSION(\[0.19\]),' $(UNPACK_DIR)/configure.ac
	# disable malloc rewriting to non-existent rpl_malloc
	sed -i.orig 's,AC_FUNC_MALLOC,dnl AC_FUNC_MALLOC,' $(UNPACK_DIR)/configure.ac
	# disable realloc rewriting to non-existent rpl_realloc
	sed -i.orig 's,AC_FUNC_REALLOC,dnl AC_FUNC_REALLOC,' $(UNPACK_DIR)/configure.ac
	# fix automake lookup for README
	touch $(UNPACK_DIR)/README
	$(MOVE)

DEPS_zvbi = png $(DEPS_png) iconv $(DEPS_iconv)

ZVBICONF := \
	--disable-dvb --disable-bktr \
	--disable-nls --disable-proxy \
	--without-doxygen

ifdef HAVE_ANDROID
# discard bogus pthread_cancel calls
ZVBICONF += --disable-v4l
# nl_langinfo() is available in API level 26 but it's detected though the
# header existence, not the fact nl_langinfo() can be called.
ZVBICONF += ac_cv_header_langinfo_h=no
endif

ifdef HAVE_WIN32
DEPS_zvbi += winpthreads $(DEPS_winpthreads)
endif

.zvbi: zvbi
	cd $< && ./autogen.sh
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(ZVBICONF)
	+$(MAKEBUILD) bin_PROGRAMS= noinst_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= noinst_PROGRAMS= install
	touch $@

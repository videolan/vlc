###############################################################################
# Automake targets and declarations
###############################################################################

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
#  - src (libvlccore) is nedeed by modules
SUBDIRS = compat doc po share src modules lib bin test
DIST_SUBDIRS = m4 $(SUBDIRS)

EXTRA_DIST = \
	extras/include/x86/x86inc.asm \
	extras/include/x86/x86util.asm \
	extras/package/macosx/package.mak \
	extras/package/win32/package.mak \
	extras/package/npapi.am

dist_noinst_SCRIPTS = bootstrap
nodist_noinst_SCRIPTS = compile

BUILT_SOURCES_distclean =

BUILT_SOURCES = $(BUILT_SOURCES_distclean)

SUFFIXES = 

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-fast-install \
	--disable-a52 \
	--disable-alsa \
	--disable-avcodec --disable-avformat \
	--disable-postproc --disable-swscale \
	--disable-dbus \
	--disable-mad --disable-libmpeg2 \
	--disable-faad --disable-skins2 \
	--disable-live555 \
	--disable-lua \
	--disable-fribidi \
	--disable-mkv \
	--with-kde-solid='$${datadir}/kde4/apps'

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = \
	1.11 \
	-Wall \
	check-news \
	dist-xz \
	no-dist-gzip
#	std-options

ChangeLog: Makefile.am
	rm -f -- "$@"
	cd doc && $(MAKE) $(AM_MAKEFLAGS) changelogs
	$(LN_S) -f doc/ChangeLog-2015 "$@"


###############################################################################
# tools (needed for contrib)
##############################################################################
EXTRA_DIST += \
	extras/tools/bootstrap \
	extras/tools/packages.mak \
	extras/tools/tools.mak \
	extras/tools/SHA512SUMS \
	extras/tools/automake-clang.patch \
	extras/tools/bison-macOS-7df04f9.patch \
	extras/tools/bison-macOS-c41f233c.patch \
	extras/tools/cmake-enable-ALPN-support-on-macOS-10.14.patch \
	extras/tools/libtool-2.4.6-bitcode.patch \
	extras/tools/libtool-2.4.6-clang-libs.patch \
	extras/tools/libtool-2.4.6-response-files.patch \
	extras/tools/libtool-2.4.6-san.patch \
	extras/tools/ragel-6.8-javacodegen.patch

###############################################################################
# Various utilities ( editor syntax files, D-Bus controller ... )
##############################################################################
EXTRA_DIST += \
	extras/analyser/zsh_completion.sh \
	extras/analyser/zsh.cpp \
	extras/analyser/emacs.init \
	extras/analyser/vlc.vim \
	extras/analyser/valgrind.suppressions \
	extras/buildsystem/make.pl \
	extras/misc/mpris.py \
	extras/misc/mpris.xml

###############################################################################
# Scripts for building dependencies.
##############################################################################
EXTRA_DIST += \
	contrib/bootstrap \
	contrib/src

###############################################################################
# Building libvlc
###############################################################################

CLEANFILES =
DISTCLEANFILES = $(BUILT_SOURCES_distclean) compile doltcompile doltlibtool
MAINTAINERCLEANFILES = ChangeLog

# Shortcut for developers to rebuild the core (libvlc + vlc)
# Don't use it if you don't know what it is about.
# Don't complain if it doesn't work. -- Courmisch
libcompat:
	cd compat && $(MAKE) $(AM_MAKEFLAGS)

libvlccore: libcompat
	cd src && $(MAKE) $(AM_MAKEFLAGS) libvlccore.la

libvlc: libvlccore
	cd lib && $(MAKE) $(AM_MAKEFLAGS) libvlc.la

core: libvlc vlc$(EXEEXT)
	cd bin && $(MAKE) $(AM_MAKEFLAGS) vlc$(EXEEXT) vlc-static$(EXEEXT)

doc:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc

.PHONY: libvlc core doc

###############################################################################
# Building aliases
###############################################################################

ALL_ALIASES = cvlc rvlc svlc qvlc nvlc
if BUILD_VLC
bin_SCRIPTS = $(ALIASES)
endif
CLEANFILES += $(ALIASES) $(noinst_SCRIPTS)
EXTRA_SCRIPTS = $(ALL_ALIASES)

dist_noinst_SCRIPTS += make-alias

MKALIAS = bindir="$(bindir)" transform="$(transform)" program_prefix="$(program_prefix)" program_suffix="$(program_suffix)" $(top_srcdir)/make-alias $@

cvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) dummy

rvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) rc

svlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) skins2

qvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) qt

nvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) ncurses

if BUILD_VLC
noinst_SCRIPTS = vlc$(EXEEXT)
endif

vlc$(EXEEXT):
if HAVE_DARWIN
	$(AM_V_GEN)$(LN_S) -f bin/vlc-osx-static vlc
else
	$(AM_V_GEN)$(LN_S) -f bin/vlc-static$(EXEEXT) vlc$(EXEEXT)
endif

TESTS = test/run_vlc.sh
dist_noinst_SCRIPTS += test/run_vlc.sh

if BUILD_VLC
###############################################################################
# Installing plugins cache
###############################################################################
install-exec-hook:
	if test "$(build)" = "$(host)"; then \
		PATH="$(DESTDIR)$(bindir):$$PATH" \
		LD_LIBRARY_PATH="$(DESTDIR)$(libdir):$$LD_LIBRARY_PATH" \
		"$(DESTDIR)$(pkglibexecdir)/vlc-cache-gen$(EXEEXT)" \
			 "$(DESTDIR)$(pkglibdir)/plugins" ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi
endif

uninstall-hook:
	rm -f -- "$(DESTDIR)$(pkglibdir)/plugins/plugins.dat"

###############################################################################
# Test coverage
###############################################################################

lcov-raw.out:
	$(MAKE) $(AM_MAKEFLAGS) all
	lcov -z -d .
	$(MAKE) $(AM_MAKEFLAGS) check
	lcov -c -d . -o lcov-raw.out

lcov.out: lcov-raw.out
	lcov -r lcov-raw.out -o lcov.out \
		'*test*' 'contrib/*' '/usr/include/*' '*mock*'

lcov: lcov.out
	rm -Rf lcov lcov.tmp
	prefix="$$(cd "$(top_srcdir)" && pwd)" ; \
	genhtml -p "$$prefix" -o lcov.tmp lcov.out >/dev/null
	mv lcov.tmp lcov

.PHONY: lcov-raw.out


###############################################################################
# PO translation files update
###############################################################################
.PHONY: update-po

update-po:
	cd po && $(MAKE) POTFILES vlc.pot update-po

###############################################################################
# OS Packaging rules
###############################################################################
include extras/package/macosx/package.mak
include extras/package/win32/package.mak
include extras/package/win32/msi.mak


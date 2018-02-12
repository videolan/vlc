# Qt

QTSVG_VERSION := 5.6.3
QTSVG_URL := https://download.qt.io/official_releases/qt/5.6/$(QTSVG_VERSION)/submodules/qtsvg-opensource-src-$(QTSVG_VERSION).tar.xz

DEPS_qtsvg += qt $(DEPS_qt)

ifdef HAVE_WIN32
PKGS += qtsvg
endif

ifeq ($(call need_pkg,"Qt5Svg"),)
PKGS_FOUND += qtsvg
endif

$(TARBALLS)/qtsvg-$(QTSVG_VERSION).tar.xz:
	$(call download,$(QTSVG_URL))

.sum-qtsvg: qtsvg-$(QTSVG_VERSION).tar.xz

qtsvg: qtsvg-$(QTSVG_VERSION).tar.xz .sum-qtsvg
	$(UNPACK)
	mv qtsvg-opensource-src-$(QTSVG_VERSION) qtsvg-$(QTSVG_VERSION)
	$(APPLY) $(SRC)/qtsvg/0001-Force-the-usage-of-QtZlib-header.patch
	$(MOVE)

.qtsvg: qtsvg
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $</src && $(MAKE) sub-plugins-install_subtargets sub-svg-install_subtargets
	mv $(PREFIX)/plugins/iconengines/libqsvgicon.a $(PREFIX)/lib/
	mv $(PREFIX)/plugins/imageformats/libqsvg.a $(PREFIX)/lib/
	cd $(PREFIX)/lib/pkgconfig; sed -i \
		-e 's/d\.a/.a/g' \
		-e 's/-lQt\([^ ]*\)d/-lQt\1/g' \
		-e '/Libs:/  s/-lQt5Svg/-lqsvg -lqsvgicon -lQt5Svg/ ' \
		Qt5Svg.pc
	touch $@

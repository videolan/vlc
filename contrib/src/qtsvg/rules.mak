# Qt

QTSVG_VERSION_MAJOR := 5.12
QTSVG_VERSION := $(QTSVG_VERSION_MAJOR).7
QTSVG_URL := https://download.qt.io/official_releases/qt/$(QTSVG_VERSION_MAJOR)/$(QTSVG_VERSION)/submodules/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

DEPS_qtsvg += qt $(DEPS_qt)

ifdef HAVE_WIN32
PKGS += qtsvg
endif

ifeq ($(call need_pkg,"Qt5Svg"),)
PKGS_FOUND += qtsvg
endif

$(TARBALLS)/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz:
	$(call download_pkg,$(QTSVG_URL),qt)

.sum-qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz .sum-qtsvg
	$(UNPACK)
	$(APPLY) $(SRC)/qtsvg/0001-Force-the-usage-of-QtZlib-header.patch
	$(MOVE)

.qtsvg: qtsvg
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-plugins-install_subtargets sub-svg-install_subtargets
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5Svg plugins/iconengines qsvgicon
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5Svg plugins/imageformats qsvg
	touch $@

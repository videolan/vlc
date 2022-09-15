# Qt

QTSVG_VERSION_MAJOR := 5.15
QTSVG_VERSION := $(QTSVG_VERSION_MAJOR).1
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
	cd $< && $(PREFIX)/lib/qt5/bin/qmake
	# Make && Install libraries
	$(MAKE) -C $<
	$(MAKE) -C $< -C src sub-plugins-install_subtargets \
		INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)" \
		sub-svg-install_subtargets
	touch $@

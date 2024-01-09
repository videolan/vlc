# qtsvg

QTSVG_VERSION_MAJOR := 6.6
QTSVG_VERSION := $(QTSVG_VERSION_MAJOR).2
QTSVG_URL := $(QT)/$(QTSVG_VERSION_MAJOR)/$(QTSVG_VERSION)/submodules/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

DEPS_qtsvg += qt $(DEPS_qt)

ifdef HAVE_WIN32
#PKGS += qtsvg
endif

ifeq ($(call need_pkg,"Qt6Svg >= 6.6"),)
PKGS_FOUND += qtsvg
endif

$(TARBALLS)/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz:
	$(call download_pkg,$(QTSVG_URL),qt)

.sum-qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz .sum-qtsvg
	$(UNPACK)
	$(MOVE)

.qtsvg: qtsvg toolchain.cmake
	mkdir -p $(BUILD_DIR)
	+cd $(BUILD_DIR) && $(PREFIX)/bin/qt-configure-module $(BUILD_SRC)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

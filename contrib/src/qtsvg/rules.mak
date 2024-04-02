# qtsvg

QTSVG_VERSION_MAJOR := 6.6
QTSVG_VERSION := $(QTSVG_VERSION_MAJOR).2
QTSVG_URL := $(QT)/$(QTSVG_VERSION_MAJOR)/$(QTSVG_VERSION)/submodules/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

DEPS_qtsvg += qt $(DEPS_qt)

ifdef HAVE_WIN32
PKGS += qtsvg
endif

ifeq ($(call need_pkg,"Qt6Svg >= 6.6"),)
PKGS_FOUND += qtsvg
endif

$(TARBALLS)/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz:
	$(call download_pkg,$(QTSVG_URL),qt)

.sum-qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

QTSVG_CONFIG := -DCMAKE_TOOLCHAIN_FILE=$(PREFIX)/lib/cmake/Qt6/qt.toolchain.cmake
ifdef ENABLE_PDB
QTSVG_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QTSVG_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz .sum-qtsvg
	$(UNPACK)
	$(MOVE)

.qtsvg: qtsvg toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(QTSVG_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

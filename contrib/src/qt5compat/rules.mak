# Qt5Compat

QT5COMPAT_VERSION_MAJOR := 6.6
QT5COMPAT_VERSION := $(QT5COMPAT_VERSION_MAJOR).2
QT5COMPAT_URL := $(QT)/$(QT5COMPAT_VERSION_MAJOR)/$(QT5COMPAT_VERSION)/submodules/qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz

DEPS_qt5compat += qtdeclarative $(DEPS_qtdeclarative) qtshadertools $(DEPS_qtshadertools)

ifdef HAVE_WIN32
PKGS += qt5compat
endif

ifeq ($(call need_pkg,"Qt6Core5Compat >= 6.6"),)
PKGS_FOUND += qt5compat
endif

$(TARBALLS)/qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz:
	$(call download_pkg,$(QT5COMPAT_URL),qt)

.sum-qt5compat: qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz

QT5_COMPAT_CONFIG := -DCMAKE_TOOLCHAIN_FILE=$(PREFIX)/lib/cmake/Qt6/qt.toolchain.cmake
ifdef ENABLE_PDB
QT5_COMPAT_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QT5_COMPAT_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

qt5compat: qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz .sum-qt5compat
	$(UNPACK)
	$(MOVE)

.qt5compat: qt5compat toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(QT5_COMPAT_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

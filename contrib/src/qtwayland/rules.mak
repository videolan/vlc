# qtwayland

QTWAYLAND_VERSION_MAJOR := 6.6
QTWAYLAND_VERSION := $(QTWAYLAND_VERSION_MAJOR).2
QTWAYLAND_URL := $(QT)/$(QTWAYLAND_VERSION_MAJOR)/$(QTWAYLAND_VERSION)/submodules/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

DEPS_qtwayland = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz:
	$(call download,$(QTWAYLAND_URL))

.sum-qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

QTWAYLAND_CONFIG := -DCMAKE_TOOLCHAIN_FILE=$(PREFIX)/lib/cmake/Qt6/qt.toolchain.cmake
ifdef ENABLE_PDB
QTWAYLAND_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QTWAYLAND_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz .sum-qtwayland
	$(UNPACK)
	$(MOVE)

.qtwayland: qtwayland toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(QTWAYLAND_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

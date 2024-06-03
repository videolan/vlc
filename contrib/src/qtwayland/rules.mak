# qtwayland

QTWAYLAND_VERSION := $(QTBASE_VERSION)
QTWAYLAND_URL := $(QT)/$(QTWAYLAND_VERSION)/submodules/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

ifdef HAVE_LINUX
ifneq ($(findstring qt,$(PKGS)),)
PKGS += qtwayland
endif
endif

DEPS_qtwayland = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz:
	$(call download,$(QTWAYLAND_URL))

.sum-qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

QTWAYLAND_CONFIG := $(QT_CMAKE_CONFIG)
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
	$(HOSTVARS_CMAKE) $(CMAKE) $(QTWAYLAND_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

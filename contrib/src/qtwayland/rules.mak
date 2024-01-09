# qtwayland

QTWAYLAND_VERSION_MAJOR := 6.6
QTWAYLAND_VERSION := $(QTWAYLAND_VERSION_MAJOR).2
QTWAYLAND_URL := $(QT)/$(QTWAYLAND_VERSION_MAJOR)/$(QTWAYLAND_VERSION)/submodules/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

DEPS_qtwayland = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz:
	$(call download,$(QTWAYLAND_URL))

.sum-qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz .sum-qtwayland
	$(UNPACK)
	$(MOVE)

.qtwayland: qtwayland toolchain.cmake
	mkdir -p $(BUILD_DIR)
	+cd $(BUILD_DIR) && $(PREFIX)/bin/qt-configure-module $(BUILD_SRC)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

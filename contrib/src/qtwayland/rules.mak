# qtwayland

QTWAYLAND_VERSION_MAJOR := 5.15
QTWAYLAND_VERSION := $(QTWAYLAND_VERSION_MAJOR).8
QTWAYLAND_URL := $(QT)/$(QTWAYLAND_VERSION_MAJOR)/$(QTWAYLAND_VERSION)/submodules/qtwayland-everywhere-opensource-src-$(QTWAYLAND_VERSION).tar.xz

DEPS_qtwayland = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz:
	$(call download,$(QTWAYLAND_URL))

.sum-qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

qtwayland: qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz .sum-qtwayland
	$(UNPACK)
	sed -i.orig '/SUBDIRS/d' "$(UNPACK_DIR)/tests/tests.pro"
	sed -i.orig 's/"egl drm"/"egl"/g' \
		$(UNPACK_DIR)/src/compositor/configure.json \
		$(UNPACK_DIR)/src/client/configure.json
	$(MOVE)

.qtwayland: qtwayland
	$(call qmake_toolchain, $<)
	cd $< && $(PREFIX)/lib/qt5/bin/qmake
	# Make && Install libraries
	$(MAKE) -C $<
	$(MAKE) -C $< install
	touch $@

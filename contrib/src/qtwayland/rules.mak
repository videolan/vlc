# qtwayland

QTWAYLAND_VERSION_MAJOR := 5.15
QTWAYLAND_VERSION := $(QTWAYLAND_VERSION_MAJOR).1
QTWAYLAND_URL := http://download.qt.io/development_releases/qt/5.12/5.12.0-beta1/submodules/qtwayland-everywhere-src-5.12.0-beta1.tar.xz
QTWAYLAND_URL := http://download.qt.io/official_releases/qt/$(QTWAYLAND_VERSION_MAJOR)/$(QTWAYLAND_VERSION)/submodules/qtwayland-everywhere-src-$(QTWAYLAND_VERSION).tar.xz

DEPS_qtwayland = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtwayland-$(QTWAYLAND_VERSION).tar.xz:
	$(call download,$(QTWAYLAND_URL))

.sum-qtwayland: qtwayland-$(QTWAYLAND_VERSION).tar.xz

qtwayland: qtwayland-$(QTWAYLAND_VERSION).tar.xz .sum-qtwayland
	$(UNPACK)
	mv qtwayland-everywhere-src-$(QTWAYLAND_VERSION) qtwayland-$(QTWAYLAND_VERSION)
	sed -i.orig '/SUBDIRS/d' "$(UNPACK_DIR)/tests/tests.pro"
	sed -i.orig 's/"egl drm"/"egl"/g' \
		$(UNPACK_DIR)/src/compositor/configure.json \
		$(UNPACK_DIR)/src/client/configure.json
	$(MOVE)

.qtwayland: qtwayland
	cd $< && $(PREFIX)/lib/qt5/bin/qmake
	# Make && Install libraries
	$(MAKE) -C $<
	$(MAKE) -C $< -C src \
		INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)" \
		sub-plugins-install_subtargets
	touch $@

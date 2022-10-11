# qtx11extras

QTX11_VERSION_MAJOR := 5.15
QTX11_VERSION:= $(QTX11_VERSION_MAJOR).1
QTX11_URL := $(QT)/$(QTX11_VERSION_MAJOR)/$(QTX11_VERSION)/submodules/qtx11extras-everywhere-src-$(QTX11_VERSION).tar.xz

DEPS_qtx11extras += qt $(DEPS_qt)

$(TARBALLS)/qtx11extras-$(QTX11_VERSION).tar.xz:
	$(call download,$(QTX11_URL))

.sum-qtx11extras: qtx11extras-$(QTX11_VERSION).tar.xz

qtx11extras: qtx11extras-$(QTX11_VERSION).tar.xz .sum-qtx11extras
	$(UNPACK)
	mv qtx11extras-everywhere-src-$(QTX11_VERSION) qtx11extras-$(QTX11_VERSION)
	$(MOVE)

.qtx11extras: qtx11extras
	$(call qmake_toolchain, $<)
	cd $< && $(PREFIX)/lib/qt5/bin/qmake
	# Make && Install libraries
	$(MAKE) -C $< install INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)"
	touch $@

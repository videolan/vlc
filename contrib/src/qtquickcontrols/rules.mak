#QtQuickControls 1
QTQC_VERSION := 5.11.0
QTQC_URL := http://download.qt.io/official_releases/qt/5.11/$(QTQC_VERSION)/submodules/qtquickcontrols-everywhere-src-$(QTQC_VERSION).tar.xz

DEPS_qtquickcontrols = qtquickcontrols2 $(DEPS_qtquickcontrols2)

$(TARBALLS)/qtquickcontrols-$(QTQC_VERSION).tar.xz:
	$(call download_pkg,$(QTQC_URL),qt)

.sum-qtquickcontrols: qtquickcontrols-$(QTQC_VERSION).tar.xz

qtquickcontrols: qtquickcontrols-$(QTQC_VERSION).tar.xz .sum-qtquickcontrols
	$(UNPACK)
	mv qtquickcontrols-everywhere-src-$(QTQC_VERSION) qtquickcontrols-$(QTQC_VERSION)
	$(MOVE)

.qtquickcontrols: qtquickcontrols
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-controls-install_subtargets
	cp $(PREFIX)/qml/QtQuick/Controls/libqtquickcontrolsplugin.a $(PREFIX)/lib/
	rm -rf $(PREFIX)/qml
	touch $@

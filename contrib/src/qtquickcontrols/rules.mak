# QtQuickControls 1 & 2

QTQC_VERSION := 5.11.0
QTQC_URL := http://download.qt.io/official_releases/qt/5.11/$(QTQC_VERSION)/submodules/qtquickcontrols-everywhere-src-$(QTQC_VERSION).tar.xz
QTQC2_URL := http://download.qt.io/official_releases/qt/5.11/$(QTQC_VERSION)/submodules/qtquickcontrols2-everywhere-src-$(QTQC_VERSION).tar.xz

ifdef HAVE_WIN32
PKGS += qtquickcontrols qtquickcontrols2
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtquickcontrols2
endif
# QtQuickControl(1) doesn't provide a .pc

DEPS_qtquickcontrols = qtdeclarative $(DEPS_qtdeclarative)
DEPS_qtquickcontrols2 = qtquickcontrols $(DEPS_qtquickcontrols)

$(TARBALLS)/qtquickcontrols-$(QTQC_VERSION).tar.xz:
	$(call download,$(QTQC_URL))

$(TARBALLS)/qtquickcontrols2-$(QTQC_VERSION).tar.xz:
	$(call download,$(QTQC2_URL))

.sum-qtquickcontrols: qtquickcontrols-$(QTQC_VERSION).tar.xz
.sum-qtquickcontrols2: qtquickcontrols2-$(QTQC_VERSION).tar.xz

qtquickcontrols: qtquickcontrols-$(QTQC_VERSION).tar.xz .sum-qtquickcontrols
	$(UNPACK)
	mv qtquickcontrols-everywhere-src-$(QTQC_VERSION) qtquickcontrols-$(QTQC_VERSION)
	$(MOVE)

qtquickcontrols2: qtquickcontrols2-$(QTQC_VERSION).tar.xz .sum-qtquickcontrols2
	$(UNPACK)
	mv qtquickcontrols2-everywhere-src-$(QTQC_VERSION) qtquickcontrols2-$(QTQC_VERSION)
	$(MOVE)


ifdef HAVE_CROSS_COMPILE
QMAKE=$(PREFIX)/bin/qmake
else
QMAKE=../qt/bin/qmake
endif

.qtquickcontrols: qtquickcontrols
	cd $< && $(QMAKE)
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-controls-install_subtargets
	cp $(PREFIX)/qml/QtQuick/Controls/libqtquickcontrolsplugin.a $(PREFIX)/lib/
	rm -rf $(PREFIX)/qml
	touch $@

.qtquickcontrols2: qtquickcontrols2 .qtquickcontrols
	cd $< && $(QMAKE)
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-quickcontrols2-install_subtargets sub-imports-install_subtargets
	cp $(PREFIX)/qml/QtQuick/Controls.2/libqtquickcontrols2plugin.a $(PREFIX)/lib/
	cp $(PREFIX)/qml/QtQuick/Templates.2/libqtquicktemplates2plugin.a $(PREFIX)/lib/
	rm -rf $(PREFIX)/qml
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig \
		-e 's/d\.a/.a/g' \
		-e 's/-lQt\([^ ]*\)d/-lQt\1/g' \
		-e 's/ -lQt5QuickControls2/ -lqtquickcontrolsplugin -lqtquickcontrols2plugin -lqtquicktemplates2plugin -lQt5QuickControls2/' \
		Qt5QuickControls2.pc

	touch $@


# QtQuickControls 2

QTQC2_VERSION_MAJOR := 5.12
QTQC2_VERSION := $(QTQC2_VERSION_MAJOR).2
QTQC2_URL := http://download.qt.io/official_releases/qt/$(QTQC2_VERSION_MAJOR)/$(QTQC2_VERSION)/submodules/qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz

ifdef HAVE_WIN32
PKGS += qtquickcontrols2
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtquickcontrols2
endif
# QtQuickControl(1) doesn't provide a .pc

DEPS_qtquickcontrols2 = qtdeclarative $(DEPS_qtdeclarative)

$(TARBALLS)/qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz:
	$(call download_pkg,$(QTQC2_URL),qt)

.sum-qtquickcontrols2: qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz

qtquickcontrols2: qtquickcontrols2-everywhere-src-$(QTQC2_VERSION).tar.xz .sum-qtquickcontrols2
	$(UNPACK)
	$(MOVE)

.qtquickcontrols2: qtquickcontrols2
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-quickcontrols2-install_subtargets sub-imports-install_subtargets
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5QuickControls2 qml/QtQuick/Controls.2 qtquickcontrols2plugin
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5QuickControls2 qml/QtQuick/Templates.2 qtquicktemplates2plugin
	touch $@

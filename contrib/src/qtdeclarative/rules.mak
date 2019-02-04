# QtDeclarative

QTDECLARATIVE_VERSION_MAJOR := 5.12
QTDECLARATIVE_VERSION := $(QTDECLARATIVE_VERSION_MAJOR).2
QTDECLARATIVE_URL := http://download.qt.io/official_releases/qt/$(QTDECLARATIVE_VERSION_MAJOR)/$(QTDECLARATIVE_VERSION)/submodules/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

DEPS_qtdeclarative += qt $(DEPS_qt)

ifdef HAVE_WIN32
PKGS += qtdeclarative
endif

ifeq ($(call need_pkg,"Qt5Quick"),)
PKGS_FOUND += qtdeclarative
endif

$(TARBALLS)/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz:
	$(call download_pkg,$(QTDECLARATIVE_URL),qt)

.sum-qtdeclarative: qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

qtdeclarative: qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz .sum-qtdeclarative
	$(UNPACK)
	$(APPLY) $(SRC)/qtdeclarative/0001-V4-Only-enable-the-JIT-on-ARM-on-specific-known-OSes.patch
	$(MOVE)

.qtdeclarative: qtdeclarative
	# Generate Makefile & src/Makefile
	cd $< && $(PREFIX)/bin/qmake
	cd $</src && $(PREFIX)/bin/qmake -o Makefile src.pro
	# Build & install only what we require
	# Invoke the build rules one at a time as some rule dependencies seem to be broken
	cd $< && $(MAKE) -C src sub-quick-make_first-ordered
	cd $< && $(MAKE) -C src sub-qmltest-make_first-ordered
	# We don't use particles, but the import target (which generates the qtquick2plugin.a) require
	# the particle module to be built
	cd $< && $(MAKE) -C src sub-particles-make_first-ordered
	cd $< && $(MAKE) -C src sub-qmltest-install_subtargets sub-quick-install_subtargets sub-qml-install_subtargets sub-quickwidgets-install_subtargets sub-imports-install_subtargets
	cp $(PREFIX)/qml/QtQuick.2/libqtquick2plugin.a $(PREFIX)/lib/
	cd $(PREFIX)/qml/QtQuick/ && cp Layouts/libqquicklayoutsplugin.a Window.2/libwindowplugin.a $(PREFIX)/lib/
	cp $(PREFIX)/qml/QtQml/Models.2/libmodelsplugin.a $(PREFIX)/lib/
	rm -rf $(PREFIX)/qml
	for i in Qt5Quick Qt5Qml Qt5QuickWidgets; do \
		$(SRC)/qt/FixQtPcFiles.sh $(PREFIX)/lib/$$i.prl $(PREFIX)/lib/pkgconfig/$$i.pc; \
	done
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Quick/ -lqtquick2plugin -lqquicklayoutsplugin -lwindowplugin -lQt5Quick/' Qt5Quick.pc
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Qml/ -lmodelsplugin -lQt5Qml/' Qt5Qml.pc


	touch $@

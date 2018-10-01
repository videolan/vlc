# QtDeclarative

QTDECLARATIVE_VERSION := 5.11.0
QTDECLARATIVE_URL := http://download.qt.io/official_releases/qt/5.11/$(QTDECLARATIVE_VERSION)/submodules/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

DEPS_qtdeclarative += qt $(DEPS_qt)

ifdef HAVE_WIN32
ifeq ($(findstring $(ARCH), arm aarch64),)
# There is no opengl available on windows on these architectures.
# QtDeclarative in itself should be usable without opengl though, but
# our current build rules requires opengl (the "particles" feature
# is unavailable if opengl is disabled).
PKGS += qtdeclarative
endif
endif

ifeq ($(call need_pkg,"Qt5Quick"),)
PKGS_FOUND += qtdeclarative
endif

$(TARBALLS)/qtdeclarative-$(QTDECLARATIVE_VERSION).tar.xz:
	$(call download,$(QTDECLARATIVE_URL))

.sum-qtdeclarative: qtdeclarative-$(QTDECLARATIVE_VERSION).tar.xz

qtdeclarative: qtdeclarative-$(QTDECLARATIVE_VERSION).tar.xz .sum-qtdeclarative
	$(UNPACK)
	mv qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION) qtdeclarative-$(QTDECLARATIVE_VERSION)
	$(MOVE)

.qtdeclarative: qtdeclarative
	# Generate Makefile & src/Makefile
ifdef HAVE_CROSS_COMPILE
	cd $< && $(PREFIX)/bin/qmake
	cd $</src && $(PREFIX)/bin/qmake -o Makefile src.pro
else
	cd $< && ../qt/bin/qmake
	cd $</src && ../../qt/bin/qmake -o Makefile src.pro
endif
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
	cd $(PREFIX)/lib/pkgconfig; for i in Qt5Quick.pc Qt5Qml.pc Qt5QuickWidgets.pc; do \
		sed -i.orig -e 's/d\.a/.a/g' -e 's/-lQt\([^ ]*\)d/-lQt\1/g' $$i; done
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Quick/ -lqtquick2plugin -lqquicklayoutsplugin -lwindowplugin -lQt5Quick/' Qt5Quick.pc
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Qml/ -lmodelsplugin -lQt5Qml/' Qt5Qml.pc

	touch $@

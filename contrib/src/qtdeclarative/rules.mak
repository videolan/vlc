# QtDeclarative

QTDECLARATIVE_VERSION_MAJOR := 5.12
QTDECLARATIVE_VERSION := $(QTDECLARATIVE_VERSION_MAJOR).7
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
	$(MOVE)

QT_DECLARATIVE_CONFIG := \
     -no-feature-qml-debug \
     -no-feature-quick-designer

.qtdeclarative: qtdeclarative
	# Generate Makefile & src/Makefile
	cd $< && $(PREFIX)/bin/qmake -- $(QT_DECLARATIVE_CONFIG)
	cd $</src && $(PREFIX)/bin/qmake -o Makefile src.pro
	# Build & install only what we require
	# Invoke the build rules one at a time as some rule dependencies seem to be broken
	cd $< && $(MAKE) -C src sub-quick-make_first-ordered
	# We don't use particles, but the import target (which generates the qtquick2plugin.a) require
	# the particle module to be built
	cd $< && $(MAKE) -C src sub-particles-make_first-ordered
	cd $< && $(MAKE) -C src sub-quick-install_subtargets sub-qml-install_subtargets sub-quickwidgets-install_subtargets sub-imports-install_subtargets
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5Quick qml/QtQuick.2 qtquick2plugin
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5Quick qml/QtQuick/Layouts qquicklayoutsplugin
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5Quick qml/QtQuick/Window.2 windowplugin
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5Qml qml/QtQml/Models.2 modelsplugin

	touch $@

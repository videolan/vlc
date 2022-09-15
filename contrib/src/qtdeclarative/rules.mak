# QtDeclarative

QTDECLARATIVE_VERSION_MAJOR := 5.15
QTDECLARATIVE_VERSION := $(QTDECLARATIVE_VERSION_MAJOR).1
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
	$(APPLY) $(SRC)/qtdeclarative/fix-gcc11-build.patch
	$(MOVE)

QT_DECLARATIVE_CONFIG := \
     -no-feature-d3d12 \
     -no-feature-qml-debug \
     -no-feature-quick-designer

.qtdeclarative: qtdeclarative
	# Generate Makefile & src/Makefile
	cd $< && $(PREFIX)/lib/qt5/bin/qmake -- $(QT_DECLARATIVE_CONFIG)
	cd $</src && $(PREFIX)/lib/qt5/bin/qmake -o Makefile src.pro
	# Build & install only what we require
	# Invoke the build rules one at a time as some rule dependencies seem to be broken
	$(MAKE) -C $< -C src \
		INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)" \
		sub-quick-make_first-ordered \
		sub-qmlmodels-make_first-ordered \
		sub-qmldevtools-make_first-ordered \
		sub-qmlworkerscript-make_first-ordered
	# We don't use particles, but the import target (which generates the qtquick2plugin.a) require
	# the particle module to be built
	$(MAKE) -C $< -C src \
		INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)" \
		sub-particles-make_first-ordered
	$(MAKE) -C $< -C src \
		INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)" \
		sub-quick-install_subtargets \
		sub-qml-install_subtargets \
		sub-quickwidgets-install_subtargets \
		sub-imports-install_subtargets \
		sub-qmlmodels-install_subtargets \
		sub-qmlworkerscript-install_subtargets \
		sub-quickshapes-install_subtargets
	cd $</tools && $(PREFIX)/lib/qt5/bin/qmake -o Makefile tools.pro
	$(MAKE) -C $< -C tools \
		INSTALL_FILE="$(QT_QINSTALL)" VLC_PREFIX="$(PREFIX)" \
		sub-qmlcachegen-install_subtargets
	touch $@

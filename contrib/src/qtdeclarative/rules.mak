# QtDeclarative

QTDECLARATIVE_VERSION_MAJOR := 5.15
QTDECLARATIVE_VERSION := $(QTDECLARATIVE_VERSION_MAJOR).8
QTDECLARATIVE_URL := $(QT)/$(QTDECLARATIVE_VERSION_MAJOR)/$(QTDECLARATIVE_VERSION)/submodules/qtdeclarative-everywhere-opensource-src-$(QTDECLARATIVE_VERSION).tar.xz

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
	# do not build qml.exe and other useless tools
	sed -i.orig 's,!wasm:!rtems ,!wasm:!rtems:!static ,' "$(UNPACK_DIR)/tools/tools.pro"
	$(MOVE)

QT_DECLARATIVE_CONFIG := \
     -no-feature-d3d12 \
     -no-feature-qml-debug \
     -no-feature-quick-designer \
     -no-feature-quick-particles

.qtdeclarative: qtdeclarative
	# Generate Makefile & src/Makefile
	$(call qmake_toolchain, $<)
	cd $< && $(PREFIX)/lib/qt5/bin/qmake -- $(QT_DECLARATIVE_CONFIG)
	$(MAKE) -C $<
	$(MAKE) -C $< install
	touch $@

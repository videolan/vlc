# Qt

QT_VERSION_MAJOR := 5.15
QT_VERSION := $(QT_VERSION_MAJOR).1
# Insert potential -betaX suffix here:
QT_VERSION_FULL := $(QT_VERSION)
QT_URL := $(QT)/$(QT_VERSION_MAJOR)/$(QT_VERSION_FULL)/submodules/qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
DEPS_qt = fxc2 $(DEPS_fxc2) d3d9 $(DEPS_d3d9)
ifneq ($(call mingw_at_least, 8), true)
DEPS_qt += dcomp $(DEPS_dcomp)
endif # MINGW 8
ifdef HAVE_CROSS_COMPILE
DEPS_qt += wine-headers
endif
endif

DEPS_qt += freetype2 $(DEPS_freetype2) harfbuzz $(DEPS_harfbuzz) jpeg $(DEPS_jpeg) png $(DEPS_png) zlib $(DEPS_zlib)

ifeq ($(call need_pkg,"Qt5Core >= 5.11 Qt5Gui Qt5Widgets"),)
PKGS_FOUND += qt
endif

$(TARBALLS)/qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz:
	$(call download_pkg,$(QT_URL),qt)

.sum-qt: qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz

qt: qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz .sum-qt
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/qt/0002-Windows-QPA-Disable-systray-notification-sounds.patch
ifndef HAVE_WIN64
	$(APPLY) $(SRC)/qt/0001-disable-qt_random_cpu.patch
endif
	$(APPLY) $(SRC)/qt/0006-ANGLE-don-t-use-msvc-intrinsics-when-crosscompiling-.patch
	$(APPLY) $(SRC)/qt/0007-ANGLE-remove-static-assert-that-can-t-be-evaluated-b.patch
	$(APPLY) $(SRC)/qt/0008-ANGLE-disable-ANGLE_STD_ASYNC_WORKERS-when-compiling.patch
	$(APPLY) $(SRC)/qt/0009-Add-KHRONOS_STATIC-to-allow-static-linking-on-Windows.patch

ifdef HAVE_CROSS_COMPILE
	$(APPLY) $(SRC)/qt/0003-allow-cross-compilation-of-angle-with-wine.patch
ifndef HAVE_CLANG
	$(APPLY) $(SRC)/qt/0010-Windows-QPA-Fix-build-with-mingw64-Win32-threading.patch
endif
else
	$(APPLY) $(SRC)/qt/0003-fix-angle-compilation.patch
	cd $(UNPACK_DIR); for i in QtFontDatabaseSupport QtWindowsUIAutomationSupport QtEventDispatcherSupport QtCore; do \
		sed -i.orig -e 's,"../../../../../src,"../src,g' include/$$i/$(QT_VERSION)/$$i/private/*.h; done
endif
endif
	$(APPLY) $(SRC)/qt/qt-fix-gcc11-build.patch
	$(APPLY) $(SRC)/qt/qt-add-missing-header-darwin.patch
	$(APPLY) $(SRC)/qt/set-mkspecs-properties.patch
	# fix detection of our harfbuzz on macosx
	sed -i.orig 's#"-lharfbuzz"#{ "libs": "-framework CoreText -framework CoreGraphics -framework CoreFoundation -lharfbuzz", "condition": "config.darwin" }, "-lharfbuzz"#' "$(UNPACK_DIR)/src/gui/configure.json"
	# Let us decide the WINVER/_WIN32_WINNT
	sed -i.orig 's,mingw: DEFINES += WINVER=0x0601,# mingw: DEFINES += WINVER=0x0601,' "$(UNPACK_DIR)/mkspecs/features/qt_build_config.prf"
	# Prevent all Qt contribs from generating and installing libtool .la files
	sed -i.orig "/CONFIG/ s/ create_libtool/ -create_libtool/g" $(UNPACK_DIR)/mkspecs/features/qt_module.prf
	$(MOVE)


ifdef HAVE_WIN32
QT_OPENGL := -angle
else
QT_OPENGL := -opengl desktop
endif

ifdef HAVE_MACOSX
QT_SPEC := macx-clang
endif

ifdef HAVE_LINUX
ifdef HAVE_CLANG
QT_SPEC := linux-clang
else
QT_SPEC := linux-g++
endif
endif

ifdef HAVE_WIN32

ifdef HAVE_CLANG
QT_SPEC := win32-clang-g++
else
QT_SPEC := win32-g++
endif

endif

ifdef HAVE_CROSS_COMPILE
QT_PLATFORM := -xplatform $(QT_SPEC) -device-option CROSS_COMPILE=$(HOST)-
else
ifneq ($(QT_SPEC),)
QT_PLATFORM := -platform $(QT_SPEC)
endif
endif

QT_CONFIG := -static -opensource -confirm-license $(QT_OPENGL) -no-pkg-config \
	-no-sql-sqlite -no-gif -no-openssl -no-dbus -no-vulkan -no-sql-odbc -no-pch \
	-no-feature-testlib -no-feature-itemmodeltester -no-feature-sqlmodel -no-feature-sql \
	-no-feature-xml -no-feature-printer -no-feature-concurrent -no-compile-examples -nomake examples -nomake tests \
	-system-freetype -system-harfbuzz -system-libjpeg -system-libpng -system-zlib

# For now, we only build Qt in release mode. In debug mode, startup is prevented by the internal ANGLE
# throwing an assertion in debug mode, but only when built with clang. See issue 27476.
QT_CONFIG += -release

ifeq ($(V),1)
QT_CONFIG += -verbose
endif

ifdef HAVE_MINGW_W64
QT_CONFIG += -no-direct2d
endif

QT_ENV_VARS := $(HOSTVARS) DXSDK_DIR=$(PREFIX)/bin
QT_QINSTALL="$(shell cd $(SRC)/qt/; pwd -P)/install_wrapper.sh"

qmake_toolchain = echo "!host_build {"    > $(1)/.qmake.cache && \
	echo "  QMAKE_C        = $(CC)"      >> $(1)/.qmake.cache && \
	echo "  QMAKE_CXX      = $(CXX)"     >> $(1)/.qmake.cache && \
	echo "  QMAKE_CFLAGS   += -isystem $(PREFIX)/include $(CFLAGS)" >> $(1)/.qmake.cache && \
	echo "  QMAKE_CXXFLAGS += -isystem $(PREFIX)/include $(CXXFLAGS)" >> $(1)/.qmake.cache && \
	echo "  QMAKE_LFLAGS   += $(LDFLAGS)"  >> $(1)/.qmake.cache && \
	echo "} else {"                        >> $(1)/.qmake.cache && \
	echo "  QMAKE_C        = $(BUILDCC)"   >> $(1)/.qmake.cache && \
	echo "  QMAKE_CXX      = $(BUILDCXX)"  >> $(1)/.qmake.cache && \
	echo "  QMAKE_CFLAGS   += $(BUILDCFLAGS)"   >> $(1)/.qmake.cache && \
	echo "  QMAKE_CXXFLAGS += $(BUILDCXXFLAGS)" >> $(1)/.qmake.cache && \
	echo "  QMAKE_LFLAGS   += $(BUILDLDFLAGS)"  >> $(1)/.qmake.cache && \
	echo "}"                                           >> $(1)/.qmake.cache && \
	echo "CONFIG -= debug_and_release" >> $(1)/.qmake.cache && \
	echo "CONFIG += object_parallel_to_source create_pc force_bootstrap" >> $(1)/.qmake.cache


.qt: qt
	$(call qmake_toolchain, $<)
	+cd $< && $(QT_ENV_VARS) ./configure $(QT_PLATFORM) $(QT_CONFIG) -prefix $(PREFIX) -hostprefix $(PREFIX)/lib/qt5 \
	    $(shell $(SRC)/qt/configure-env.py $(CPPFLAGS) $(LDFLAGS))
	# Make && Install libraries
	cd $< && $(QT_ENV_VARS) $(MAKE)
	$(MAKE) -C $< -C src \
		INSTALL_FILE=$(QT_QINSTALL) VLC_PREFIX="$(PREFIX)" \
		sub-corelib-install_subtargets \
		sub-gui-install_subtargets \
		sub-widgets-install_subtargets \
		sub-platformsupport-install_subtargets \
		sub-bootstrap-install_subtargets \
		sub-network-install_subtargets
	# Install tools
	$(MAKE) -C $< -C src \
		sub-moc-install_subtargets \
		sub-rcc-install_subtargets \
		sub-uic-install_subtargets \
		sub-qlalr-install_subtargets
	# Install plugins
	$(MAKE) -C $< -C src/plugins \
		INSTALL_FILE=$(QT_QINSTALL) VLC_PREFIX="$(PREFIX)" \
		sub-imageformats-install_subtargets \
		sub-platforms-install_subtargets \
		sub-styles-install_subtargets

ifdef HAVE_WIN32
	# Add the ANGLE headers to our project
	sed -i.orig -e 's#-I$${includedir}/QtGui#-I$${includedir}/QtGui -I$${includedir}/QtANGLE#' $(PREFIX)/lib/pkgconfig/Qt5Gui.pc
endif

	#fix host tools headers to avoid collusion with target headers
	mkdir -p $(PREFIX)/lib/qt5/include
	cp -R $(PREFIX)/include/QtCore $(PREFIX)/lib/qt5/include
	sed -i.orig -e "s#\$\$QT_MODULE_INCLUDE_BASE#$(PREFIX)/lib/qt5/include#g" $(PREFIX)/lib/qt5/mkspecs/modules/qt_lib_bootstrap_private.pri
	# Install a qmake with correct paths set
	$(MAKE) -C $< sub-qmake-qmake-aux-pro-install_subtargets install_mkspecs
ifdef HAVE_WIN32
	# Install libqtmain for potentially other targets, eg. docs/ samples
	$(MAKE) -C "$</src/winmain" all
	$(MAKE) -C "$</src/winmain" install
endif
	touch $@

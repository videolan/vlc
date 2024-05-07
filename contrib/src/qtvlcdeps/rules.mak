# qtvlcdeps

DEPS_qtvlcdeps += qt $(DEPS_qt) qtsvg $(DEPS_qtsvg) qtdeclarative $(DEPS_qtdeclarative) qt5compat $(DEPS_qt5compat)

ifdef HAVE_LINUX
DEPS_qtvlcdeps += qtwayland $(DEPS_qtwayland)
endif

ifneq ($(findstring qt,$(PKGS)),)
PKGS += qtvlcdeps
endif

ifeq ($(call need_pkg,"qtvlcdeps >= 0.1"),)
PKGS_FOUND += qtvlcdeps
endif

QT_VLC_DEP_SOURCES := Imports.qml qtvlcdeps.pc.in CMakeLists.txt

.sum-qtvlcdeps:
	touch $@

qtvlcdeps: UNPACK_DIR=qtvlcdeps-unpack
qtvlcdeps:
	$(RM) -R $@ && mkdir -p $(UNPACK_DIR)
	for f in $(QT_VLC_DEP_SOURCES) ; do \
	  cp -f $(SRC)/qtvlcdeps/$$f $(UNPACK_DIR) ; \
	done
	$(MOVE)

.qtvlcdeps: qtvlcdeps
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) -G Ninja -DCMAKE_TOOLCHAIN_FILE=$(PREFIX)/lib/cmake/Qt6/qt.toolchain.cmake
	QT_LIBS=$$(awk -F '=' '/LINK_LIBRARIES/ {print $$2; exit}' $(BUILD_DIR)/build.ninja); \
	  cat $</qtvlcdeps.pc.in                         | \
	  sed "s|%1|$$QT_LIBS|"                          | \
	  sed "s|$(PREFIX)/lib/|$$\{libdir\}/|g"         | \
	  sed "s|$(PREFIX)/plugins/|$$\{pluginsdir\}/|g" | \
	  sed "s|$(PREFIX)/qml/|$$\{qmldir\}/|g"         | \
	  sed "s|$(PREFIX)/|$$\{prefix\}/|g"             | \
	  sed "s|@@CONTRIB_PREFIX@@|$(PREFIX)|" > $</qtvlcdeps.pc
	install -d $(PREFIX)/lib/pkgconfig && install $</qtvlcdeps.pc $(PREFIX)/lib/pkgconfig
	touch $@

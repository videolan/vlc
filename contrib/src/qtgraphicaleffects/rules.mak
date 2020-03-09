# QtGraphicalEffects

QTGE_VERSION_MAJOR := 5.12
QTGE_VERSION := $(QTGE_VERSION_MAJOR).7
QTGE_URL := http://download.qt.io/official_releases/qt/$(QTGE_VERSION_MAJOR)/$(QTGE_VERSION)/submodules/qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz

DEPS_qtgraphicaleffects += qtdeclarative $(DEPS_qtdeclarative)

ifdef HAVE_WIN32
PKGS += qtgraphicaleffects
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtgraphicaleffects
endif

$(TARBALLS)/qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz:
	$(call download_pkg,$(QTGE_URL),qt)

.sum-qtgraphicaleffects: qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz

qtgraphicaleffects: qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz .sum-qtgraphicaleffects
	$(UNPACK)
	$(MOVE)

.qtgraphicaleffects: qtgraphicaleffects
	cd $< && $(PREFIX)/bin/qmake
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-effects-install_subtargets
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5QuickWidgets qml/QtGraphicalEffects qtgraphicaleffectsplugin
	$(SRC)/qt/AddStaticLink.sh "$(PREFIX)" Qt5QuickWidgets qml/QtGraphicalEffects/private qtgraphicaleffectsprivate
	touch $@

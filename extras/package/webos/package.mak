# webOS build targets — delegates to build-webos.sh at the repository root.
# Usage: make -f extras/package/webos/package.mak webos

WEBOS_DEPLOY_DIR ?= $(abs_top_srcdir)/vlc-webos-deploy
WEBOS_APP_ID ?= org.videolan.vlc
WEBOS_APP_VERSION ?= 1.0.0
WEBOS_TARGET_ARCH ?= arm
WEBOS_OUTPUT_DIR ?= $(abs_top_srcdir)/webos-package

webos-deps:
	"$(abs_top_srcdir)/build-webos.sh" deps

webos-configure:
	"$(abs_top_srcdir)/build-webos.sh" configure

webos-build:
	"$(abs_top_srcdir)/build-webos.sh" build

webos-install:
	"$(abs_top_srcdir)/build-webos.sh" install

webos-package:
	"$(abs_top_srcdir)/extras/package/webos/package.sh"

webos: webos-deps webos-configure webos-build webos-install webos-package

.PHONY: webos webos-deps webos-configure webos-build webos-install webos-package

webos-build: webos-configure
	$(MAKE) -C "$(WEBOS_BUILDDIR)"

webos-install: webos-build
	$(MKDIR_P) "$(WEBOS_DEPLOY_DIR)"
	$(MAKE) -C "$(WEBOS_BUILDDIR)" install DESTDIR="$(WEBOS_DEPLOY_DIR)"

webos-ipk: webos-install
	WEBOS_HOST="$(WEBOS_HOST)" \
	WEBOS_DEPLOY_DIR="$(WEBOS_DEPLOY_DIR)" \
	WEBOS_APP_ID="$(WEBOS_APP_ID)" \
	WEBOS_APP_VERSION="$(WEBOS_APP_VERSION)" \
	WEBOS_TARGET_ARCH="$(WEBOS_TARGET_ARCH)" \
	WEBOS_OUTPUT_DIR="$(WEBOS_OUTPUT_DIR)" \
	"$(abs_top_srcdir)/extras/package/webos/package.sh"

webos-all: webos-contrib webos-build
webos-all-ipk: webos-contrib webos-ipk

.PHONY: webos-contrib webos-configure webos-build webos-install webos-ipk webos-all webos-all-ipk

EXTRA_DIST += \
	extras/package/webos/env.build.sh \
	extras/package/webos/configure.sh \
	extras/package/webos/build.sh \
	extras/package/webos/package.sh \
	extras/package/webos/package.mak

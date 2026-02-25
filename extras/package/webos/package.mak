WEBOS_BUILDDIR ?= $(top_builddir)/build-webos
WEBOS_CONTRIB_BUILDDIR ?= $(abs_top_srcdir)/contrib/contrib-webos
WEBOS_HOST ?= arm-webos-linux-gnueabi
WEBOS_CONTRIB_BOOTSTRAP_FLAGS ?=
WEBOS_DEPLOY_DIR ?= $(abs_top_srcdir)/vlc-webos-deploy
WEBOS_APP_ID ?= org.videolan.vlc.webos
WEBOS_APP_VERSION ?= 1.0.0
WEBOS_TARGET_ARCH ?= arm
WEBOS_OUTPUT_DIR ?= $(abs_top_srcdir)/webos-package

webos-contrib:
	$(MKDIR_P) "$(WEBOS_CONTRIB_BUILDDIR)"
	cd "$(WEBOS_CONTRIB_BUILDDIR)" && ../bootstrap --host="$(WEBOS_HOST)" $(WEBOS_CONTRIB_BOOTSTRAP_FLAGS)
	$(MAKE) -C "$(WEBOS_CONTRIB_BUILDDIR)" fetch
	$(MAKE) -C "$(WEBOS_CONTRIB_BUILDDIR)"

webos-configure:
	$(MKDIR_P) "$(WEBOS_BUILDDIR)"
	cd "$(WEBOS_BUILDDIR)" && "$(abs_top_srcdir)/extras/package/webos/configure.sh"

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

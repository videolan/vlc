WEBOS_BUILDDIR ?= $(top_builddir)/build-webos
WEBOS_CONTRIB_BUILDDIR ?= $(abs_top_srcdir)/contrib/contrib-webos
WEBOS_HOST ?= arm-webos-linux-gnueabi
WEBOS_CONTRIB_BOOTSTRAP_FLAGS ?=

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

webos-all: webos-contrib webos-build

.PHONY: webos-contrib webos-configure webos-build webos-all

EXTRA_DIST += \
	extras/package/webos/env.build.sh \
	extras/package/webos/configure.sh \
	extras/package/webos/build.sh \
	extras/package/webos/package.mak

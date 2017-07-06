# HIDAPI

HIDAPI_VERSION := a6a622ffb680c55da0de787ff93b80280498330f
HIDAPI_GITURL := https://github.com/signal11/hidapi.git

$(TARBALLS)/hidapi-git.tar.xz:
	$(call download_git,$(HIDAPI_GITURL),,$(HIDAPI_VERSION))

.sum-hidapi: hidapi-git.tar.xz
	$(call check_githash,$(HIDAPI_VERSION))
	touch $@

hidapi: hidapi-git.tar.xz .sum-hidapi
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.hidapi: hidapi
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-static --disable-shared
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@

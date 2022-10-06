# rav1e-vendor

rav1e-vendor-build: .cargo .sum-rav1e
	-$(RM) -R $@
	mkdir -p $@
	tar xzfo $(TARBALLS)/rav1e-$(RAV1E_VERSION).tar.gz -C $@ --strip-components=1
	cd $@ && $(CARGO) vendor --locked rav1e-$(RAV1E_VERSION)-vendor
	cd $@ && tar -jcf rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 rav1e-$(RAV1E_VERSION)-vendor
	install $@/rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 "$(TARBALLS)"
	# cd $@ && sha512sum rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 > SHA512SUMS
	# install $@/SHA512SUMS $(SRC)/rav1e-vendor/SHA512SUMS
	$(RM) -R $@

$(TARBALLS)/rav1e-$(RAV1E_VERSION)-vendor.tar.bz2:
	$(call download_vendor,rav1e-$(RAV1E_VERSION)-vendor.tar.bz2,rav1e)
	if test ! -s "$@"; then $(MAKE) rav1e-vendor-build; fi

.sum-rav1e-vendor: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2
	touch $@

rav1e-vendor: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 .sum-rav1e-vendor
	$(UNPACK)
	$(MOVE)

# we may not need cargo if the tarball is downloaded, but it will be needed by rav1e anyway
DEPS_rav1e-vendor = cargo $(DEPS_cargo)

.rav1e-vendor: rav1e-vendor
	touch $@

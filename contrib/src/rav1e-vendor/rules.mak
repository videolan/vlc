# rav1e-vendor

$(TARBALLS)/rav1e-$(RAV1E_VERSION)-vendor.tar.bz2:
	$(call download_vendor,rav1e-$(RAV1E_VERSION)-vendor.tar.bz2,rav1e)

.rav1e-vendor: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2

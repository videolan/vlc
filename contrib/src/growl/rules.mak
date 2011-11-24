# growl

GROWL_VERSION := 1.2.2
GROWL_URL := http://growl.googlecode.com/files/Growl-$(GROWL_VERSION)-src.tbz

ifdef HAVE_MACOSX
PKGS += growl
endif

$(TARBALLS)/growl-$(GROWL_VERSION).tar.bz2:
	$(call download,$(GROWL_URL))

.sum-growl: growl-$(GROWL_VERSION).tar.bz2

growl: growl-$(GROWL_VERSION).tar.bz2 .sum-growl
	$(UNPACK)
	mv Growl-1.2.2-src $@
	touch $@

.growl: growl
	cd $< && xcodebuild -sdk /Developer/SDKs/MacOSX10.6.sdk -arch $(ARCH)
	cd $< && cp -R -L build/Release/Growl.framework "$(PREFIX)"
	touch $@

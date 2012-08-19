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
	mv Growl-1.2.2-src growl-1.2.2
	$(APPLY) $(SRC)/growl/growl-xcode4.patch
	sed -i.orig -e s/"REVISION \$$REV"/"REVISION 0x\$$REV"/g growl-1.2.2/generateHgRevision.sh
	$(MOVE)

.growl: growl
	cd $< && xcodebuild $(XCODE_FLAGS) -target Growl.framework -configuration Release
	cd $< && cp -R build/Release/Growl.framework "$(PREFIX)"
	touch $@

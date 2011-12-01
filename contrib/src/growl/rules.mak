# growl

GROWL_VERSION := 1.2.2
GROWL_URL := http://growl.googlecode.com/files/Growl-$(GROWL_VERSION)-src.tbz

ifdef HAVE_MACOSX
PKGS += growl
endif

$(TARBALLS)/growl-$(GROWL_VERSION).tar.bz2:
	$(call download,$(GROWL_URL))

.sum-growl: growl-$(GROWL_VERSION).tar.bz2

ifeq ($(shell clang -v 2>/dev/null || echo FAIL),)
COMPILER=com.apple.compilers.llvm.clang.1_0
else
COMPILER=com.apple.compilers.gcc.4_2
endif

growl: growl-$(GROWL_VERSION).tar.bz2 .sum-growl
	$(UNPACK)
	mv Growl-1.2.2-src $@
	sed -i.orig -e s/"SDKROOT = macosx10.5"/"SDKROOT = macosx$(OSX_VERSION)"/g \
		-e s/"GCC_VERSION = 4.0"/"GCC_VERSION = $(COMPILER)"/g \
		-e s/com.apple.compilers.gcc.4_0/$(COMPILER)/g \
		$@/Growl.xcodeproj/project.pbxproj
	sed -i.orig -e s/"REVISION \$$REV"/"REVISION 0x\$$REV"/g $@/generateHgRevision.sh
	touch $@

.growl: growl
	cd $< && xcodebuild $(XCODE_FLAGS) -target Growl.framework -configuration Release
	cd $< && cp -R -L build/Release/Growl.framework "$(PREFIX)"
	touch $@

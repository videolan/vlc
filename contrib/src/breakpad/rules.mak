# Breakpad

# This is the VideoLAN fork of Breakpad, not Google Breakpad!
BREAKPAD_VERSION := 0.1.3
BREAKPAD_URL := http://download.videolan.org/pub/contrib/breakpad/breakpad-$(BREAKPAD_VERSION).tar.gz

ifdef HAVE_MACOSX
# fails to build on newer SDK because of missing libarclite (found in 13.1, missing in 13.3 from XCode 14.3)
ifeq ($(call darwin_sdk_at_most, 13.1), true)
CAN_BUILD_BREAKPAD:=1
endif
ifeq ($(call darwin_min_os_at_least, 10.11), true)
# builds when targeting macOS 10.11
CAN_BUILD_BREAKPAD:=1
endif
endif

ifdef CAN_BUILD_BREAKPAD
PKGS += breakpad
endif

$(TARBALLS)/breakpad-$(BREAKPAD_VERSION).tar.gz:
	$(call download_pkg,$(BREAKPAD_URL),breakpad)

.sum-breakpad: breakpad-$(BREAKPAD_VERSION).tar.gz

breakpad: breakpad-$(BREAKPAD_VERSION).tar.gz .sum-breakpad
	$(UNPACK)
	$(APPLY) $(SRC)/breakpad/0001-mac-client-Upgrade-Breakpad.xib-to-new-format.patch
	$(APPLY) $(SRC)/breakpad/windows-arm64.patch
	sed -i.orig -e "s/GCC_TREAT_WARNINGS_AS_ERRORS = YES/GCC_TREAT_WARNINGS_AS_ERRORS = NO/" "$(UNPACK_DIR)/src/common/mac/Breakpad.xcconfig"
	$(MOVE)

BREAKPAD_CONF := --disable-processor

.breakpad: breakpad
	# Framework
ifdef HAVE_MACOSX
	cd $</src/client/mac/ && xcodebuild $(XCODE_FLAGS) CLANG_CXX_LIBRARY=libc++
	cd $</src/client/mac/ && \
		mkdir -p "$(PREFIX)/Frameworks" && \
		rm -Rf $(PREFIX)/Frameworks/Breakpad.framework && \
		cp -R build/Release/Breakpad.framework "$(PREFIX)/Frameworks"
	# Tools
	cd $</src/tools/mac/dump_syms && \
		xcodebuild $(XCODE_FLAGS) CLANG_CXX_LIBRARY=libc++ && \
		install -d "$(PREFIX)/bin" && \
		install build/Release/dump_syms "$(PREFIX)/bin"
else
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(BREAKPAD_CONF)
	Configuration=Release $(MAKE) -C $< install
endif
	touch $@

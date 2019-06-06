# Breakpad

# This is the VideoLAN fork of Breakpad, not Google Breakpad!
BREAKPAD_VERSION := 0.1.3
BREAKPAD_URL := http://download.videolan.org/pub/contrib/breakpad/breakpad-$(BREAKPAD_VERSION).tar.gz

ifdef HAVE_MACOSX
PKGS += breakpad
endif

$(TARBALLS)/breakpad-$(BREAKPAD_VERSION).tar.gz:
	$(call download_pkg,$(BREAKPAD_URL),breakpad)

.sum-breakpad: breakpad-$(BREAKPAD_VERSION).tar.gz

breakpad: breakpad-$(BREAKPAD_VERSION).tar.gz .sum-breakpad
	$(UNPACK)
	$(APPLY) $(SRC)/breakpad/0001-mac-client-Upgrade-Breakpad.xib-to-new-format.patch
	$(MOVE)

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
		cp -R build/Release/dump_syms "$(PREFIX)/bin"
else
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-processor
	cd $< && Configuration=Release $(MAKE) install
endif
	touch $@

# breakpad

BREAKPAD_HASH := 94b6309aecaddfcf11672f6cfad9575d68ad3b40
BREAKPAD_URL := https://chromium.googlesource.com/breakpad/breakpad/+archive/$(BREAKPAD_HASH).tar.gz

ifdef HAVE_MACOSX
PKGS += breakpad
endif

$(TARBALLS)/breakpad-$(BREAKPAD_HASH).tar.gz:
	$(call download_pkg,$(BREAKPAD_URL),breakpad)

.sum-breakpad: breakpad-$(BREAKPAD_HASH).tar.gz
	$(warning $@ not implemented)
	touch $@

breakpad: breakpad-$(BREAKPAD_HASH).tar.gz .sum-breakpad
	rm -Rf $@ breakpad-$(BREAKPAD_HASH)
	mkdir breakpad-$(BREAKPAD_HASH)
	tar xvzf $(TARBALLS)/breakpad-$(BREAKPAD_HASH).tar.gz -C breakpad-$(BREAKPAD_HASH)
	$(MOVE)

.breakpad: breakpad
	# Framework
	cd $</src/client/mac/ && xcodebuild $(XCODE_FLAGS) CLANG_CXX_LIBRARY=libc++ WARNING_CFLAGS=-Wno-error
	cd $</src/client/mac/ && \
		mkdir -p "$(PREFIX)/Frameworks" && \
		rm -Rf $(PREFIX)/Frameworks/Breakpad.framework && \
		cp -R build/Release/Breakpad.framework "$(PREFIX)/Frameworks"
	# Tools
	cd $</src/tools/mac/dump_syms && \
		xcodebuild $(XCODE_FLAGS) CLANG_CXX_LIBRARY=libc++ WARNING_CFLAGS=-Wno-error && \
		cp -R build/Release/dump_syms "$(PREFIX)/bin"
	touch $@

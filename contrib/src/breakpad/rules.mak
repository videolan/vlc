# breakpad

BREAKPAD_HASH := bbebd8d5e7d61666c3a2dae82867bb7b5aeda639
BREAKPAD_URL := https://chromium.googlesource.com/breakpad/breakpad/+archive/$(BREAKPAD_HASH).tar.gz

ifdef HAVE_MACOSX
PKGS += breakpad
endif

$(TARBALLS)/breakpad-$(BREAKPAD_HASH).tar.gz:
	$(call download_pkg,$(BREAKPAD_URL),breakpad)

.sum-breakpad: breakpad-$(BREAKPAD_HASH).tar.gz
	$(warning $@ not implemented)
	touch $@

breakpad: breakpad-$(BREAKPAD_HASH).tar.gz
	mkdir $@
	tar xvzf $(TARBALLS)/breakpad-$(BREAKPAD_HASH).tar.gz -C breakpad

.breakpad: breakpad
	cd $</src/client/mac/ && xcodebuild $(XCODE_FLAGS) CLANG_CXX_LIBRARY=libc++ WARNING_CFLAGS=-Wno-error
	cd $</src/tools/mac/dump_syms && xcodebuild $(XCODE_FLAGS) CLANG_CXX_LIBRARY=libc++ WARNING_CFLAGS=-Wno-error
	install -d $(PREFIX)
	cd $</src/client/mac/ && cp -R build/Release/Breakpad.framework "$(PREFIX)"
	cd $</src/tools/mac/dump_syms && cp -R build/Release/dump_syms "$(PREFIX)/bin"
	touch $@

if ENABLE_MACOSX_UI

if ENABLE_MACOSX_XCTESTS

macosx_gui_xctest_frameworks_dir = $(shell $(XCRUN) --sdk macosx --show-sdk-platform-path)/Developer/Library/Frameworks
macosx_gui_xctest_bundles = VLCMacOSXTests.xctest

check_PROGRAMS += VLCMacOSXTests
TESTS += $(macosx_gui_xctest_bundles)

VLCMacOSXTests_SOURCES = \
	gui/macosx/tests/VLCPositionFormatterTest.m \
	gui/macosx/extensions/VLCPositionFormatter.m

VLCMacOSXTests_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-F"$(macosx_gui_xctest_frameworks_dir)" \
	-I$(srcdir)/gui/macosx

VLCMacOSXTests_OBJCFLAGS = \
	$(AM_OBJCFLAGS) \
	-fblocks \
	-fobjc-arc \
	-fobjc-exceptions \
	-fmodules

VLCMacOSXTests_LDFLAGS = \
	-bundle \
	-F"$(macosx_gui_xctest_frameworks_dir)" \
	-Wl,-rpath,"$(macosx_gui_xctest_frameworks_dir)" \
	-Wl,-framework,XCTest \
	-Wl,-framework,Foundation

VLCMacOSXTests_LDADD =

VLCMacOSXTests.xctest: VLCMacOSXTests$(EXEEXT) $(srcdir)/gui/macosx/tests/VLCMacOSXTests-Info.plist
	$(AM_V_GEN)rm -rf $@
	$(AM_V_at)$(MKDIR_P) $@/Contents/MacOS
	$(AM_V_at)$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install \
		$(INSTALL_PROGRAM) VLCMacOSXTests$(EXEEXT) \
		$(abs_builddir)/$@/Contents/MacOS/VLCMacOSXTests
	$(AM_V_at)$(INSTALL_DATA) $(srcdir)/gui/macosx/tests/VLCMacOSXTests-Info.plist $@/Contents/Info.plist

EXTRA_DIST += gui/macosx/tests/VLCMacOSXTests-Info.plist

mostlyclean-local: mostlyclean-local-macosx-xctest
mostlyclean-local-macosx-xctest:
	rm -rf $(macosx_gui_xctest_bundles)

check-macosx: VLCMacOSXTests.xctest
	$(XCTEST_LOG_COMPILER) $<

else

check-macosx:
	@echo "XCTest-based macOS GUI tests require a native macOS build; skipping."

endif

.PHONY: check-macosx mostlyclean-local-macosx-xctest

endif

if ENABLE_MACOSX_UI

if ENABLE_MACOSX_XCTESTS

macosx_gui_xctest_frameworks_dir = $(shell $(XCRUN) --sdk macosx --show-sdk-platform-path)/Developer/Library/Frameworks
macosx_gui_xctest_bundles = VLCMacOSXTests.xctest

check_PROGRAMS += VLCMacOSXTests
TESTS += $(macosx_gui_xctest_bundles)

VLCMacOSXTests_SOURCES = \
	gui/macosx/tests/VLCAppKitMockingTestCase.m \
	gui/macosx/tests/VLCHexNumberFormatterTest.m \
	gui/macosx/tests/NSStringHelpersTest.m \
	gui/macosx/tests/VLCLibraryDataTypesTest.m \
	gui/macosx/tests/VLCLibraryDataTypesTestSupport.m \
	gui/macosx/tests/VLCInputItemTest.m \
	gui/macosx/tests/VLCInputItemTestSupport.m \
	gui/macosx/tests/VLCInputNodeTest.m \
	gui/macosx/tests/VLCMediaSourceTableViewSortingTest.m \
	gui/macosx/tests/VLCLibraryDataTypesIntegrationTest.m \
	gui/macosx/tests/VLCLibraryDataTypesIntegrationTestSupport.m \
	gui/macosx/tests/VLCLibraryHomeBrowseCardTest.m \
	gui/macosx/tests/VLCLibraryHomeBrowseCarouselTest.m \
	gui/macosx/tests/VLCLibraryHomeTestSupport.m \
	gui/macosx/tests/VLCLogMessageTest.m \
	gui/macosx/tests/VLCMacOSXTestDefaults.m \
	gui/macosx/tests/VLCPositionFormatterTest.m \
	gui/macosx/tests/VLCTimeFormatterTest.m \
	gui/macosx/tests/VLCLibraryHomeViewSectionHeaderViewTest.m \
	gui/macosx/tests/VLCVisualEffectViewTest.m \
	gui/macosx/extensions/NSAnimationContext+VLCAdditions.m \
	gui/macosx/extensions/NSColor+VLCAdditions.m \
	gui/macosx/extensions/NSTextField+VLCAdditions.m \
	gui/macosx/extensions/VLCHexNumberFormatter.m \
	gui/macosx/extensions/VLCPositionFormatter.m \
	gui/macosx/extensions/NSString+Helpers.m \
	gui/macosx/extensions/NSAppearance+VLCAdditions.m \
	gui/macosx/extensions/NSFont+VLCAdditions.m \
	gui/macosx/extensions/NSView+VLCAdditions.m \
	gui/macosx/library/home-library/VLCLibraryHomeViewSectionHeaderView.m \
	gui/macosx/library/VLCLibraryCarouselView.m \
	gui/macosx/library/VLCLibraryCollectionViewDelegate.m \
	gui/macosx/library/VLCLibraryCollectionViewFlowLayout.m \
	gui/macosx/library/VLCLibraryDataTypes.m \
	gui/macosx/library/VLCInputItem.m \
	gui/macosx/library/VLCInputNode.m \
	gui/macosx/library/home-library/VLCLibraryHomeBrowseCarouselView.m \
	gui/macosx/library/home-library/VLCLibraryHomeBrowseCollectionViewItem.m \
	gui/macosx/library/home-library/VLCLibraryHomeViewBaseCarouselContainerView.m \
	gui/macosx/library/home-library/VLCLibraryHomeViewBrowseCarouselContainerView.m \
	gui/macosx/library/home-library/VLCLibraryHomeViewBrowseCarouselItemCell.m \
	gui/macosx/library/home-library/VLCLibraryHomeViewBrowseCarouselItemView.m \
	gui/macosx/library/media-source/VLCMediaSourceTableViewSorting.m \
	gui/macosx/windows/logging/VLCLogMessage.m \
	gui/macosx/views/VLCCarouselView.m \
	gui/macosx/views/VLCSubScrollView.m \
	gui/macosx/views/VLCTimeFormatter.m \
	gui/macosx/views/VLCUIUnits.m \
	gui/macosx/views/VLCVisualEffectView.m

noinst_HEADERS += \
	gui/macosx/tests/VLCAppKitMockingTestCase.h \
	gui/macosx/tests/VLCInputItemTestSupport.h \
	gui/macosx/tests/VLCLibraryDataTypesTestSupport.h \
	gui/macosx/tests/VLCLibraryDataTypesIntegrationTestSupport.h \
	gui/macosx/tests/VLCMacOSXTestDefaults.h

VLCMacOSXTests_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DVLC_MACOSX_TEST_SOURCE_DIR=\"$(abs_top_srcdir)\" \
	-F"$(macosx_gui_xctest_frameworks_dir)" \
	-I$(srcdir)/gui/macosx \
	-I$(top_srcdir)

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
	-Wl,-rpath,"$(abs_top_builddir)/src/.libs" \
	-Wl,-rpath,"$(abs_top_builddir)/lib/.libs" \
	-Wl,-framework,XCTest \
	-Wl,-framework,Foundation \
	-Wl,-framework,Cocoa \
	-Wl,-framework,CoreServices \
	-Wl,-framework,CoreVideo \
	-Wl,-framework,IOKit \
	-Wl,-framework,QuartzCore

VLCMacOSXTests_LDADD = \
	$(top_builddir)/lib/libvlc.la \
	$(top_builddir)/src/libvlccore.la \
	-lsqlite3

VLCMacOSXTests_LINK = $(LIBTOOL) $(AM_V_lt) --tag=CC \
	$(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(OBJCLD) \
	$(VLCMacOSXTests_OBJCFLAGS) $(OBJCFLAGS) \
	$(VLCMacOSXTests_LDFLAGS) $(LDFLAGS) -o $@

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

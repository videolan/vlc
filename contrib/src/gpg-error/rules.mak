# GPGERROR
GPGERROR_VERSION := 1.56
GPGERROR_URL := $(GNUGPG)/libgpg-error/libgpg-error-$(GPGERROR_VERSION).tar.bz2

$(TARBALLS)/libgpg-error-$(GPGERROR_VERSION).tar.bz2:
	$(call download_pkg,$(GPGERROR_URL),gpg-error)

PKGS += gpg-error
ifeq ($(call need_pkg,"gpg-error >= 1.33"),)
PKGS_FOUND += gpg-error
endif

.sum-gpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2

libgpg-error: libgpg-error-$(GPGERROR_VERSION).tar.bz2 .sum-gpg-error
	$(UNPACK)
	$(call pkg_static,"src/gpg-error.pc.in")
	# $(call update_autoconfig,build-aux)
	# gpg-error doesn't know about mingw32uwp but it's the same as mingw32
	$(APPLY) $(SRC)/gpg-error/gpg-error-uwp-fix.patch

	# use CreateFile2 in Win8 as CreateFileW is forbidden in UWP
	$(APPLY) $(SRC)/gpg-error/0004-use-WCHAR-API-for-temporary-windows-folder.patch
	$(APPLY) $(SRC)/gpg-error/gpg-error-createfile2.patch

	# don't use GetFileSize on UWP
	$(APPLY) $(SRC)/gpg-error/gpg-error-uwp-GetFileSize.patch
	$(APPLY) $(SRC)/gpg-error/0007-don-t-use-GetThreadLocale-on-UWP.patch
	$(APPLY) $(SRC)/gpg-error/0008-don-t-use-GetUserNameW-on-Windows-10.patch
	$(APPLY) $(SRC)/gpg-error/0009-gpg-error-config.in-add-missing-GPG_ERROR_CONFIG_LIB.patch
	$(APPLY) $(SRC)/gpg-error/0011-logging-add-ws2tcpip.h-include-for-proper-inet_pton-.patch
	$(APPLY) $(SRC)/gpg-error/0012-use-GetCurrentProcessId-in-UWP.patch
	$(APPLY) $(SRC)/gpg-error/0013-configure-allow-building-Windows-with-disable-thread.patch
	$(APPLY) $(SRC)/gpg-error/0014-core-disable-locking-API-with-disable-threads.patch
	$(APPLY) $(SRC)/gpg-error/0015-core-disable-process-spawning-with-disable-threads.patch

	$(MOVE)

GPGERROR_CONF := \
	--disable-nls \
	--disable-languages \
	--disable-tests \
	--disable-doc \
	--enable-install-gpg-error-config \
	--disable-threads

.gpg-error: libgpg-error
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(GPGERROR_CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@

# libtasn1

LIBTASN1_VERSION := 4.19.0
LIBTASN1_URL := $(GNU)/libtasn1/libtasn1-$(LIBTASN1_VERSION).tar.gz

ifeq ($(call need_pkg,"libtasn1 >= 4.3"),)
PKGS_FOUND += libtasn1
endif

DEPS_libtasn1 :=
ifdef HAVE_WINSTORE
# gnulib uses GetFileInformationByHandle
DEPS_libtasn1 += alloweduwp $(DEPS_alloweduwp)
endif


$(TARBALLS)/libtasn1-$(LIBTASN1_VERSION).tar.gz:
	$(call download_pkg,$(LIBTASN1_URL),libtasn1)

.sum-libtasn1: libtasn1-$(LIBTASN1_VERSION).tar.gz

libtasn1: libtasn1-$(LIBTASN1_VERSION).tar.gz .sum-libtasn1
	$(UNPACK)
	$(call update_autoconfig,build-aux)

	# use CreateFile2 in Win8 as CreateFileW is forbidden in UWP
	$(APPLY) $(SRC)/libtasn1/0001-Use-CreateFile2-in-UWP-builds.patch

	$(MOVE)

LIBTASN1_CONF := --disable-doc

.libtasn1: libtasn1
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(LIBTASN1_CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@

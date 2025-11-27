GNU=https://ftpmirror.gnu.org/gnu
APACHE=https://downloads.apache.org/
SF=https://downloads.sourceforge.net/project
VIDEOLAN=https://downloads.videolan.org/pub/contrib

NASM_VERSION=2.14
NASM_URL=https://www.nasm.us/pub/nasm/releasebuilds/$(NASM_VERSION)/nasm-$(NASM_VERSION).tar.gz

CMAKE_VERSION_MAJ=4.1
CMAKE_VERSION=$(CMAKE_VERSION_MAJ).2
CMAKE_URL=https://www.cmake.org/files/v$(CMAKE_VERSION_MAJ)/cmake-$(CMAKE_VERSION).tar.gz

LIBTOOL_VERSION=2.5.4
LIBTOOL_URL=$(GNU)/libtool/libtool-$(LIBTOOL_VERSION).tar.gz

AUTOCONF_VERSION=2.72
AUTOCONF_URL=$(GNU)/autoconf/autoconf-$(AUTOCONF_VERSION).tar.gz

AUTOMAKE_VERSION=1.18
AUTOMAKE_URL=$(GNU)/automake/automake-$(AUTOMAKE_VERSION).tar.gz

M4_VERSION=1.4.20
M4_URL=$(GNU)/m4/m4-$(M4_VERSION).tar.gz

PKGCFG_VERSION=0.28-1
PKGCFG_URL=$(SF)/pkgconfiglite/$(PKGCFG_VERSION)/pkg-config-lite-$(PKGCFG_VERSION).tar.gz

TAR_VERSION=1.34
TAR_URL=$(GNU)/tar/tar-$(TAR_VERSION).tar.bz2

XZ_VERSION=5.4.1
XZ_URL=https://tukaani.org/xz/xz-$(XZ_VERSION).tar.bz2

SED_VERSION=4.2.2
SED_URL=$(GNU)/sed/sed-$(SED_VERSION).tar.bz2

ANT_VERSION=1.9.7
ANT_URL=$(APACHE)/ant/binaries/apache-ant-$(ANT_VERSION)-bin.tar.bz2

BISON_VERSION=3.8.2
BISON_URL=$(GNU)/bison/bison-$(BISON_VERSION).tar.xz

FLEX_VERSION=2.6.4
FLEX_URL=https://github.com/westes/flex/releases/download/v$(FLEX_VERSION)/flex-$(FLEX_VERSION).tar.gz

GETTEXT_VERSION=0.26
GETTEXT_URL=$(GNU)/gettext/gettext-$(GETTEXT_VERSION).tar.gz

HELP2MAN_VERSION=1.47.6
HELP2MAN_URL=$(GNU)/help2man/help2man-$(HELP2MAN_VERSION).tar.xz

MESON_VERSION=1.9.1
MESON_URL=https://github.com/mesonbuild/meson/releases/download/$(MESON_VERSION)/meson-$(MESON_VERSION).tar.gz

NINJA_VERSION=1.11.1
NINJA_BUILD_NAME=$(NINJA_VERSION).g95dee.kitware.jobserver-1
NINJA_URL=https://github.com/Kitware/ninja/archive/refs/tags/v$(NINJA_BUILD_NAME).tar.gz

GPERF_VERSION=3.1
GPERF_URL=$(GNU)/gperf/gperf-$(GPERF_VERSION).tar.gz

CONFIGGUESS_VERSION=a2287c3041a3f2a204eb942e09c015eab00dc7dd
CONFIGSUB_VERSION=a2287c3041a3f2a204eb942e09c015eab00dc7dd

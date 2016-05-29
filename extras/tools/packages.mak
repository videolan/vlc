GNU=http://ftp.gnu.org/gnu
APACHE=http://mir2.ovh.net/ftp.apache.org/dist
SF= http://downloads.sourceforge.net/project
VIDEOLAN=http://downloads.videolan.org/pub/contrib

YASM_VERSION=1.2.0
#YASM_URL=$(VIDEOLAN)/yasm-$(YASM_VERSION).tar.gz
YASM_URL=http://www.tortall.net/projects/yasm/releases/yasm-$(YASM_VERSION).tar.gz

CMAKE_VERSION=3.4.3
CMAKE_URL=http://www.cmake.org/files/v3.4/cmake-$(CMAKE_VERSION).tar.gz

LIBTOOL_VERSION=2.4.2
LIBTOOL_URL=$(GNU)/libtool/libtool-$(LIBTOOL_VERSION).tar.gz

AUTOCONF_VERSION=2.69
AUTOCONF_URL=$(GNU)/autoconf/autoconf-$(AUTOCONF_VERSION).tar.gz

AUTOMAKE_VERSION=1.15
AUTOMAKE_URL=$(GNU)/automake/automake-$(AUTOMAKE_VERSION).tar.gz

M4_VERSION=1.4.16
M4_URL=$(GNU)/m4/m4-$(M4_VERSION).tar.gz

PKGCFG_VERSION=0.28-1
#PKGCFG_URL=http://downloads.videolan.org/pub/videolan/testing/contrib/pkg-config-$(PKGCFG_VERSION).tar.gz
PKGCFG_URL=$(SF)/pkgconfiglite/$(PKGCFG_VERSION)/pkg-config-lite-$(PKGCFG_VERSION).tar.gz

TAR_VERSION=1.26
TAR_URL=$(GNU)/tar/tar-$(TAR_VERSION).tar.bz2

XZ_VERSION=5.2.2
XZ_URL=http://tukaani.org/xz/xz-$(XZ_VERSION).tar.bz2

GAS_VERSION=72887b9
GAS_URL=http://git.libav.org/?p=gas-preprocessor.git;a=snapshot;h=$(GAS_VERSION);sf=tgz

RAGEL_VERSION=6.8
RAGEL_URL=http://www.colm.net/files/ragel/ragel-$(RAGEL_VERSION).tar.gz

SED_VERSION=4.2.2
SED_URL=$(GNU)/sed/sed-$(SED_VERSION).tar.bz2

ANT_VERSION=1.9.7
ANT_URL=$(VIDEOLAN)/apache-ant-$(ANT_VERSION)-bin.tar.bz2

PROTOBUF_VERSION := 2.6.1
PROTOBUF_URL := https://github.com/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-$(PROTOBUF_VERSION).tar.bz2

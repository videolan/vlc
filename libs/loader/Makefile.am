
# Makefile to build MPlayer's DLL loader
# from VLC trunk
#
# by Rémi Denis-Courmont <rem # videolan.org>
# $Id$
#
# All other files in this directory (and subdirectories) are copyrighted
# by their respective owner(s).
#

noinst_LTLIBRARIES = libloader.la

# TODO: real check for vsscanf() and <sys/mmap.h>
AM_CPPFLAGS = -D__WINE__ -DHAVE_SYS_MMAN_H=1 -DHAVE_VSSCANF=1 \
	-DWIN32_PATH=\"\" -DTRACE\(...\)=\(void\)0
AM_CFLAGS = -fno-PIC `$(top_builddir)/vlc-config --cflags libs` -U_FILE_OFFSET_BITS \
	-fno-omit-frame-pointer -mno-omit-leaf-frame-pointer

noinst_HEADERS = com.h driver.h ext.h ldt_keeper.h loader.h registry.h \
	win32.h wineacm.h wrapper.h qtx/qtxsdk/components.h \
	wine/avifmt.h wine/basetsd.h wine/debugtools.h wine/driver.h \
	wine/heap.h wine/mmreg.h wine/module.h wine/msacmdrv.h wine/msacm.h \
	wine/pe_image.h wine/poppack.h wine/pshpack1.h wine/pshpack2.h \
	wine/vfw.h wine/winbase.h wine/windef.h wine/winerror.h \
	wine/winestring.h wine/winnt.h wine/winreg.h wine/winuser.h

libloader_la_SOURCES = ldt_keeper.c pe_image.c module.c ext.c win32.c \
	driver.c pe_resource.c resource.c registry.c afl.c vfl.c \
	wrapper.S stubs.s kludge.c
libloader_la_LIBADD = -lpthread


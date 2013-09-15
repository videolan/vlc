SUBDIRS = rtp
noinst_LTLIBRARIES =
access_LTLIBRARIES =
EXTRA_LTLIBRARIES =

include ../common.am
accessdir = $(pluginsdir)/access

### Files ###

libattachment_plugin_la_SOURCES = attachment.c
access_LTLIBRARIES += libattachment_plugin.la

libfilesystem_plugin_la_SOURCES = fs.h file.c directory.c fs.c
if HAVE_WIN32
libfilesystem_plugin_la_LIBADD = -lshlwapi
endif
access_LTLIBRARIES += libfilesystem_plugin.la

libidummy_plugin_la_SOURCES = idummy.c
access_LTLIBRARIES += libidummy_plugin.la

libimem_plugin_la_SOURCES = imem.c
libimem_plugin_la_LIBADD = $(LIBM)
access_LTLIBRARIES += libimem_plugin.la

librar_plugin_la_SOURCES = rar/rar.c rar/rar.h \
	rar/access.c rar/stream.c rar/module.c
access_LTLIBRARIES += librar_plugin.la

libsdp_plugin_la_SOURCES = sdp.c
access_LTLIBRARIES += libsdp_plugin.la

libtimecode_plugin_la_SOURCES = timecode.c
access_LTLIBRARIES += libtimecode_plugin.la

libvdr_plugin_la_SOURCES = vdr.c
access_LTLIBRARIES += libvdr_plugin.la

libzip_plugin_la_SOURCES = zip/zip.h zip/zipstream.c zip/zipaccess.c
libzip_plugin_la_CFLAGS = $(AM_CFLAGS) $(MINIZIP_CFLAGS)
libzip_plugin_la_LIBADD = $(MINIZIP_LIBS)
libunzip_la_SOURCES = zip/unzip/unzip.c zip/unzip/unzip.h zip/unzip/crypt.h \
	zip/unzip/ioapi.c zip/unzip/ioapi.h
libunzip_la_LIBADD = -lz
libunzip_la_LDFLAGS = -static
if HAVE_ZLIB
access_LTLIBRARIES += libzip_plugin.la
if !HAVE_MINIZIP
noinst_LTLIBRARIES += libunzip.la
libzip_plugin_la_CFLAGS += -I$(srcdir)/zip/unzip
libzip_plugin_la_LIBADD += libunzip.la
endif
endif


### Audio capture ###

libaccess_alsa_plugin_la_SOURCES = alsa.c
libaccess_alsa_plugin_la_CFLAGS = $(AM_CFLAGS) $(ALSA_CFLAGS)
libaccess_alsa_plugin_la_LIBADD = $(ALSA_LIBS)
if HAVE_ALSA
access_LTLIBRARIES += libaccess_alsa_plugin.la
endif

libaccess_oss_plugin_la_SOURCES = oss.c
libaccess_oss_plugin_la_LIBADD = $(OSS_LIBS)
if HAVE_OSS
access_LTLIBRARIES += libaccess_oss_plugin.la
endif

libaccess_jack_plugin_la_SOURCES = jack.c
libaccess_jack_plugin_la_CFLAGS = $(AM_CFLAGS) $(JACK_CFLAGS)
libaccess_jack_plugin_la_LIBADD = $(JACK_LIBS)
if HAVE_JACK
access_LTLIBRARIES += libaccess_jack_plugin.la
endif

libpulsesrc_plugin_la_SOURCES = pulse.c
libpulsesrc_plugin_la_CFLAGS= $(AM_CFLAGS) $(PULSE_CFLAGS)
libpulsesrc_plugin_la_LIBADD = ../libvlc_pulse.la $(PULSE_LIBS)
if HAVE_PULSE
access_LTLIBRARIES += libpulsesrc_plugin.la
endif

libqtsound_plugin_la_SOURCES = qtsound.m
libqtsound_plugin_la_LDFLAGS = -Wl,-framework,QTKit -Wl,-framework,CoreAudio
if HAVE_QTKIT
access_LTLIBRARIES += libqtsound_plugin.la
endif


### Video capture ###

libavcapture_plugin_la_SOURCES = avcapture.m
libavcapture_plugin_la_LDFLAGS = \
	-Wl,-framework,AVFoundation -Wl,-framework,CoreMedia
if HAVE_AVFOUNDATION
access_LTLIBRARIES += libavcapture_plugin.la
endif

libdc1394_plugin_la_SOURCES = dc1394.c
libdc1394_plugin_la_CFLAGS = $(AM_CFLAGS) $(DC1394_CFLAGS)
libdc1394_plugin_la_LIBADD = $(DC1394_LIBS)
libdc1394_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdc1394)
EXTRA_LTLIBRARIES += libdc1394_plugin.la

libdv1394_plugin_la_SOURCES = dv.c
libdv1394_plugin_la_CFLAGS = $(AM_CFLAGS) $(DV1394_CFLAGS)
libdv1394_plugin_la_LIBADD = $(DV1394_LIBS)
libdv1394_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdv1394)
EXTRA_LTLIBRARIES += libdv1394_plugin.la

liblinsys_hdsdi_plugin_la_SOURCES = \
	linsys/linsys_sdiaudio.h \
	linsys/linsys_sdivideo.h \
	linsys/linsys_hdsdi.c
liblinsys_hdsdi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
liblinsys_sdi_plugin_la_SOURCES = linsys/linsys_sdi.c linsys/linsys_sdi.h
liblinsys_sdi_plugin_la_CFLAGS = $(AM_CFLAGS) $(LINSYS_SDI_CFLAGS)
liblinsys_sdi_plugin_la_LIBADD = $(LINSYS_SDI_LIBS)
liblinsys_sdi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBlinsys_hdsdi) $(LTLIBlinsys_sdi)
EXTRA_LTLIBRARIES += liblinsys_hdsdi_plugin.la liblinsys_sdi_plugin.la

libdecklink_plugin_la_SOURCES = decklink.cpp
libdecklink_plugin_la_CXXFLAGS = $(AM_CFLAGS) $(CPPFLAGS_decklink)
libdecklink_plugin_la_LIBADD = $(LIBS_decklink) -ldl
if HAVE_DECKLINK
access_LTLIBRARIES += libdecklink_plugin.la
endif

libshm_plugin_la_SOURCES = shm.c
libshm_plugin_la_LIBADD = $(LIBM)
if HAVE_WIN32
libshm_plugin_la_DEPENDENCIES = libshm_plugin.rc.o
endif
access_LTLIBRARIES += libshm_plugin.la

libqtcapture_plugin_la_SOURCES = qtcapture.m
libqtcapture_plugin_la_LDFLAGS = \
	-Wl,-framework,QTKit -Wl,-framework,CoreVideo -Wl,-framework,QuartzCore
if HAVE_QTKIT
access_LTLIBRARIES += libqtcapture_plugin.la
endif

libv4l2_plugin_la_SOURCES = \
	v4l2/linux/videodev2.h \
	v4l2/linux/v4l2-common.h \
	v4l2/linux/v4l2-controls.h \
	v4l2/v4l2.c \
	v4l2/video.c \
	v4l2/vbi.c \
	v4l2/demux.c \
	v4l2/access.c \
	v4l2/radio.c \
	v4l2/controls.c \
	v4l2/lib.c \
	v4l2/v4l2.h
libv4l2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/v4l2
libv4l2_plugin_la_CFLAGS = $(AM_CFLAGS) $(ZVBI_CFLAGS)
libv4l2_plugin_la_LIBADD = $(LIBDL) $(LIBM) $(ZVBI_LIBS)
if HAVE_V4L2
access_LTLIBRARIES += libv4l2_plugin.la
endif

libdshow_plugin_la_SOURCES = dshow/vlc_dshow.h dshow/dshow.cpp dshow/access.h \
	dshow/filter.cpp dshow/filter.h dshow/crossbar.cpp
libdshow_plugin_la_LIBADD = -lole32 -loleaut32 -luuid -lstrmiids -lksuser
if HAVE_WIN32
access_LTLIBRARIES += libdshow_plugin.la
libdshow_plugin_la_DEPENDENCIES = libdshow_plugin.rc.o
endif


### Screen grab ###

libxcb_screen_plugin_la_SOURCES = screen/xcb.c
libxcb_screen_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(XCB_COMPOSITE_CFLAGS) $(XCB_SHM_CFLAGS)
libxcb_screen_plugin_la_LIBADD = $(XCB_LIBS) $(XCB_COMPOSITE_LIBS) $(XCB_SHM_LIBS)
if HAVE_XCB
access_LTLIBRARIES += libxcb_screen_plugin.la
endif

libscreen_plugin_la_SOURCES = screen/screen.c screen/screen.h
libscreen_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_WIN32
libscreen_plugin_la_SOURCES += screen/win32.c
libscreen_plugin_la_LIBADD = -lgdi32
access_LTLIBRARIES += libscreen_plugin.la
libscreen_plugin_la_DEPENDENCIES = libscreen_plugin.rc.o
endif
if HAVE_MAC_SCREEN
libscreen_plugin_la_SOURCES += screen/mac.c
libscreen_plugin_la_LDFLAGS += "-Wl,-framework,OpenGL,-framework,ApplicationServices"
access_LTLIBRARIES += libscreen_plugin.la
endif

librdp_plugin_la_SOURCES = rdp.c
librdp_plugin_la_CFLAGS = $(AM_CFLAGS) $(FREERDP_CFLAGSp)
librdp_plugin_la_LIBADD = $(FREERDP_LIBS)
librdp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBrdp)
EXTRA_LTLIBRARIES += librdp_plugin.la

libvnc_plugin_la_SOURCES = vnc.c
libvnc_plugin_la_CFLAGS = $(AM_CFLAGS) $(VNC_CFLAGS)
libvnc_plugin_la_LIBADD = $(VNC_LIBS)
libvnc_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBvnc)
EXTRA_LTLIBRARIES += libvnc_plugin.la


### Optical media ###

libcdda_plugin_la_SOURCES = cdda.c vcd/cdrom.c vcd/cdrom.h vcd/cdrom_internals.h
if HAVE_DARWIN
libcdda_plugin_la_LIBADD = -liconv
endif
libcdda_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
EXTRA_LTLIBRARIES += libcdda_plugin.la
access_LTLIBRARIES += $(LTLIBcdda)

libvcd_plugin_la_SOURCES = vcd/vcd.c vcd/cdrom.c vcd/cdrom.h vcd/cdrom_internals.h
if HAVE_DARWIN
libvcd_plugin_la_LIBADD = -liconv
endif
libvcd_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
EXTRA_LTLIBRARIES += libvcd_plugin.la
access_LTLIBRARIES += $(LTLIBvcd)

libvcdx_plugin_la_SOURCES = \
	vcdx/access.h vcdx/access.c \
	vcdx/vcd.c vcdx/vcd.h \
	vcdx/vcdplayer.h vcdx/vcdplayer.c \
	vcdx/info.c vcdx/info.h
libvcdx_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_WIN32
libvcdx_plugin_la_LIBADD = -lwinmm
endif
libvcdx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
EXTRA_LTLIBRARIES += libvcdx_plugin.la
access_LTLIBRARIES += $(LTLIBvcdx)

libdvdnav_plugin_la_SOURCES = dvdnav.c
libdvdnav_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVDNAV_CFLAGS)
libdvdnav_plugin_la_LIBADD = $(DVDNAV_LIBS)
libdvdnav_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdvdnav)
EXTRA_LTLIBRARIES += libdvdnav_plugin.la

libdvdread_plugin_la_SOURCES = dvdread.c
libdvdread_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVDREAD_CFLAGS)
libdvdread_plugin_la_LIBADD = $(DVDREAD_LIBS)
libdvdread_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdvdread)
EXTRA_LTLIBRARIES += libdvdread_plugin.la

liblibbluray_plugin_la_SOURCES = bluray.c
liblibbluray_plugin_la_CFLAGS = $(AM_CFLAGS) $(BLURAY_CFLAGS)
liblibbluray_plugin_la_LIBADD = $(BLURAY_LIBS)
liblibbluray_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBlibbluray)
EXTRA_LTLIBRARIES += liblibbluray_plugin.la

libaccess_bd_plugin_la_SOURCES = bd/bd.c bd/mpls.c bd/mpls.h bd/clpi.c bd/clpi.h
access_LTLIBRARIES += libaccess_bd_plugin.la
if HAVE_WIN32
libaccess_bd_plugin_la_DEPENDENCIES = libaccess_bd_plugin.rc.o
endif


### Digital TV ###

libdtv_plugin_la_SOURCES = \
	dtv/dtv.h \
	dtv/access.c
libdtv_plugin_la_CFLAGS = $(AM_CFLAGS)

if HAVE_LINUX_DVB
libdtv_plugin_la_SOURCES += dtv/linux.c
libdtv_plugin_la_CFLAGS += -DHAVE_LINUX_DVB
if HAVE_DVBPSI
libdtv_plugin_la_SOURCES += dtv/en50221.c dtv/en50221.h ../mux/mpeg/dvbpsi_compat.h
libdtv_plugin_la_CFLAGS += -DHAVE_DVBPSI $(DVBPSI_CFLAGS)
libdtv_plugin_la_LIBADD = $(DVBPSI_LIBS)
endif
access_LTLIBRARIES += libdtv_plugin.la
endif

if HAVE_WIN32
# Work-around automake bug:
# Use an import library for C++ to retain C linker on other platforms.
libbda_la_SOURCES = dtv/bdadefs.h dtv/bdagraph.cpp dtv/bdagraph.hpp
libbda_la_LDFLAGS = -static
libbda_la_LIBADD = -lstrmiids -lole32 -loleaut32 -luuid
noinst_LTLIBRARIES += libbda.la

libdtv_plugin_la_DEPENDENCIES = libbda.la libdtv_plugin.rc.o
libdtv_plugin_la_LIBADD = libbda.la -lstdc++
access_LTLIBRARIES += libdtv_plugin.la
endif

# Old Linux DVB scanner
libdvb_plugin_la_SOURCES = \
	dvb/access.c \
	dvb/linux_dvb.c \
	dvb/scan.c dvb/scan.h \
	dvb/dvb.h
libdvb_plugin_la_CFLAGS = $(AM_CFLAGS)
if HAVE_LINUX_DVB
if HAVE_DVBPSI
libdvb_plugin_la_SOURCES += ../mux/mpeg/dvbpsi_compat.h
access_LTLIBRARIES += libdvb_plugin.la
libdvb_plugin_la_CFLAGS += $(DVBPSI_CFLAGS)
libdvb_plugin_la_LIBADD = $(DVBPSI_LIBS)
endif
endif

libaccess_eyetv_plugin_la_SOURCES = eyetv.m
libaccess_eyetv_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)' \
	-Wl,-framework,Foundation
access_LTLIBRARIES += $(LTLIBaccess_eyetv)
EXTRA_LTLIBRARIES += libaccess_eyetv_plugin.la


### Network streams ###

libftp_plugin_la_SOURCES = ftp.c
libftp_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libftp_plugin.la

libhttp_plugin_la_SOURCES = http.c
libhttp_plugin_la_LIBADD = $(SOCKET_LIBS)
if HAVE_ZLIB
libhttp_plugin_la_LIBADD += -lz
endif
access_LTLIBRARIES += libhttp_plugin.la

liblive555_plugin_la_SOURCES = live555.cpp mms/asf.c mms/buffer.c
liblive555_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_live555)
liblive555_plugin_la_LIBADD = $(LIBS_live555)
liblive555_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)' \
	$(LDFLAGS_live555)
access_LTLIBRARIES += $(LTLIBlive555)
EXTRA_LTLIBRARIES += liblive555_plugin.la

libaccess_mms_plugin_la_SOURCES = \
	mms/mms.c mms/mms.h \
	mms/mmsh.c mms/mmsh.h \
	mms/mmstu.c mms/mmstu.h \
	mms/buffer.c mms/buffer.h \
	mms/asf.h mms/asf.c \
	../demux/asf/libasf_guid.h
libaccess_mms_plugin_la_CFLAGS = $(AM_CFLAGS)
libaccess_mms_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libaccess_mms_plugin.la

libsmb_plugin_la_SOURCES = smb.c
libsmb_plugin_la_CFLAGS = $(AM_CFLAGS) $(SMBCLIENT_CFLAGS)
libsmb_plugin_la_LIBADD = $(SMBCLIENT_LIBS)
if HAVE_WIN32
libsmb_plugin_la_LIBADD += -lmpr
endif
libsmb_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBsmb)
EXTRA_LTLIBRARIES += libsmb_plugin.la

libtcp_plugin_la_SOURCES = tcp.c
libtcp_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libtcp_plugin.la

libudp_plugin_la_SOURCES = udp.c
libudp_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libudp_plugin.la

libsftp_plugin_la_SOURCES = sftp.c
libsftp_plugin_la_CFLAGS = $(AM_CFLAGS) $(SFTP_CFLAGS)
libsftp_plugin_la_LIBADD = $(SFTP_LIBS)
libsftp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBsftp)
EXTRA_LTLIBRARIES += libsftp_plugin.la

libaccess_realrtsp_plugin_la_SOURCES = \
	rtsp/access.c \
	rtsp/rtsp.c rtsp/rtsp.h \
	rtsp/real.c rtsp/real.h \
	rtsp/real_rmff.c rtsp/real_rmff.h \
	rtsp/real_sdpplin.c rtsp/real_sdpplin.h \
	rtsp/real_asmrp.c
libaccess_realrtsp_plugin_la_LIBADD = $(SOCKET_LIBS)
libaccess_realrtsp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBaccess_realrtsp)
EXTRA_LTLIBRARIES += libaccess_realrtsp_plugin.la

libavio_plugin_la_SOURCES = avio.c avio.h ../codec/avcodec/cpu.c
libavio_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS)
libavio_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(SYMBOLIC_LDFLAGS)
libavio_plugin_la_LIBADD = $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(LIBM)
if HAVE_AVFORMAT
access_LTLIBRARIES += libavio_plugin.la
endif


### Misc ###

libgnomevfs_plugin_la_SOURCES = gnomevfs.c
libgnomevfs_plugin_la_CFLAGS = $(AM_CFLAGS) $(GNOMEVFS_CFLAGS)
libgnomevfs_plugin_la_LIBADD = $(GNOMEVFS_LIBS)
libgnomevfs_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBgnomevfs)
EXTRA_LTLIBRARIES += libgnomevfs_plugin.la

libaccess_mtp_plugin_la_SOURCES = mtp.c
libaccess_mtp_plugin_la_CFLAGS = $(AM_CFLAGS) $(MTP_CFLAGS)
libaccess_mtp_plugin_la_LIBADD = $(MTP_LIBS)
libaccess_mtp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBaccess_mtp)
EXTRA_LTLIBRARIES += libaccess_mtp_plugin.la

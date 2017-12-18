accessdir = $(pluginsdir)/access
access_LTLIBRARIES =

AM_CPPFLAGS += -I$(srcdir)/access

### Files ###

libattachment_plugin_la_SOURCES = access/attachment.c
access_LTLIBRARIES += libattachment_plugin.la

libdcp_plugin_la_SOURCES = access/dcp/dcpparser.h access/dcp/dcp.cpp access/dcp/dcpparser.cpp access/dcp/dcpdecrypt.cpp
if HAVE_ASDCP
libdcp_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(ASDCP_CFLAGS)
libdcp_plugin_la_LIBADD = $(AM_LIBADD) $(ASDCP_LIBS)
if HAVE_GCRYPT
libdcp_plugin_la_CPPFLAGS += $(GCRYPT_CFLAGS)
libdcp_plugin_la_LIBADD += $(GCRYPT_LIBS)
access_LTLIBRARIES += libdcp_plugin.la
endif
endif

libfilesystem_plugin_la_SOURCES = access/fs.h access/file.c access/directory.c access/fs.c
libfilesystem_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_WIN32
libfilesystem_plugin_la_LIBADD = -lshlwapi
endif
access_LTLIBRARIES += libfilesystem_plugin.la

libidummy_plugin_la_SOURCES = access/idummy.c
access_LTLIBRARIES += libidummy_plugin.la

libimem_plugin_la_SOURCES = access/imem-access.c
libimem_plugin_la_LIBADD = $(LIBM)
access_LTLIBRARIES += libimem_plugin.la

libaccess_imem_plugin_la_SOURCES = access/imem.c
access_LTLIBRARIES += libaccess_imem_plugin.la

libsdp_plugin_la_SOURCES = access/sdp.c
access_LTLIBRARIES += libsdp_plugin.la

libtimecode_plugin_la_SOURCES = access/timecode.c
access_LTLIBRARIES += libtimecode_plugin.la

libvdr_plugin_la_SOURCES = access/vdr.c
access_LTLIBRARIES += libvdr_plugin.la

### Audio capture ###

libaccess_alsa_plugin_la_SOURCES = access/alsa.c
libaccess_alsa_plugin_la_CFLAGS = $(AM_CFLAGS) $(ALSA_CFLAGS)
libaccess_alsa_plugin_la_LIBADD = $(ALSA_LIBS)
if HAVE_ALSA
access_LTLIBRARIES += libaccess_alsa_plugin.la
endif

libaccess_oss_plugin_la_SOURCES = access/oss.c
libaccess_oss_plugin_la_LIBADD = $(OSS_LIBS)
if HAVE_OSS
access_LTLIBRARIES += libaccess_oss_plugin.la
endif

libaccess_jack_plugin_la_SOURCES = access/jack.c
libaccess_jack_plugin_la_CFLAGS = $(AM_CFLAGS) $(JACK_CFLAGS)
libaccess_jack_plugin_la_LIBADD = $(JACK_LIBS)
if HAVE_JACK
access_LTLIBRARIES += libaccess_jack_plugin.la
endif

libpulsesrc_plugin_la_SOURCES = access/pulse.c
libpulsesrc_plugin_la_CFLAGS= $(AM_CFLAGS) $(PULSE_CFLAGS)
libpulsesrc_plugin_la_LIBADD = libvlc_pulse.la $(PULSE_LIBS)
if HAVE_PULSE
access_LTLIBRARIES += libpulsesrc_plugin.la
endif

libaccess_qtsound_plugin_la_SOURCES = access/qtsound.m
libaccess_qtsound_plugin_la_LDFLAGS = -Wl,-framework,QTKit -Wl,-framework,CoreAudio
access_LTLIBRARIES += $(LTLIBaccess_qtsound)
EXTRA_LTLIBRARIES += libaccess_qtsound_plugin.la

libaccess_wasapi_plugin_la_SOURCES = access/wasapi.c
libaccess_wasapi_plugin_la_LIBADD = $(LIBCOM) -lksuser
if HAVE_WASAPI
if !HAVE_WINSTORE
access_LTLIBRARIES += libaccess_wasapi_plugin.la
endif
endif


### Video capture ###

libavcapture_plugin_la_SOURCES = access/avcapture.m
libavcapture_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc
libavcapture_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)' -Wl,-framework,Foundation -Wl,-framework,AVFoundation -Wl,-framework,CoreMedia,  -Wl,-framework,CoreVideo
if HAVE_AVFOUNDATION
access_LTLIBRARIES += libavcapture_plugin.la
endif

libdc1394_plugin_la_SOURCES = access/dc1394.c
libdc1394_plugin_la_CFLAGS = $(AM_CFLAGS) $(DC1394_CFLAGS)
libdc1394_plugin_la_LIBADD = $(DC1394_LIBS)
libdc1394_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdc1394)
EXTRA_LTLIBRARIES += libdc1394_plugin.la

libdv1394_plugin_la_SOURCES = access/dv.c
libdv1394_plugin_la_CFLAGS = $(AM_CFLAGS) $(DV1394_CFLAGS)
libdv1394_plugin_la_LIBADD = $(DV1394_LIBS) $(LIBPTHREAD)
libdv1394_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdv1394)
EXTRA_LTLIBRARIES += libdv1394_plugin.la

liblinsys_hdsdi_plugin_la_SOURCES = \
	access/linsys/linsys_sdiaudio.h \
	access/linsys/linsys_sdivideo.h \
	access/linsys/linsys_hdsdi.c
liblinsys_hdsdi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
liblinsys_hdsdi_plugin_la_LIBADD = $(LIBPTHREAD)
liblinsys_sdi_plugin_la_SOURCES = access/linsys/linsys_sdi.c access/linsys/linsys_sdi.h
liblinsys_sdi_plugin_la_CFLAGS = $(AM_CFLAGS) $(LINSYS_SDI_CFLAGS)
liblinsys_sdi_plugin_la_LIBADD = $(LINSYS_SDI_LIBS)
liblinsys_sdi_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBlinsys_hdsdi) $(LTLIBlinsys_sdi)
EXTRA_LTLIBRARIES += liblinsys_hdsdi_plugin.la liblinsys_sdi_plugin.la

libdecklink_plugin_la_SOURCES = access/decklink.cpp access/sdi.c access/sdi.h
libdecklink_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(CPPFLAGS_decklink)
libdecklink_plugin_la_LIBADD = $(LIBS_decklink) $(LIBDL) -lpthread
if HAVE_DECKLINK
access_LTLIBRARIES += libdecklink_plugin.la
endif

libshm_plugin_la_SOURCES = access/shm.c
libshm_plugin_la_LIBADD = $(LIBM)
access_LTLIBRARIES += libshm_plugin.la

libv4l2_plugin_la_SOURCES = \
	access/v4l2/linux/videodev2.h \
	access/v4l2/linux/v4l2-common.h \
	access/v4l2/linux/v4l2-controls.h \
	access/v4l2/v4l2.c \
	access/v4l2/video.c \
	access/v4l2/vbi.c \
	access/v4l2/demux.c \
	access/v4l2/access.c \
	access/v4l2/radio.c \
	access/v4l2/controls.c \
	access/v4l2/lib.c \
	access/v4l2/v4l2.h
libv4l2_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/access/v4l2
libv4l2_plugin_la_CFLAGS = $(AM_CFLAGS) $(ZVBI_CFLAGS)
libv4l2_plugin_la_LIBADD = $(LIBDL) $(LIBM) $(ZVBI_LIBS) $(LIBPTHREAD)
if HAVE_V4L2
access_LTLIBRARIES += libv4l2_plugin.la
endif

libdshow_plugin_la_SOURCES = access/dshow/vlc_dshow.h access/dshow/dshow.cpp access/dshow/access.h \
	access/dshow/filter.cpp access/dshow/filter.h access/dshow/crossbar.cpp
libdshow_plugin_la_LIBADD = $(LIBCOM) -loleaut32 -luuid -lstrmiids -lksuser
if HAVE_WIN32_DESKTOP
access_LTLIBRARIES += libdshow_plugin.la
endif


### Screen grab ###

libxcb_screen_plugin_la_SOURCES = access/screen/xcb.c
libxcb_screen_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS) $(XCB_COMPOSITE_CFLAGS) $(XCB_SHM_CFLAGS)
libxcb_screen_plugin_la_LIBADD = $(XCB_LIBS) $(XCB_COMPOSITE_LIBS) $(XCB_SHM_LIBS)
if HAVE_XCB
access_LTLIBRARIES += libxcb_screen_plugin.la
endif

libscreen_plugin_la_SOURCES = access/screen/screen.c access/screen/screen.h
libscreen_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_WIN32_DESKTOP
libscreen_plugin_la_SOURCES += access/screen/win32.c
libscreen_plugin_la_LIBADD = -lgdi32
access_LTLIBRARIES += libscreen_plugin.la
endif
if HAVE_MAC_SCREEN
libscreen_plugin_la_SOURCES += access/screen/mac.c
libscreen_plugin_la_LDFLAGS += "-Wl,-framework,OpenGL,-framework,ApplicationServices"
access_LTLIBRARIES += libscreen_plugin.la
endif

librdp_plugin_la_SOURCES = access/rdp.c
librdp_plugin_la_CFLAGS = $(AM_CFLAGS) $(FREERDP_CFLAGS)
librdp_plugin_la_LIBADD = $(FREERDP_LIBS)
librdp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBrdp)
EXTRA_LTLIBRARIES += librdp_plugin.la

libvnc_plugin_la_SOURCES = access/vnc.c
libvnc_plugin_la_CFLAGS = $(AM_CFLAGS) $(VNC_CFLAGS)
libvnc_plugin_la_LIBADD = $(VNC_LIBS)
libvnc_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBvnc)
EXTRA_LTLIBRARIES += libvnc_plugin.la


### Optical media ###

libcdda_plugin_la_SOURCES = access/cdda.c access/vcd/cdrom.c access/vcd/cdrom.h access/vcd/cdrom_internals.h
libcdda_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBCDDB_CFLAGS)
libcdda_plugin_la_LIBADD = $(LIBCDDB_LIBS) $(LIBM)
libcdda_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
if HAVE_DARWIN
libcdda_plugin_la_LIBADD += -liconv
libcdda_plugin_la_LDFLAGS += -Wl,-framework,IOKit,-framework,CoreFoundation
endif
EXTRA_LTLIBRARIES += libcdda_plugin.la
access_LTLIBRARIES += $(LTLIBcdda)

libvcd_plugin_la_SOURCES = access/vcd/vcd.c access/vcd/cdrom.c access/vcd/cdrom.h access/vcd/cdrom_internals.h
libvcd_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
if HAVE_DARWIN
libvcd_plugin_la_LIBADD = -liconv
libvcd_plugin_la_LDFLAGS += -Wl,-framework,IOKit,-framework,CoreFoundation
endif
EXTRA_LTLIBRARIES += libvcd_plugin.la
access_LTLIBRARIES += $(LTLIBvcd)

libdvdnav_plugin_la_SOURCES = access/dvdnav.c demux/mpeg/ps.h demux/mpeg/pes.h
libdvdnav_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVDNAV_CFLAGS)
libdvdnav_plugin_la_LIBADD = $(DVDNAV_LIBS)
libdvdnav_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
if HAVE_DARWIN
libdvdnav_plugin_la_LDFLAGS += -Wl,-framework,IOKit,-framework,CoreFoundation
endif
access_LTLIBRARIES += $(LTLIBdvdnav)
EXTRA_LTLIBRARIES += libdvdnav_plugin.la

libdvdread_plugin_la_SOURCES = access/dvdread.c demux/mpeg/ps.h demux/mpeg/pes.h
libdvdread_plugin_la_CFLAGS = $(AM_CFLAGS) $(DVDREAD_CFLAGS)
libdvdread_plugin_la_LIBADD = $(DVDREAD_LIBS)
libdvdread_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
if HAVE_DARWIN
libdvdread_plugin_la_LDFLAGS += -Wl,-framework,IOKit,-framework,CoreFoundation
endif
access_LTLIBRARIES += $(LTLIBdvdread)
EXTRA_LTLIBRARIES += libdvdread_plugin.la

liblibbluray_plugin_la_SOURCES = access/bluray.c
liblibbluray_plugin_la_CFLAGS = $(AM_CFLAGS) $(BLURAY_CFLAGS)
liblibbluray_plugin_la_LIBADD = $(BLURAY_LIBS)
liblibbluray_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBlibbluray)
EXTRA_LTLIBRARIES += liblibbluray_plugin.la

### Digital TV ###

libdtv_plugin_la_SOURCES = \
	access/dtv/dtv.h \
	access/dtv/access.c
libdtv_plugin_la_CFLAGS = $(AM_CFLAGS)

if HAVE_LINUX_DVB
libdtv_plugin_la_SOURCES += access/dtv/linux.c \
                            access/dtv/en50221.c \
                            access/dtv/en50221.h \
                            access/dtv/en50221_capmt.h
libdtv_plugin_la_CFLAGS += -DHAVE_LINUX_DVB
access_LTLIBRARIES += libdtv_plugin.la
endif

if HAVE_WIN32_DESKTOP
# Work-around automake bug:
# Use an import library for C++ to retain C linker on other platforms.
libbda_la_SOURCES = access/dtv/bdadefs.h access/dtv/bdagraph.cpp access/dtv/bdagraph.hpp
libbda_la_LDFLAGS = -static
libbda_la_LIBADD = -lstrmiids $(LIBCOM) -loleaut32 -luuid
noinst_LTLIBRARIES += libbda.la

libdtv_plugin_la_DEPENDENCIES = libbda.la
libdtv_plugin_la_LIBADD = libbda.la -lstdc++
access_LTLIBRARIES += libdtv_plugin.la
endif

# Old Linux DVB scanner
libdvb_plugin_la_SOURCES = \
	access/dvb/access.c \
	access/dvb/linux_dvb.c \
	access/dvb/scan.c access/dvb/scan.h \
	access/dvb/scan_list.c access/dvb/scan_list.h \
	access/dvb/dvb.h
libdvb_plugin_la_CFLAGS = $(AM_CFLAGS)
if HAVE_LINUX_DVB
if HAVE_DVBPSI
libdvb_plugin_la_SOURCES += mux/mpeg/dvbpsi_compat.h
access_LTLIBRARIES += libdvb_plugin.la
libdvb_plugin_la_CFLAGS += $(DVBPSI_CFLAGS)
libdvb_plugin_la_LIBADD = $(DVBPSI_LIBS)
endif
endif

### Network streams ###

libftp_plugin_la_SOURCES = access/ftp.c
libftp_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libftp_plugin.la

libhttp_plugin_la_SOURCES = access/http.c
libhttp_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libhttp_plugin.la

liblive555_plugin_la_SOURCES = access/live555.cpp access/mms/asf.c access/mms/buffer.c
liblive555_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_live555)
liblive555_plugin_la_LIBADD = $(LIBS_live555) $(SOCKET_LIBS)
liblive555_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)' \
	$(LDFLAGS_live555)
access_LTLIBRARIES += $(LTLIBlive555)
EXTRA_LTLIBRARIES += liblive555_plugin.la

libaccess_mms_plugin_la_SOURCES = \
	access/mms/mms.c access/mms/mms.h \
	access/mms/mmsh.c access/mms/mmsh.h \
	access/mms/mmstu.c access/mms/mmstu.h \
	access/mms/buffer.c access/mms/buffer.h \
	access/mms/asf.h access/mms/asf.c \
	demux/asf/libasf_guid.h
libaccess_mms_plugin_la_CFLAGS = $(AM_CFLAGS)
libaccess_mms_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libaccess_mms_plugin.la

libsmb_plugin_la_SOURCES = access/smb.c access/smb_common.h
libsmb_plugin_la_CFLAGS = $(AM_CFLAGS) $(SMBCLIENT_CFLAGS)
libsmb_plugin_la_LIBADD = $(SMBCLIENT_LIBS)
if HAVE_WIN32
libsmb_plugin_la_LIBADD += -lmpr -lnetapi32
endif
libsmb_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBsmb)
EXTRA_LTLIBRARIES += libsmb_plugin.la

libdsm_plugin_la_SOURCES = access/dsm/access.c access/dsm/sd.c access/smb_common.h
libdsm_plugin_la_CFLAGS = $(AM_CFLAGS) $(DSM_CFLAGS)
libdsm_plugin_la_LIBADD = $(DSM_LIBS)
libdsm_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBdsm)
EXTRA_LTLIBRARIES += libdsm_plugin.la

libtcp_plugin_la_SOURCES = access/tcp.c
libtcp_plugin_la_LIBADD = $(SOCKET_LIBS)
access_LTLIBRARIES += libtcp_plugin.la

libudp_plugin_la_SOURCES = access/udp.c
libudp_plugin_la_LIBADD = $(SOCKET_LIBS) $(LIBPTHREAD)
access_LTLIBRARIES += libudp_plugin.la

libsftp_plugin_la_SOURCES = access/sftp.c
libsftp_plugin_la_CFLAGS = $(AM_CFLAGS) $(SFTP_CFLAGS)
libsftp_plugin_la_LIBADD = $(SFTP_LIBS)
libsftp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBsftp)
EXTRA_LTLIBRARIES += libsftp_plugin.la

libnfs_plugin_la_SOURCES = access/nfs.c
libnfs_plugin_la_CFLAGS = $(AM_CFLAGS) $(NFS_CFLAGS)
libnfs_plugin_la_LIBADD = $(NFS_LIBS) $(SOCKET_LIBS)
libnfs_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBnfs)
EXTRA_LTLIBRARIES += libnfs_plugin.la

libaccess_realrtsp_plugin_la_SOURCES = \
	access/rtsp/access.c \
	access/rtsp/rtsp.c access/rtsp/rtsp.h \
	access/rtsp/real.c access/rtsp/real.h \
	access/rtsp/real_rmff.c access/rtsp/real_rmff.h \
	access/rtsp/real_sdpplin.c access/rtsp/real_sdpplin.h \
	access/rtsp/real_asmrp.c
libaccess_realrtsp_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libaccess_realrtsp_plugin_la_LIBADD = $(SOCKET_LIBS)
libaccess_realrtsp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBaccess_realrtsp)
EXTRA_LTLIBRARIES += libaccess_realrtsp_plugin.la

libavio_plugin_la_SOURCES = access/avio.c access/avio.h
libavio_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS)
libavio_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(SYMBOLIC_LDFLAGS)
libavio_plugin_la_LIBADD = $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(LIBM)
if HAVE_AVFORMAT
if !MERGE_FFMPEG
access_LTLIBRARIES += libavio_plugin.la
endif
endif

libsatip_plugin_la_SOURCES = access/satip.c
libsatip_plugin_la_LIBADD = $(SOCKET_LIBS) $(LIBPTHREAD)
access_LTLIBRARIES += libsatip_plugin.la

### Misc ###

libaccess_concat_plugin_la_SOURCES = access/concat.c
access_LTLIBRARIES += libaccess_concat_plugin.la

libaccess_mtp_plugin_la_SOURCES = access/mtp.c
libaccess_mtp_plugin_la_CFLAGS = $(AM_CFLAGS) $(MTP_CFLAGS)
libaccess_mtp_plugin_la_LIBADD = $(MTP_LIBS)
libaccess_mtp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBaccess_mtp)
EXTRA_LTLIBRARIES += libaccess_mtp_plugin.la

### SRT ###

libaccess_srt_plugin_la_SOURCES = access/srt.c
libaccess_srt_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SRT_CPPFLAGS)
libaccess_srt_plugin_la_LIBADD = $(SRT_LIBS)
libaccess_srt_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(accessdir)'
access_LTLIBRARIES += $(LTLIBaccess_srt)
EXTRA_LTLIBRARIES += libaccess_srt_plugin.la

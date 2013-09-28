stream_filterdir = $(pluginsdir)/stream_filter

stream_filter_LTLIBRARIES =

libdecomp_plugin_la_SOURCES = stream_filter/decomp.c
libdecomp_plugin_la_LIBADD = $(LIBPTHREAD)
if !HAVE_WIN32
stream_filter_LTLIBRARIES += libdecomp_plugin.la
endif

libdash_plugin_la_SOURCES = \
    stream_filter/dash/adaptationlogic/AbstractAdaptationLogic.cpp \
    stream_filter/dash/adaptationlogic/AbstractAdaptationLogic.h \
    stream_filter/dash/adaptationlogic/AdaptationLogicFactory.cpp \
    stream_filter/dash/adaptationlogic/AdaptationLogicFactory.h \
    stream_filter/dash/adaptationlogic/AlwaysBestAdaptationLogic.cpp \
    stream_filter/dash/adaptationlogic/AlwaysBestAdaptationLogic.h \
    stream_filter/dash/adaptationlogic/IAdaptationLogic.h \
    stream_filter/dash/adaptationlogic/IDownloadRateObserver.h \
    stream_filter/dash/adaptationlogic/RateBasedAdaptationLogic.h \
    stream_filter/dash/adaptationlogic/RateBasedAdaptationLogic.cpp \
    stream_filter/dash/buffer/BlockBuffer.cpp \
    stream_filter/dash/buffer/BlockBuffer.h \
    stream_filter/dash/buffer/IBufferObserver.h \
    stream_filter/dash/http/Chunk.cpp \
    stream_filter/dash/http/Chunk.h \
    stream_filter/dash/http/HTTPConnection.cpp \
    stream_filter/dash/http/HTTPConnection.h \
    stream_filter/dash/http/HTTPConnectionManager.cpp \
    stream_filter/dash/http/HTTPConnectionManager.h \
    stream_filter/dash/http/IHTTPConnection.h \
    stream_filter/dash/http/PersistentConnection.cpp \
    stream_filter/dash/http/PersistentConnection.h \
    stream_filter/dash/mpd/AdaptationSet.cpp \
    stream_filter/dash/mpd/AdaptationSet.h \
    stream_filter/dash/mpd/BaseUrl.h \
    stream_filter/dash/mpd/BasicCMManager.cpp \
    stream_filter/dash/mpd/BasicCMManager.h \
    stream_filter/dash/mpd/BasicCMParser.cpp \
    stream_filter/dash/mpd/BasicCMParser.h \
    stream_filter/dash/mpd/CommonAttributesElements.cpp \
    stream_filter/dash/mpd/CommonAttributesElements.h \
    stream_filter/dash/mpd/ContentDescription.cpp \
    stream_filter/dash/mpd/ContentDescription.h \
    stream_filter/dash/mpd/IMPDManager.h \
    stream_filter/dash/mpd/IMPDParser.h \
    stream_filter/dash/mpd/IsoffMainParser.cpp \
    stream_filter/dash/mpd/IsoffMainParser.h \
    stream_filter/dash/mpd/IsoffMainManager.cpp \
    stream_filter/dash/mpd/IsoffMainManager.h \
    stream_filter/dash/mpd/MPD.cpp \
    stream_filter/dash/mpd/MPD.h \
    stream_filter/dash/mpd/MPDFactory.cpp \
    stream_filter/dash/mpd/MPDFactory.h \
    stream_filter/dash/mpd/MPDManagerFactory.cpp \
    stream_filter/dash/mpd/MPDManagerFactory.h \
    stream_filter/dash/mpd/Period.cpp \
    stream_filter/dash/mpd/Period.h \
    stream_filter/dash/mpd/ProgramInformation.cpp \
    stream_filter/dash/mpd/ProgramInformation.h \
    stream_filter/dash/mpd/Representation.cpp \
    stream_filter/dash/mpd/Representation.h \
    stream_filter/dash/mpd/Segment.cpp \
    stream_filter/dash/mpd/Segment.h \
    stream_filter/dash/mpd/SegmentBase.cpp \
    stream_filter/dash/mpd/SegmentBase.h \
    stream_filter/dash/mpd/SegmentInfo.cpp \
    stream_filter/dash/mpd/SegmentInfo.h \
    stream_filter/dash/mpd/SegmentInfoCommon.cpp \
    stream_filter/dash/mpd/SegmentInfoCommon.h \
    stream_filter/dash/mpd/SegmentInfoDefault.cpp \
    stream_filter/dash/mpd/SegmentInfoDefault.h \
    stream_filter/dash/mpd/SegmentList.cpp \
    stream_filter/dash/mpd/SegmentList.h \
    stream_filter/dash/mpd/SegmentTemplate.cpp \
    stream_filter/dash/mpd/SegmentTemplate.h \
    stream_filter/dash/mpd/SegmentTimeline.cpp \
    stream_filter/dash/mpd/SegmentTimeline.h \
    stream_filter/dash/mpd/TrickModeType.cpp \
    stream_filter/dash/mpd/TrickModeType.h \
    stream_filter/dash/xml/DOMHelper.cpp \
    stream_filter/dash/xml/DOMHelper.h \
    stream_filter/dash/xml/DOMParser.cpp \
    stream_filter/dash/xml/DOMParser.h \
    stream_filter/dash/xml/Node.cpp \
    stream_filter/dash/xml/Node.h \
    stream_filter/dash/dash.cpp \
    stream_filter/dash/DASHDownloader.cpp \
    stream_filter/dash/DASHDownloader.h \
    stream_filter/dash/DASHManager.cpp \
    stream_filter/dash/DASHManager.h \
    stream_filter/dash/Helper.cpp \
    stream_filter/dash/Helper.h
libdash_plugin_la_CXXFLAGS = $(AM_CFLAGS) -I$(srcdir)/stream_filter/dash
libdash_plugin_la_LIBADD = $(SOCKET_LIBS)
stream_filter_LTLIBRARIES += libdash_plugin.la

libsmooth_plugin_la_SOURCES = \
    stream_filter/smooth/smooth.c \
    stream_filter/smooth/utils.c \
    stream_filter/smooth/downloader.c \
    stream_filter/smooth/smooth.h

libsmooth_plugin_la_CFLAGS = $(AM_CFLAGS)
stream_filter_LTLIBRARIES += libsmooth_plugin.la

libhttplive_plugin_la_SOURCES = stream_filter/httplive.c
libhttplive_plugin_la_CFLAGS = $(AM_CFLAGS) $(GCRYPT_CFLAGS)
libhttplive_plugin_la_LIBADD = $(GCRYPT_LIBS) -lgpg-error
if HAVE_GCRYPT
stream_filter_LTLIBRARIES += libhttplive_plugin.la
endif

librecord_plugin_la_SOURCES = stream_filter/record.c
stream_filter_LTLIBRARIES += librecord_plugin.la

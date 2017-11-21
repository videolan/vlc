video_filterdir = $(pluginsdir)/video_filter

noinst_HEADERS += video_filter/filter_picture.h

# video filters
libedgedetection_plugin_la_SOURCES = video_filter/edgedetection.c
libedgedetection_plugin_la_LIBADD = $(LIBM)
libadjust_plugin_la_SOURCES = video_filter/adjust.c video_filter/adjust_sat_hue.c video_filter/adjust_sat_hue.h
libadjust_plugin_la_LIBADD = $(LIBM)
libalphamask_plugin_la_SOURCES = video_filter/alphamask.c
libanaglyph_plugin_la_SOURCES = video_filter/anaglyph.c
libantiflicker_plugin_la_SOURCES = video_filter/antiflicker.c
libball_plugin_la_SOURCES = video_filter/ball.c
libball_plugin_la_LIBADD = $(LIBM)
libblendbench_plugin_la_SOURCES = video_filter/blendbench.c
libbluescreen_plugin_la_SOURCES = video_filter/bluescreen.c
libcanvas_plugin_la_SOURCES = video_filter/canvas.c
libcolorthres_plugin_la_SOURCES = video_filter/colorthres.c
libcolorthres_plugin_la_LIBADD = $(LIBM)
libcroppadd_plugin_la_SOURCES = video_filter/croppadd.c
liberase_plugin_la_SOURCES = video_filter/erase.c
libextract_plugin_la_SOURCES = video_filter/extract.c
libextract_plugin_la_LIBADD = $(LIBM)
libfps_plugin_la_SOURCES = video_filter/fps.c
libfreeze_plugin_la_SOURCES = video_filter/freeze.c
libgaussianblur_plugin_la_SOURCES = video_filter/gaussianblur.c
libgaussianblur_plugin_la_LIBADD = $(LIBM)
libgradfun_plugin_la_SOURCES = video_filter/gradfun.c video_filter/gradfun.h
libgradient_plugin_la_SOURCES = video_filter/gradient.c
libgradient_plugin_la_LIBADD = $(LIBM)
libgrain_plugin_la_SOURCES = video_filter/grain.c
libgrain_plugin_la_LIBADD = $(LIBM)
libhqdn3d_plugin_la_SOURCES = video_filter/hqdn3d.c video_filter/hqdn3d.h
libhqdn3d_plugin_la_LIBADD = $(LIBM)
libinvert_plugin_la_SOURCES = video_filter/invert.c
libmagnify_plugin_la_SOURCES = video_filter/magnify.c
libmirror_plugin_la_SOURCES = video_filter/mirror.c
libmotionblur_plugin_la_SOURCES = video_filter/motionblur.c
libmotiondetect_plugin_la_SOURCES = video_filter/motiondetect.c
liboldmovie_plugin_la_SOURCES = video_filter/oldmovie.c
liboldmovie_plugin_la_LIBADD = $(LIBM)
libposterize_plugin_la_SOURCES = video_filter/posterize.c
libpsychedelic_plugin_la_SOURCES = video_filter/psychedelic.c
libpsychedelic_plugin_la_LIBADD = $(LIBM)
libpuzzle_plugin_la_SOURCES = \
	video_filter/puzzle.c video_filter/puzzle.h \
	video_filter/puzzle_bezier.c video_filter/puzzle_bezier.h \
	video_filter/puzzle_lib.c video_filter/puzzle_lib.h \
	video_filter/puzzle_mgt.c video_filter/puzzle_mgt.h \
	video_filter/puzzle_pce.c video_filter/puzzle_pce.h
libpuzzle_plugin_la_LIBADD = $(LIBM)
libripple_plugin_la_SOURCES = video_filter/ripple.c
libripple_plugin_la_LIBADD = $(LIBM)
librotate_plugin_la_SOURCES = video_filter/rotate.c
librotate_plugin_la_LIBADD = libvlc_motion.la $(LIBM)
librotate_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(video_filterdir)'
if HAVE_DARWIN
librotate_plugin_la_LDFLAGS += -Wl,-framework,IOKit,-framework,CoreFoundation
endif
libscale_plugin_la_SOURCES = video_filter/scale.c
libscene_plugin_la_SOURCES = video_filter/scene.c
libscene_plugin_la_LIBADD = $(LIBM)
libsepia_plugin_la_SOURCES = video_filter/sepia.c
libsharpen_plugin_la_SOURCES = video_filter/sharpen.c
libtransform_plugin_la_SOURCES = video_filter/transform.c
libvhs_plugin_la_SOURCES = video_filter/vhs.c
libwave_plugin_la_SOURCES = video_filter/wave.c
libwave_plugin_la_LIBADD = $(LIBM)

video_filter_LTLIBRARIES = \
	libadjust_plugin.la \
	libalphamask_plugin.la \
	libball_plugin.la \
	libblendbench_plugin.la \
	libbluescreen_plugin.la \
	libcanvas_plugin.la \
	libcolorthres_plugin.la \
	libcroppadd_plugin.la \
	libedgedetection_plugin.la \
	liberase_plugin.la \
	libextract_plugin.la \
	libgradient_plugin.la \
	libgrain_plugin.la \
	libgaussianblur_plugin.la \
	libinvert_plugin.la \
	libmagnify_plugin.la \
	libmirror_plugin.la \
	libmotionblur_plugin.la \
	libmotiondetect_plugin.la \
	libposterize_plugin.la \
	libpsychedelic_plugin.la \
	libripple_plugin.la \
	libscale_plugin.la \
	libscene_plugin.la \
	libsepia_plugin.la \
	libsharpen_plugin.la \
	libtransform_plugin.la \
	libwave_plugin.la \
	libgradfun_plugin.la \
	libantiflicker_plugin.la \
	libhqdn3d_plugin.la \
	libanaglyph_plugin.la \
	liboldmovie_plugin.la \
	libvhs_plugin.la \
	libfps_plugin.la \
	libfreeze_plugin.la \
	libpuzzle_plugin.la \
	librotate_plugin.la

# macOS / iOS hardware video filters
libci_filters_plugin_la_SOURCES = video_filter/ci_filters.m codec/vt_utils.c codec/vt_utils.h
if HAVE_OSX
libci_filters_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(video_filterdir)' \
	-Wl,-framework,Foundation -Wl,-framework,CoreGraphics -Wl,-framework,CoreImage -Wl,-framework,CoreVideo
video_filter_LTLIBRARIES += libci_filters_plugin.la
endif

if HAVE_IOS
video_filter_LTLIBRARIES += libci_filters_plugin.la
endif

libdeinterlace_common_la_SOURCES = video_filter/deinterlace/common.c video_filter/deinterlace/common.h
libdeinterlace_common_la_LDFLAGS = -static
noinst_LTLIBRARIES += libdeinterlace_common.la

libdeinterlace_plugin_la_SOURCES = \
	video_filter/deinterlace/deinterlace.c video_filter/deinterlace/deinterlace.h \
        video_filter/deinterlace/mmx.h \
	video_filter/deinterlace/merge.c video_filter/deinterlace/merge.h \
	video_filter/deinterlace/helpers.c video_filter/deinterlace/helpers.h \
	video_filter/deinterlace/algo_basic.c video_filter/deinterlace/algo_basic.h \
	video_filter/deinterlace/algo_x.c video_filter/deinterlace/algo_x.h \
	video_filter/deinterlace/algo_yadif.c video_filter/deinterlace/algo_yadif.h \
	video_filter/deinterlace/yadif.h video_filter/deinterlace/yadif_template.h \
	video_filter/deinterlace/algo_phosphor.c video_filter/deinterlace/algo_phosphor.h \
	video_filter/deinterlace/algo_ivtc.c video_filter/deinterlace/algo_ivtc.h
# inline ASM doesn't build with -O0
libdeinterlace_plugin_la_CFLAGS = $(AM_CFLAGS) -O2
if HAVE_NEON
libdeinterlace_plugin_la_SOURCES += video_filter/deinterlace/merge_arm.S
libdeinterlace_plugin_la_CFLAGS += -DCAN_COMPILE_ARM
endif
if HAVE_ARM64
libdeinterlace_plugin_la_SOURCES += video_filter/deinterlace/merge_arm64.S
libdeinterlace_plugin_la_CFLAGS += -DCAN_COMPILE_ARM64
endif
libdeinterlace_plugin_la_LIBADD = libdeinterlace_common.la
video_filter_LTLIBRARIES += libdeinterlace_plugin.la

libopencv_wrapper_plugin_la_SOURCES = video_filter/opencv_wrapper.c
libopencv_wrapper_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(OPENCV_CFLAGS)
libopencv_wrapper_plugin_la_LIBADD = $(OPENCV_LIBS)
libopencv_wrapper_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(video_filterdir)'
video_filter_LTLIBRARIES += $(LTLIBopencv_wrapper)
EXTRA_LTLIBRARIES += libopencv_wrapper_plugin.la

libpostproc_plugin_la_SOURCES = video_filter/postproc.c
libpostproc_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(POSTPROC_CFLAGS) $(AVUTIL_CFLAGS)
libpostproc_plugin_la_LIBADD = $(LIBM) $(POSTPROC_LIBS) $(AVUTIL_LIBS)
libpostproc_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(video_filterdir)'
video_filter_LTLIBRARIES += $(LTLIBpostproc)
EXTRA_LTLIBRARIES += libpostproc_plugin.la

# misc
libblend_plugin_la_SOURCES = video_filter/blend.cpp
video_filter_LTLIBRARIES += libblend_plugin.la

libopencv_example_plugin_la_SOURCES = video_filter/opencv_example.cpp video_filter/filter_event_info.h
libopencv_example_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(OPENCV_CFLAGS)
libopencv_example_plugin_la_LIBADD = $(OPENCV_LIBS)
libopencv_example_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(video_filterdir)'
video_filter_LTLIBRARIES += $(LTLIBopencv_example)
EXTRA_LTLIBRARIES += libopencv_example_plugin.la

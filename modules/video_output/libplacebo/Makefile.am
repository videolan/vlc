### libplacebo ###

LIBPLACEBO_COMMONSOURCES = video_output/libplacebo/utils.h \
	video_output/libplacebo/instance.h

# Trigger the c++ linker because of glslang dependency of libplacebo
LIBPLACEBO_COMMONSOURCES += dummy.cpp

libplacebo_utils_la_SOURCES = $(LIBPLACEBO_COMMONSOURCES) \
			      video_output/libplacebo/utils.c
libplacebo_utils_la_CFLAGS = $(AM_CFLAGS) $(LIBPLACEBO_CFLAGS)
libplacebo_utils_la_LIBADD = $(LIBPLACEBO_LIBS)
libplacebo_utils_la_LDFLAGS = -static -undefined

noinst_LTLIBRARIES += libplacebo_utils.la

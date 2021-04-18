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

libplacebo_plugin_la_SOURCES = $(LIBPLACEBO_COMMONSOURCES) \
			  video_output/libplacebo/instance.c \
			  video_output/libplacebo/display.c
libplacebo_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBPLACEBO_CFLAGS)
libplacebo_plugin_la_LIBADD = $(LIBPLACEBO_LIBS) libplacebo_utils.la

vout_LTLIBRARIES += libplacebo_plugin.la

### Vulkan ###
libplacebo_vk_plugin_la_SOURCES = $(LIBPLACEBO_COMMONSOURCES) \
				  video_output/vulkan/instance.h \
				  video_output/vulkan/instance.c \
				  video_output/libplacebo/instance_vulkan.c
libplacebo_vk_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBPLACEBO_CFLAGS) $(VULKAN_CFLAGS)
libplacebo_vk_plugin_la_LIBADD = $(LIBPLACEBO_LIBS) $(VULKAN_LIBS)
libplacebo_vk_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(voutdir)'

if HAVE_VULKAN
vout_LTLIBRARIES += $(LTLIBplacebo_vk)
EXTRA_LTLIBRARIES += libplacebo_vk_plugin.la
endif

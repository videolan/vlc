
### Vulkan ###

VULKAN_COMMONSOURCES = video_output/vulkan/instance.c \
	video_output/vulkan/instance.h \
	video_output/vulkan/platform.h \
	video_output/placebo_utils.c \
	video_output/placebo_utils.h

# Trigger the c++ linker because of glslang dependency of libplacebo
VULKAN_COMMONSOURCES += dummy.cpp

VULKAN_COMMONCFLAGS = $(VULKAN_CFLAGS) $(LIBPLACEBO_CFLAGS)
VULKAN_COMMONLIBS = $(VULKAN_LIBS) $(LIBPLACEBO_LIBS)

libvk_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/display.c
libvk_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_COMMONCFLAGS)
libvk_plugin_la_LIBADD = $(VULKAN_COMMONLIBS)

libvk_win32_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/surface.c \
				video_output/vulkan/platform_win32.c
libvk_win32_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_COMMONCFLAGS) \
			       -DVK_USE_PLATFORM_WIN32_KHR -DPLATFORM_NAME=Win32
libvk_win32_plugin_la_LIBADD = $(VULKAN_COMMONLIBS)

libvk_android_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) video_output/vulkan/surface.c \
				video_output/vulkan/platform_android.c
libvk_android_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_COMMONCFLAGS) \
			       -DVK_USE_PLATFORM_ANDROID_KHR -DPLATFORM_NAME=Android
libvk_android_plugin_la_LIBADD = $(VULKAN_COMMONLIBS)

if HAVE_VULKAN
vout_LTLIBRARIES += libvk_plugin.la

if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += libvk_win32_plugin.la
endif

if HAVE_ANDROID
vout_LTLIBRARIES += libvk_android_plugin.la
endif
endif


### Vulkan ###

VULKAN_COMMONSOURCES = video_output/vulkan/platform.c \
	video_output/vulkan/instance.h \
	video_output/vulkan/platform.h

libvk_win32_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) \
				video_output/vulkan/platform_win32.c
libvk_win32_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_CFLAGS) \
			       -DVK_USE_PLATFORM_WIN32_KHR -DPLATFORM_NAME=Win32

libvk_android_plugin_la_SOURCES = $(VULKAN_COMMONSOURCES) \
				video_output/vulkan/platform_android.c
libvk_android_plugin_la_CFLAGS = $(AM_CFLAGS) $(VULKAN_CFLAGS) \
			       -DVK_USE_PLATFORM_ANDROID_KHR -DPLATFORM_NAME=Android

if HAVE_VULKAN
if HAVE_WIN32_DESKTOP
vout_LTLIBRARIES += libvk_win32_plugin.la
endif

if HAVE_ANDROID
vout_LTLIBRARIES += libvk_android_plugin.la
endif
endif

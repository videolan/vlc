# SDL

SDL_VERSION := 3.4.10
SDL_URL := https://github.com/libsdl-org/SDL/releases/download/release-$(SDL_VERSION)/SDL3-$(SDL_VERSION).tar.gz

#PKGS += sdl
ifeq ($(call need_pkg,"sdl"),)
PKGS_FOUND += sdl
endif

$(TARBALLS)/SDL3-$(SDL_VERSION).tar.gz:
	$(call download_pkg,$(SDL_URL),sdl)

.sum-sdl: SDL3-$(SDL_VERSION).tar.gz

sdl: SDL3-$(SDL_VERSION).tar.gz .sum-sdl
	$(UNPACK)
	$(call pkg_static,"cmake/sdl3.pc.in")
	$(MOVE)

SDL_CONF := \
	-DSDL_AUDIO=OFF \
	-DSDL_VIDEO=ON \
	-DSDL_HAPTIC=OFF \
	-DSDL_JOYSTICK=OFF \
	-DSDL_THREADS=OFF \
	-DSDL_ASSEMBLY=OFF \
	-DSDL_X11=OFF \
	-DSDL_DIRECTX=OFF \
	-DSDL_TESTS=OFF \
	-DSDL_EXAMPLES=OFF


	# -Dsdl-dlopen=ON
	# -DSDL_events=ON \
	# -DSDL_cdrom=OFF \
	# -DSDL_timers=OFF \
	# -DSDL_file=OFF \
	# -DSDL_video-aalib=OFF \
	# -DSDL_video-dga=OFF \
	# -DSDL_video-fbcon=OFF \
	# -DSDL_video-directfb=OFF \
	# -DSDL_video-ggi=OFF \
	# -DSDL_video-svga=OFF \

.sdl: sdl toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SDL_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

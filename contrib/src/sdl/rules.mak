# SDL

SDL_VERSION := 1.2.15
SDL_URL := http://www.libsdl.org/release/SDL-$(SDL_VERSION).tar.gz

#PKGS += sdl
ifeq ($(call need_pkg,"sdl"),)
PKGS_FOUND += sdl
endif

$(TARBALLS)/SDL-$(SDL_VERSION).tar.gz:
	$(call download_pkg,$(SDL_URL),sdl)

.sum-sdl: SDL-$(SDL_VERSION).tar.gz

sdl: SDL-$(SDL_VERSION).tar.gz .sum-sdl
	$(UNPACK)
	$(APPLY) $(SRC)/sdl/direct_palette_ref.diff
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
	mv sdl/config.sub sdl/config.guess sdl/build-scripts

SDLCONF := $(HOSTCONF) \
	--disable-audio \
	--enable-video \
	--enable-events \
	--disable-joystick \
	--disable-cdrom \
	--disable-threads \
	--disable-timers \
	--disable-file \
	--disable-assembly \
	--disable-video-x11 \
	--disable-video-aalib \
	--disable-video-dga \
	--disable-video-fbcon \
	--disable-video-directfb \
	--disable-video-ggi \
	--disable-video-svga \
	--disable-directx \
	--disable-sdl-dlopen

.sdl: sdl
	cd $< && $(HOSTVARS) ./configure $(SDLCONF)
	cd $< && $(MAKE) install
	touch $@

# SDL

SDL_VERSION := 1.2.14
SDL_URL := http://www.libsdl.org/release/SDL-$(SDL_VERSION).tar.gz

PKGS += sdl
ifeq ($(call need_pkg,"sdl"),)
PKGS_FOUND += sdl
endif

$(TARBALLS)/SDL-$(SDL_VERSION).tar.gz:
	$(call download,$(SDL_URL))

.sum-sdl: SDL-$(SDL_VERSION).tar.gz

sdl: SDL-$(SDL_VERSION).tar.gz .sum-sdl
	$(UNPACK)
	$(MOVE)

SDLCONF := $(HOSTCONF) \
	--disable-video-x11 \
	--disable-video-aalib \
	--disable-video-dga \
	--disable-video-fbcon \
	--disable-video-directfb \
	--disable-video-ggi \
	--disable-video-svga \
	--disable-directx \
	--disable-joystick \
	--disable-cdrom \
	--disable-threads \
	--disable-sdl-dlopen

.sdl: sdl
	cd $< && $(HOSTVARS) ./configure $(SDLCONF)
	cd $< && $(MAKE) install
	touch $@

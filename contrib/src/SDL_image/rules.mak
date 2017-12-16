# SDL_image

SDL_IMAGE_VERSION := 1.2.12
SDL_IMAGE_URL := http://www.libsdl.org/projects/SDL_image/release/SDL_image-$(SDL_IMAGE_VERSION).tar.gz

PKGS += SDL_image
ifeq ($(call need_pkg,"SDL_image"),)
PKGS_FOUND += SDL_image
endif

$(TARBALLS)/SDL_image-$(SDL_IMAGE_VERSION).tar.gz:
	$(call download_pkg,$(SDL_IMAGE_URL),SDL_image)

.sum-SDL_image: SDL_image-$(SDL_IMAGE_VERSION).tar.gz

SDL_image: SDL_image-$(SDL_IMAGE_VERSION).tar.gz .sum-SDL_image
	$(UNPACK)
	$(APPLY) $(SRC)/SDL_image/SDL_image.patch
	$(APPLY) $(SRC)/SDL_image/pkg-config.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_SDL_image = jpeg $(DEPS_jpeg) tiff $(DEPS_tiff) \
	sdl $(DEPS_sdl)

.SDL_image: SDL_image
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-tif --disable-sdltest --disable-png
	cd $< && $(MAKE) install
	touch $@

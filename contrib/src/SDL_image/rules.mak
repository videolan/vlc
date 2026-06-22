# SDL_image

SDL_IMAGE_VERSION := 3.4.4
SDL_IMAGE_URL := https://github.com/libsdl-org/SDL_image/releases/download/release-$(SDL_IMAGE_VERSION)/SDL3_image-$(SDL_IMAGE_VERSION).tar.gz

# sdl_image module is disabled on macOS, and it's dependency sdl failed to build
ifndef HAVE_DARWIN_OS
PKGS += SDL_image
endif
ifeq ($(call need_pkg,"SDL_image"),)
PKGS_FOUND += SDL_image
endif

$(TARBALLS)/SDL3_image-$(SDL_IMAGE_VERSION).tar.gz:
	$(call download_pkg,$(SDL_IMAGE_URL),SDL_image)

.sum-SDL_image: SDL3_image-$(SDL_IMAGE_VERSION).tar.gz

SDL_image: SDL3_image-$(SDL_IMAGE_VERSION).tar.gz .sum-SDL_image
	$(UNPACK)
	$(call pkg_static,"cmake/sdl3-image.pc.in")
	$(MOVE)

DEPS_SDL_image = jpeg $(DEPS_jpeg) \
	sdl $(DEPS_sdl)

SDL_IMAGE_CONF := -DSDLIMAGE_TIF=OFF \
	-DSDLIMAGE_PNG=OFF \
	-DSDLIMAGE_TESTS=OFF \
	-DSDLIMAGE_SAMPLES=OFF

.SDL_image: SDL_image toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SDL_IMAGE_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

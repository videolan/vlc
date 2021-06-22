# decklink OSS version
DECKLINK_VERSION := 1977bf76bb5bde80d171373cf8d80dd01161679b
DECKLINK_GITURL := https://gitlab.com/robUx4/decklink-oss.git

# enable build on supported platforms
ifdef HAVE_LINUX
PKGS += decklink
endif
ifdef HAVE_MACOSX
PKGS += decklink
endif
ifdef HAVE_WIN32
PKGS += decklink
endif

$(TARBALLS)/decklink-$(DECKLINK_VERSION).tar.xz:
	$(call download_git,$(DECKLINK_GITURL),,$(DECKLINK_VERSION))

.sum-decklink: $(TARBALLS)/decklink-$(DECKLINK_VERSION).tar.xz
	$(call check_githash,$(DECKLINK_VERSION))
	touch $@

decklink: decklink-$(DECKLINK_VERSION).tar.xz .sum-decklink
	$(UNPACK)
	$(MOVE)

.decklink: decklink
	mkdir -p -- "$(PREFIX)/include/"
ifdef HAVE_LINUX
	cp -R $</Linux/include/ "$(PREFIX)/include/decklink/"
else ifdef HAVE_MACOSX
	cp -R $</Mac/include/ "$(PREFIX)/include/decklink/"
else ifdef HAVE_WIN32
	cp -R $</Win/include/ "$(PREFIX)/include/decklink/"
else
	$(error Decklink SDK contrib not implemented for your OS!)
endif
	touch $@

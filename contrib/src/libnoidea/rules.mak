# libnoidea

LIBNOIDEA_VERSION := b20c2d0a6f606db2c4649f368b9735a2599d6a2c
LIBNOIDEA_GITURL := https://code.videolan.org/videolan/libnoidea.git

PKGS += libnoidea
ifeq ($(call need_pkg,"libnoidea"),)
PKGS_FOUND += libnoidea
endif

LIBNOIDEA_CONF := -Dmicrodns=disabled

$(TARBALLS)/libnoidea-$(LIBNOIDEA_VERSION).tar.xz:
	$(call download_git,$(LIBNOIDEA_GITURL),,$(LIBNOIDEA_VERSION))

.sum-libnoidea: $(TARBALLS)/libnoidea-$(LIBNOIDEA_VERSION).tar.xz
	$(call check_githash,$(LIBNOIDEA_VERSION))
	touch $@

libnoidea: libnoidea-$(LIBNOIDEA_VERSION).tar.xz .sum-libnoidea
	$(UNPACK)
	$(MOVE)

.libnoidea: libnoidea crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(LIBNOIDEA_CONF)
	+$(MESONBUILD)
	touch $@

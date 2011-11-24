# bghudappkit

BGHUDAPPKIT_GITURL := https://github.com/binarygod/BGHUDAppKit.git

ifdef HAVE_MACOSX
PKGS += bghudappkit
endif

$(TARBALLS)/bghudappkit-git.tar.xz:
	$(call download_git,$(BGHUDAPPKIT_GITURL))


.sum-bghudappkit: bghudappkit-git.tar.xz
	$(warning $@ not implemented)
	touch $@


bghudappkit: bghudappkit-git.tar.xz .sum-bghudappkit
	$(UNPACK)
	$(MOVE)

.bghudappkit: bghudappkit
	cd $< && xcodebuild -arch $(ARCH)
	cd $< && cp -R -L build/Release/BGHUDAppKit.framework "$(PREFIX)"
	touch $@

# bghudappkit

BGHUDAPPKIT_GITURL := git://github.com/binarygod/BGHUDAppKit.git

ifdef HAVE_MACOSX
PKGS += bghudappkit
endif

$(TARBALLS)/bghudappkit-git.tar.xz:
	$(call download_git,$(BGHUDAPPKIT_GITURL),,79a560d)


.sum-bghudappkit: bghudappkit-git.tar.xz
	$(warning $@ not implemented)
	touch $@


bghudappkit: bghudappkit-git.tar.xz .sum-bghudappkit
	$(UNPACK)
	$(APPLY) $(SRC)/bghudappkit/bghudappkit-xcode4.patch
	$(MOVE)

.bghudappkit: bghudappkit
	cd $< && xcodebuild $(XCODE_FLAGS)
	install_name_tool -change @loader_path/../../../../../../../BGHUDAppKit.framework/Versions/A/BGHUDAppKit \
								@loader_path/../../../../Versions/A/BGHUDAppKit \
		$</build/Release/BGHUDAppKit.framework/Resources/BGHUDAppKitPlugin.ibplugin/Contents/MacOS/BGHUDAppKitPlugin
	cd $< && cp -R build/Release/BGHUDAppKit.framework "$(PREFIX)"
	touch $@

flatpak:
	flatpak --user --command="./extras/package/flatpak/prepare-sources.sh" --filesystem=host --share=network run org.kde.Sdk//5.15-25.08
	flatpak-builder --user --install-deps-from=flathub --disable-rofiles-fuse --disable-updates --force-clean --ccache --repo=repo build extras/package/flatpak/org.videolan.VLC.yaml

flatpak-clean:
	rm -rf build repo

flatpak:
	flatpak-builder --user --install-deps-from=flathub --disable-rofiles-fuse --disable-updates --force-clean --ccache --repo=repo build org.videolan.VLC.yaml

flatpak-clean:
	rm -rf build repo

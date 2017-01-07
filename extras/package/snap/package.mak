snap:
	snapcraft prime
	mkdir -p setup/gui
	sed -i -e 's,^Exec=/bin/vlc,Exec=vlc,' -e 's,^Icon=vlc,Icon=$${SNAP}/meta/gui/vlc.png,' prime/share/applications/vlc.desktop
	ln -sf ../../prime/share/applications/vlc.desktop setup/gui
	ln -sf ../../prime/share/icons/hicolor/256x256/apps/vlc.png setup/gui
	snapcraft snap

snap-clean:
	snapcraft clean
	rm -rf setup

snap:
	snapcraft prime
	mkdir -p snap/gui
	sed -i -e 's,^Exec=/bin/vlc,Exec=vlc,' -e 's,^Icon=vlc,Icon=$${SNAP}/meta/gui/vlc.png,' prime/share/applications/vlc.desktop
	ln -sf ../../prime/share/applications/vlc.desktop snap/gui
	ln -sf ../../prime/share/icons/hicolor/256x256/apps/vlc.png snap/gui
	snapcraft snap

snap-clean:
	snapcraft clean
	rm -rf snap

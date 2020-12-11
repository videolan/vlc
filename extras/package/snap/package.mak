snap:
	export SNAPCRAFT_BUILD_INFO=1
	snapcraft snap

snap-clean:
	snapcraft clean
	rm -rf snap

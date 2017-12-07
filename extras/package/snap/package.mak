snap:
	snapcraft prime
	snapcraft snap

snap-clean:
	snapcraft clean
	rm -rf snap

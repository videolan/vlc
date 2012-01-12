VLCKit.zip: VLCKit
	zip -r -y -9 $@ $<

VLCKit: build/Debug/VLCKit.framework
	rm -rf $@-tmp && mkdir -p $@-tmp
	cp -R $< $@-tmp
	cp ../../../COPYING $@-tmp
	mv $@-tmp $@ && touch $@

build/Debug/VLCKit.framework:
	xcodebuild -project VLCKit.xcodeproj -target "Build Everything"

clean:
	xcodebuild -project VLCKit.xcodeproj clean
	rm -fr VLCKit VLCKit.zip

.PHONY: clean

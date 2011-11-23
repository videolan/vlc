# sparkle

SPARKLE_VERSION := 1.5b6
SPARKLE_URL := http://sparkle.andymatuschak.org/files/Sparkle%20$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/sparkle-$(SPARKLE_VERSION).zip:
	$(call download,$(SPARKLE_URL))

.sum-sparkle: sparkle-$(SPARKLE_VERSION).zip

sparkle: sparkle-$(SPARKLE_VERSION).zip .sum-sparkle
	$(RM) -R $@ && mkdir -p $@ && cd $@ $(foreach f,$(filter %.zip,$^), && unzip ../$(f))
	touch $@

.sparkle: sparkle
	cd $</Extras/Source\ Code && $(MAKE) && exit 1 #FIXME
	touch $@

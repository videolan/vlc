# speex

SPEEX_VERSION := 1.2rc1
SPEEX_URL := http://downloads.us.xiph.org/releases/speex/speex-$(SPEEX_VERSION).tar.gz

PKGS += speex

$(TARBALLS)/speex-$(SPEEX_VERSION).tar.gz:
	$(DOWNLOAD) $(SPEEX_URL)

.sum-speex: speex-$(SPEEX_VERSION).tar.gz
	$(CHECK_SHA512)
	touch $@

speex: speex-$(SPEEX_VERSION).tar.gz .sum-speex
	$(UNPACK_GZ)
	mv $@-$(SPEEX_VERSION) $@
	touch $@

# TODO: fixed point and ASM opts

.speex: speex
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --without-ogg
	cd $< && $(MAKE) install
	touch $@

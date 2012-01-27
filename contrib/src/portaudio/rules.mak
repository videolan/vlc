# portaudio

PORTAUDIO_VERSION := 19_20110326
PORTAUDIO_URL := http://www.portaudio.com/archives/pa_stable_v$(PORTAUDIO_VERSION).tgz

ifdef HAVE_WIN32
PKGS += portaudio
endif

ifeq ($(call need_pkg,"portaudio"),)
PKGS_FOUND += portaudio
endif

$(TARBALLS)/portaudio-$(PORTAUDIO_VERSION).tar.gz:
	$(call download,$(PORTAUDIO_URL))

.sum-portaudio: portaudio-$(PORTAUDIO_VERSION).tar.gz

portaudio: portaudio-$(PORTAUDIO_VERSION).tar.gz .sum-portaudio
	$(UNPACK)
	patch -p0 < $(SRC)/portaudio/portaudio-cross.patch

.portaudio: portaudio
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@

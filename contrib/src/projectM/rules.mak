# PROJECTM
PROJECTM_VERSION := 2.0.1
PROJECTM_URL := $(SF)/projectm/$(PROJECTM_VERSION)/projectM-$(PROJECTM_VERSION)-Source.tar.gz

ifdef HAVE_WIN32
PKGS += projectM
endif
ifeq ($(call need_pkg,"libprojectM"),)
PKGS_FOUND += projectM
endif

$(TARBALLS)/projectM-$(PROJECTM_VERSION)-Source.tar.gz:
	$(call download,$(PROJECTM_URL))

.sum-projectM: projectM-$(PROJECTM_VERSION)-Source.tar.gz

projectM: projectM-$(PROJECTM_VERSION)-Source.tar.gz .sum-projectM
	$(UNPACK)
ifdef HAVE_WIN64
	$(APPLY) $(SRC)/projectM/win64.patch
endif
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/projectM/win32.patch
endif
	$(MOVE)

DEPS_projectM = glew $(DEPS_glew)

.projectM: projectM
	-cd $< && rm CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE) \
		-DINCLUDE-PROJECTM-LIBVISUAL:BOOL=OFF \
		-DDISABLE_NATIVE_PRESETS:BOOL=ON \
		-DUSE_FTGL:BOOL=OFF \
		-DINCLUDE-PROJECTM-PULSEAUDIO:BOOL=OFF \
		-DINCLUDE-PROJECTM-QT:BOOL=OFF \
		-DBUILD_PROJECTM_STATIC:BOOL=ON .
	cd $< && $(MAKE) install
	-cd $<; cp Renderer/libRenderer.a MilkdropPresetFactory/libMilkdropPresetFactory.a $(PREFIX)/lib
	touch $@

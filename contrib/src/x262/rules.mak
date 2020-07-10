# x262

X262_GITURL := git://git.videolan.org/x262.git
X262_HASH := bb887aa4c0a4da955524aa220b62998c3b50504e

# ifdef BUILD_ENCODERS
# ifdef GPL
# PKGS += x262
# endif
# endif

#ifeq ($(call need_pkg,"x262"),)
#PKGS_FOUND += x262
#endif

$(TARBALLS)/x262-git.tar.xz:
	$(call download_git,$(X262_GITURL),,$(X262_HASH))

.sum-x262: $(TARBALLS)/x262-git.tar.xz
	$(call check_githash,$(X262_HASH))
	touch $@

x262: $(TARBALLS)/x262-git.tar.xz .sum-x262
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.x262: x262
	$(REQUIRE_GPL)
	cd $< && sed -i -e 's/x264/x262/g' configure
	cd $< && sed -i -e 's/x264_config/x262_config/g' *.h Makefile *.c
	cd $< && $(HOSTVARS) ./configure $(X264CONF)
	cd $< && sed -i -e 's/x264.pc/x262.pc/g' Makefile
	cd $< && sed -i -e 's/x264.h/x262.h/g' Makefile
	cd $< && $(MAKE)
	cd $< && cp x264.h x262.h
	cd $< && $(MAKE) install
	touch $@

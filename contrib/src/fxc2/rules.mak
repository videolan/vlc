FXC2_HASH := 63ad74b7faa7033f2c1be9cc1cd0225241a1a9a5
FXC2_VERSION := git-$(FXC2_HASH)
FXC2_GITURL := https://github.com/mozilla/fxc2.git

ifeq ($(call need_pkg,"fxc2"),)
PKGS_FOUND += fxc2
endif

$(TARBALLS)/fxc2-$(FXC2_VERSION).tar.xz:
	$(call download_git,$(FXC2_GITURL),,$(FXC2_HASH))

.sum-fxc2: fxc2-$(FXC2_VERSION).tar.xz
	$(call check_githash,$(FXC2_HASH))
	touch $@

fxc2: fxc2-$(FXC2_VERSION).tar.xz .sum-fxc2
	rm -rf $@-$(FXC2_VERSION) $@
	mkdir -p $@-$(FXC2_VERSION)
	tar xvf "$<" --strip-components=1 -C $@-$(FXC2_VERSION)
	$(APPLY) $(SRC)/fxc2/0001-make-Vn-argument-as-optional-and-provide-default-var.patch
	$(APPLY) $(SRC)/fxc2/0002-accept-windows-style-flags-and-splitted-argument-val.patch
	$(APPLY) $(SRC)/fxc2/0003-Use-meson-as-a-build-system.patch
	$(APPLY) $(SRC)/fxc2/0004-Revert-Fix-narrowing-conversion-from-int-to-BYTE.patch
	$(MOVE)

.fxc2: fxc2 crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) build
	cd $< && cd build && ninja install
	touch $@

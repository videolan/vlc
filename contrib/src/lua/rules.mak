# Lua 5.1

LUA_VERSION := 5.1.4
LUA_URL := http://www.lua.org/ftp/lua-$(LUA_VERSION).tar.gz

# Reverse priority order
LUA_TARGET := generic
ifdef HAVE_BSD
LUA_TARGET := bsd
endif
ifdef HAVE_LINUX
LUA_TARGET := linux
endif
ifdef HAVE_MACOSX
LUA_TARGET := macosx
endif
ifdef HAVE_IOS
LUA_TARGET := ios
endif
ifdef HAVE_WIN32
LUA_TARGET := mingw
endif
ifdef HAVE_SOLARIS
LUA_TARGET := solaris
endif

# Feel free to add autodetection if you need to...
PKGS += lua luac
PKGS_ALL += luac
ifeq ($(call need_pkg,"lua5.2"),)
PKGS_FOUND += lua luac
endif
ifeq ($(call need_pkg,"lua5.1"),)
PKGS_FOUND += lua luac
endif

$(TARBALLS)/lua-$(LUA_VERSION).tar.gz:
	$(call download_pkg,$(LUA_URL),lua)

.sum-lua: lua-$(LUA_VERSION).tar.gz

lua: lua-$(LUA_VERSION).tar.gz .sum-lua
	$(UNPACK)
	$(APPLY) $(SRC)/lua/lua-noreadline.patch
	$(APPLY) $(SRC)/lua/no-dylibs.patch
	$(APPLY) $(SRC)/lua/luac-32bits.patch
	$(APPLY) $(SRC)/lua/no-localeconv.patch
	$(APPLY) $(SRC)/lua/lua-ios-support.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/lua/lua-winrt.patch
endif
ifdef HAVE_DARWIN_OS
	(cd $(UNPACK_DIR) && \
	sed -e 's%gcc%$(CC)%' \
		-e 's%LDFLAGS=%LDFLAGS=$(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)%' \
		-i.orig src/Makefile)
endif
ifdef HAVE_SOLARIS
	(cd $(UNPACK_DIR) && \
	sed -e 's%LIBS="-ldl"$$%LIBS="-ldl" MYLDFLAGS="$(EXTRA_LDFLAGS)"%' \
		-i.orig src/Makefile)
endif
ifdef HAVE_WIN32
	cd $(UNPACK_DIR) && sed -i.orig -e 's/lua luac/lua.exe luac.exe/' Makefile
endif
	cd $(UNPACK_DIR)/src && sed -i.orig \
		-e 's%CC=%#CC=%' \
		-e 's%= *strip%=$(STRIP)%' \
		-e 's%= *ranlib%= $(RANLIB)%' \
		Makefile
	$(MOVE)

.lua: lua
	cd $< && $(HOSTVARS_PIC) $(MAKE) $(LUA_TARGET)
ifdef HAVE_WIN32
	cd $</src && $(HOSTVARS) $(MAKE) liblua.a
endif
	cd $< && $(HOSTVARS) $(MAKE) install INSTALL_TOP="$(PREFIX)"
ifdef HAVE_WIN32
	cd $< && $(RANLIB) "$(PREFIX)/lib/liblua.a"
	mkdir -p -- "$(PREFIX)/lib/pkgconfig"
	cp $</etc/lua.pc "$(PREFIX)/lib/pkgconfig/"
endif
	touch $@

.sum-luac: .sum-lua
	touch $@

ifdef HAVE_WIN32
ifndef HAVE_CROSS_COMPILE
LUACVARS=CPPFLAGS="-DLUA_DL_DLL"
endif
endif

luac: lua-$(LUA_VERSION).tar.gz .sum-luac
	# DO NOT use the same intermediate directory as the lua target
	rm -Rf -- $@-$(LUA_VERSION) $@
	mkdir -- $@-$(LUA_VERSION)
	tar -x -v -z -C $@-$(LUA_VERSION) --strip-components=1 -f $<
	(cd luac-$(LUA_VERSION) && patch -p1) < $(SRC)/lua/luac-32bits.patch
	mv luac-$(LUA_VERSION) luac

.luac: luac
	cd $< && $(LUACVARS) $(MAKE) generic
	mkdir -p -- $(BUILDBINDIR)
	install -m 0755 -s -- $</src/luac $(BUILDBINDIR)/$(HOST)-luac
	touch $@

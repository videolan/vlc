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
	$(APPLY) $(SRC)/lua/implib.patch
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
		-e 's%AR= *ar%AR= $(AR)%' \
		Makefile
	$(MOVE)

.lua: lua
	cd $< && $(HOSTVARS_PIC) $(MAKE) $(LUA_TARGET)
ifdef HAVE_WIN32
	cd $< && $(HOSTVARS) $(MAKE) -C src liblua.a
endif
	cd $< && $(HOSTVARS) $(MAKE) install INSTALL_TOP="$(PREFIX)"
ifdef HAVE_WIN32
	cd $< && $(RANLIB) "$(PREFIX)/lib/liblua.a"
endif
	mkdir -p -- "$(PREFIX)/lib/pkgconfig"
	sed "s#^prefix=.*#prefix=$(PREFIX)#" $</etc/lua.pc > "$(PREFIX)/lib/pkgconfig/lua.pc"
	touch $@

.sum-luac: .sum-lua
	touch $@

ifdef HAVE_WIN32
ifndef HAVE_CROSS_COMPILE
LUACVARS=CPPFLAGS="-DLUA_DL_DLL"
endif
endif

ifdef HAVE_CROSS_COMPILE
# Remove the cross-compiler environment for the native compiler
LUACVARS+=CFLAGS="" CPPFLAGS="" LDFLAGS=""
endif

# DO NOT use the same intermediate directory as the lua target
luac: UNPACK_DIR=luac-$(LUA_VERSION)
luac: lua-$(LUA_VERSION).tar.gz .sum-luac
	$(RM) -Rf $@ $(UNPACK_DIR) && mkdir -p $(UNPACK_DIR)
	tar xvzfo $< -C $(UNPACK_DIR) --strip-components=1
	$(APPLY) $(SRC)/lua/luac-32bits.patch
	$(MOVE)

.luac: luac
	cd $< && $(LUACVARS) $(MAKE) generic
	mkdir -p -- $(BUILDBINDIR)
	install -m 0755 -s -- $</src/luac $(BUILDBINDIR)/$(HOST)-luac
	touch $@

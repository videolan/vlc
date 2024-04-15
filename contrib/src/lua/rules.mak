# Lua 5.4

LUA_SHORTVERSION := 5.4
LUA_VERSION := $(LUA_SHORTVERSION).4
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
PKGS_TOOLS += luac
PKGS_ALL += luac
PKGS.tools += luac
PKGS.tools.luac.path = $(BUILDBINDIR)/$(HOST)-luac

LUAC_IF_NOT_CROSS =
ifndef HAVE_CROSS_COMPILE
LUAC_IF_NOT_CROSS += luac
endif

ifeq ($(call need_pkg,"lua >= 5.1"),)
PKGS_FOUND += lua $(LUAC_IF_NOT_CROSS)
else
ifeq ($(call need_pkg,"lua5.2"),)
PKGS_FOUND += lua $(LUAC_IF_NOT_CROSS)
else
ifeq ($(call need_pkg,"lua5.1"),)
PKGS_FOUND += lua $(LUAC_IF_NOT_CROSS)
endif
endif
endif

ifeq ($(shell $(HOST)-luac -v 2>/dev/null | head -1 | sed  -E 's/Lua ([0-9]+).([0-9]+).*/\1.\2/'),$(LUA_SHORTVERSION))
PKGS_FOUND += luac
endif
ifeq ($(shell $(HOST)-luac -v 2>/dev/null | head -1 | sed  -E 's/Lua ([0-9]+).([0-9]+).*/\1.\2/'),5.2)
PKGS_FOUND += luac
endif


LUA_MAKEFLAGS := \
	$(HOSTTOOLS) \
	AR="$(AR) rcu" \
	MYCFLAGS="$(CFLAGS) $(PIC)" \
	MYLDFLAGS="$(LDFLAGS) $(PIC)" \
	CPPFLAGS="$(CPPFLAGS) $(PIC)"

# Make sure we do not use the cross-compiler when building
# the native luac for the host.
LUA_BUILD_MAKEFLAGS := \
	$(BUILDTOOLS) \
	AR="$(BUILDAR) rcu" \
	MYCFLAGS="$(BUILDCFLAGS)" \
	MYLDFLAGS="$(BUILDLDFLAGS)" \
	CPPFLAGS="$(BUILDCPPFLAGS)"

ifdef HAVE_WIN32
	LUA_MAKEFLAGS += EXE_EXT=.exe
endif

$(TARBALLS)/lua-$(LUA_VERSION).tar.gz:
	$(call download_pkg,$(LUA_URL),lua)

.sum-lua: lua-$(LUA_VERSION).tar.gz

lua: lua-$(LUA_VERSION).tar.gz .sum-lua
	$(UNPACK)
	$(APPLY) $(SRC)/lua/Disable-dynamic-library-loading-support.patch
	$(APPLY) $(SRC)/lua/Avoid-usage-of-localeconv.patch
	$(APPLY) $(SRC)/lua/Create-an-import-library-needed-for-lld.patch
	$(APPLY) $(SRC)/lua/Disable-system-and-popen-for-windows-store-builds.patch
	$(APPLY) $(SRC)/lua/Add-version-to-library-name.patch
	$(APPLY) $(SRC)/lua/Add-a-Makefile-variable-to-override-the-strip-tool.patch
	$(APPLY) $(SRC)/lua/Create-and-install-a-.pc-file.patch
	$(APPLY) $(SRC)/lua/Add-EXE_EXT-to-allow-specifying-binary-extension.patch
	$(APPLY) $(SRC)/lua/Do-not-use-log2f-with-too-old-Android-API-level.patch
	$(APPLY) $(SRC)/lua/Do-not-use-large-file-offsets-with-too-old-Android-A.patch
	$(APPLY) $(SRC)/lua/Enforce-always-using-64bit-integers-floats.patch
	$(MOVE)

.lua: lua
	$(MAKE) -C $< $(LUA_TARGET) $(LUA_MAKEFLAGS)
ifdef HAVE_WIN32
	$(MAKE) -C $< -C src liblua$(LUA_SHORTVERSION).a $(LUA_MAKEFLAGS)
endif

	$(MAKE) -C $< install \
		INSTALL_INC="$(PREFIX)/include/lua$(LUA_SHORTVERSION)" \
		INSTALL_TOP="$(PREFIX)" \
		$(LUA_MAKEFLAGS)
ifdef HAVE_WIN32
	$(RANLIB) "$(PREFIX)/lib/liblua$(LUA_SHORTVERSION).a"
endif

	# Configure scripts might search for lua >= 5.4 or lua5.4 so expose both
	cp "$(PREFIX)/lib/pkgconfig/lua.pc" "$(PREFIX)/lib/pkgconfig/lua$(LUA_SHORTVERSION).pc"
	touch $@

# Luac (lua bytecode compiler)
#
# If lua from contribs is used, luac has to be used from contribs
# as well to match the custom patched lua we use in contribs.

.sum-luac: .sum-lua
	touch $@

# DO NOT use the same intermediate directory as the lua target
luac: UNPACK_DIR=luac-$(LUA_VERSION)
luac: lua-$(LUA_VERSION).tar.gz .sum-luac
	$(RM) -Rf $@ $(UNPACK_DIR) && mkdir -p $(UNPACK_DIR)
	tar $(TAR_VERBOSE)xzfo $< -C $(UNPACK_DIR) --strip-components=1
	$(APPLY) $(SRC)/lua/Enforce-always-using-64bit-integers-floats.patch
	$(MOVE)

.luac: luac
	$(MAKE) -C $< $(LUA_BUILD_MAKEFLAGS) generic
	mkdir -p -- $(BUILDBINDIR)
	install -m 0755 -s -- $</src/luac $(BUILDBINDIR)/$(HOST)-luac
	touch $@

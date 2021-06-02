keystoredir = $(pluginsdir)/keystore
keystore_PLUGINS =
keystore_LTLIBRARIES =
keystore_RPATH =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(keystore_PLUGINS)
else
keystore_LTLIBRARIES += $(keystore_PLUGINS)
keystore_RPATH += -rpath '$(keystoredir)'
endif

libmemory_keystore_plugin_la_SOURCES = keystore/memory.c \
	keystore/list_util.c keystore/list_util.h
keystore_PLUGINS += libmemory_keystore_plugin.la

libfile_keystore_plugin_la_SOURCES = keystore/file.c \
	keystore/file_crypt.h \
	keystore/list_util.c keystore/list_util.h
if HAVE_ANDROID
libfile_keystore_plugin_la_SOURCES += keystore/file_crypt_android.c
libfile_keystore_plugin_la_LIBADD = libandroid_utils.la
endif
if HAVE_WIN32
libfile_keystore_plugin_la_SOURCES += keystore/file_crypt_win32.c
libfile_keystore_plugin_la_LIBADD = -lcrypt32
endif
keystore_PLUGINS += libfile_keystore_plugin.la

libsecret_plugin_la_SOURCES = keystore/secret.c
libsecret_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SECRET_CFLAGS)
libsecret_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(keystore_RPATH)
libsecret_plugin_la_LIBADD = $(SECRET_LIBS)

libkwallet_plugin_la_SOURCES = keystore/kwallet.c
libkwallet_plugin_la_CFLAGS = $(AM_CFLAGS) $(DBUS_CFLAGS)
libkwallet_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(keystore_RPATH)
libkwallet_plugin_la_LIBADD = $(DBUS_LIBS) $(LIBM)

libkeychain_plugin_la_SOURCES = keystore/keychain.m keystore/list_util.c keystore/list_util.h
libkeychain_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc
libkeychain_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(keystore_RPATH) -Wl,-framework,Foundation -Wl,-framework,Security

if HAVE_DARWIN
keystore_PLUGINS += libkeychain_plugin.la
endif

keystore_PLUGINS += \
	$(LTLIBsecret) \
	$(LTLIBkwallet)

EXTRA_LTLIBRARIES += \
	libsecret_plugin.la \
	libkwallet_plugin.la

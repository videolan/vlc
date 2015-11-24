keystoredir = $(pluginsdir)/keystore

libplaintext_keystore_plugin_la_SOURCES = keystore/plaintext.c
keystore_LTLIBRARIES = libplaintext_keystore_plugin.la

libsecret_plugin_la_SOURCES = keystore/secret.c
libsecret_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SECRET_CFLAGS)
libsecret_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(keystoredir)'
libsecret_plugin_la_LIBADD = $(SECRET_LIBS)

keystore_LTLIBRARIES += \
	$(LTLIBsecret)

EXTRA_LTLIBRARIES += \
	libsecret_plugin.la

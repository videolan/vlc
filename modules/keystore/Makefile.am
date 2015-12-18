keystoredir = $(pluginsdir)/keystore

libplaintext_keystore_plugin_la_SOURCES = keystore/plaintext.c
keystore_LTLIBRARIES = libplaintext_keystore_plugin.la

libsecret_plugin_la_SOURCES = keystore/secret.c
libsecret_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SECRET_CFLAGS)
libsecret_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(keystoredir)'
libsecret_plugin_la_LIBADD = $(SECRET_LIBS)


# Meta-object compilation

SUFFIXES += .hpp .moc.cpp
moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose__$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC   " $@;
moc_verbose__0 = $(moc_verbose_0)

.hpp.moc.cpp:
	$(moc_verbose)$(MOC) $(MOC_CPPFLAGS) -o $@ $<

libkwallet_plugin_la_SOURCES = keystore/kwallet.cpp keystore/kwallet.moc.cpp
libkwallet_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libkwallet_plugin_la_CXXFLAGS = $(KWALLET_CXXFLAGS)
libkwallet_plugin_la_LIBADD = $(KWALLET_LIBS)
if HAVE_KWALLET
keystore_LTLIBRARIES += libkwallet_plugin.la
endif

keystore_LTLIBRARIES += \
	$(LTLIBsecret)

EXTRA_LTLIBRARIES += \
	libsecret_plugin.la

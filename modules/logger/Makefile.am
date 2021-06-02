loggerdir = $(pluginsdir)/logger
logger_PLUGINS =
logger_LTLIBRARIES =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(logger_PLUGINS)
else
logger_LTLIBRARIES += $(logger_PLUGINS)
endif

libconsole_logger_plugin_la_SOURCES = logger/console.c
libfile_logger_plugin_la_SOURCES = logger/file.c
logger_PLUGINS += libconsole_logger_plugin.la libfile_logger_plugin.la

libsyslog_plugin_la_SOURCES = logger/syslog.c
if HAVE_SYSLOG
logger_PLUGINS += libsyslog_plugin.la
endif

libsd_journal_plugin_la_SOURCES = logger/journal.c
libsd_journal_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SYSTEMD_CFLAGS)
libsd_journal_plugin_la_LIBADD = $(SYSTEMD_LIBS)
if HAVE_SYSTEMD
logger_PLUGINS += libsd_journal_plugin.la
endif

libandroid_logger_plugin_la_SOURCES = logger/android.c
libandroid_logger_plugin_la_CFLAGS = $(AM_CFLAGS)
libandroid_logger_plugin_la_LIBADD = -llog
if HAVE_ANDROID
logger_PLUGINS += libandroid_logger_plugin.la
endif

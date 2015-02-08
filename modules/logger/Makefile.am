loggerdir = $(pluginsdir)/logger

libconsole_logger_plugin_la_SOURCES = logger/console.c
logger_LTLIBRARIES = libconsole_logger_plugin.la

libsyslog_plugin_la_SOURCES = logger/syslog.c
if HAVE_SYSLOG
logger_LTLIBRARIES += libsyslog_plugin.la
endif

libsd_journal_plugin_la_SOURCES = logger/journal.c
libsd_journal_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SYSTEMD_CFLAGS)
libsd_journal_plugin_la_LIBADD = $(SYSTEMD_LIBS)
if HAVE_SYSTEMD
logger_LTLIBRARIES += libsd_journal_plugin.la
endif

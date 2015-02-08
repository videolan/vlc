loggerdir = $(pluginsdir)/logger

libconsole_logger_plugin_la_SOURCES = logger/console.c
logger_LTLIBRARIES = libconsole_logger_plugin.la

libsyslog_plugin_la_SOURCES = logger/syslog.c
if HAVE_SYSLOG
logger_LTLIBRARIES += libsyslog_plugin.la
endif

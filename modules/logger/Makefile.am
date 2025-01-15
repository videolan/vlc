loggerdir = $(pluginsdir)/logger

libconsole_logger_plugin_la_SOURCES = logger/console.c
libfile_logger_plugin_la_SOURCES = logger/file.c
logger_LTLIBRARIES = libconsole_logger_plugin.la libfile_logger_plugin.la

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

libandroid_logger_plugin_la_SOURCES = logger/android.c
libandroid_logger_plugin_la_LIBADD = -llog
if HAVE_ANDROID
logger_LTLIBRARIES += libandroid_logger_plugin.la
endif

libjson_tracer_plugin_la_SOURCES = logger/json.c
logger_LTLIBRARIES += libjson_tracer_plugin.la

libemscripten_logger_plugin_la_SOURCES = logger/emscripten.c

if HAVE_EMSCRIPTEN
logger_LTLIBRARIES += libemscripten_logger_plugin.la
endif

AM_V_LTCARGO = $(AM_V_LTCARGO_$(V))
AM_V_LTCARGO_ = $(AM_V_LTCARGO_$(AM_DEFAULT_VERBOSITY))
AM_V_LTCARGO__0 = $(AM_V_LTCARGO_0)
AM_V_LTCARGO_0 = @echo "  CARGO  $(@)";

libtelegraf_rs.la: $(libtelegraf_rs_plugin_la_SOURCES)
	$(AM_V_LTCARGO)$(LIBTOOL_CARGO) $(abs_srcdir)/logger/telegraf-rs $@
CLEANFILES += libtelegraf_rs.la

libtelegraf_rs_plugin_la_SOURCES = \
	logger/telegraf-rs/Cargo.toml \
	logger/telegraf-rs/src/lib.rs
libtelegraf_rs_plugin_la_LIBADD = libtelegraf_rs.la

if HAVE_RUST
logger_LTLIBRARIES += libtelegraf_rs_plugin.la
endif

# modulelist.mk - Generate vlc_modules_list from LTLIBRARIES and SUBDIRS
# Paths are relative to top_builddir using automake's $(subdir)


# Remove the non-plugins, it needs intermediate steps because
# we can only replace and not match inverse. Instead we:
#  1/ replace plugins .la targets by plugins names
#  2/ remove every word which is a .la target (ie. non-plugins)
#  3/ rename every plugin name back into the target name
VLC_PLUGINS_RENAMED = $(LTLIBRARIES:lib%_plugin.la=%_plugin)
VLC_PLUGINS = $(VLC_PLUGINS_RENAMED:%.la=)

# Generate a list containing the name and path of every plugin being built
# Format: plugin_name: relative/path/to/plugin$(LIBEXT)
# This allows plugins from SUBDIRS (like Qt) to specify their actual location
# The recipee concatenates records from local plugins and those from SUBDIRS
vlc_modules_list: Makefile
	$(AM_V_GEN)rm -f $@.tmp; \
	for plugin in $(VLC_PLUGINS); do \
		printf "$${plugin}: $(subdir)/.libs/lib$${plugin}$(LIBEXT)\n" >> $@.tmp; \
	done; \
	for subdir in $(SUBDIRS); do \
		[ "$$subdir" = "." ] && continue; \
		[ -f "$$subdir/vlc_modules_list" ] && \
			cat "$$subdir/vlc_modules_list" >> $@.tmp; \
	done; \
	if [ -s "$@.tmp" ]; then mv $@.tmp $@; else rm -f $@.tmp && touch $@; fi

BUILT_SOURCES += vlc_modules_list
CLEANFILES += vlc_modules_list

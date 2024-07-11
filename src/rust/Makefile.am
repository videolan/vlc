TEST_EXTENSIONS += .cargo
CARGO_LOG_DRIVER = env top_builddir="${abs_top_builddir}" \
                   $(abs_top_srcdir)/buildsystem/cargo-test.py \
                   --working-directory="${abs_top_srcdir}/src/rust/"

vlcrs-messages.cargo:
vlcrs-plugin.cargo:
vlcrs-utils.cargo:
	cd $(top_srcdir)/src/rust/$(@:.cargo=) && env \
		top_builddir="${abs_top_builddir}" \
		cargo build

if HAVE_RUST
TESTS += \
	vlcrs-messages.cargo \
	vlcrs-plugin.cargo \
	vlcrs-utils.cargo
endif

TEST_EXTENSIONS += .cargo
CARGO_LOG_DRIVER = env top_builddir="${abs_top_builddir}" \
                   $(abs_top_srcdir)/buildsystem/cargo-test.py \
                   --working-directory="${abs_top_srcdir}/src/rust/"

vlcrs-core.cargo:
vlcrs-macros.cargo:
vlcrs-messages.cargo:
vlcrs-utils.cargo:
	cd $(top_srcdir)/src/rust/$(@:.cargo=) && env top_builddir="${abs_top_builddir}" \
         $(CARGO) build --target=$(RUST_TARGET)

if HAVE_RUST
TESTS += \
	vlcrs-core.cargo \
	vlcrs-macros.cargo \
	vlcrs-messages.cargo \
	vlcrs-utils.cargo
endif

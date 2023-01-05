# Generate external meson machine file

# This cross or native file is meant to be used to easiy
# use the contribs with VLCs meson build system by using
# either the --cross-file or --native-file option
# respectively.

PKGS += meson-machinefile

ifdef HAVE_CROSS_COMPILE
CROSS_OR_NATIVE := cross
else
CROSS_OR_NATIVE := native
endif

meson-machinefile/contrib.ini: $(SRC)/gen-meson-machinefile.py
	mkdir -p meson-machinefile
	PREFIX="$(PREFIX)" \
	$(SRC)/gen-meson-machinefile.py --type external-$(CROSS_OR_NATIVE) $@

meson-machinefile: meson-machinefile/contrib.ini

# Dummy target, there is nothing to check
# as we download nothing.
.sum-meson-machinefile:
	touch $@

.meson-machinefile: meson-machinefile
	install -d "$(PREFIX)/share/meson/$(CROSS_OR_NATIVE)"
	install $</contrib.ini "$(PREFIX)/share/meson/$(CROSS_OR_NATIVE)"
	touch $@

JINJA_VERSION := 3.1.2
JINJA_URL := $(GITHUB)/pallets/jinja/archive/refs/tags/$(JINJA_VERSION).tar.gz

DEPS_jinja = markupsafe $(DEPS_markupsafe)

$(TARBALLS)/jinja-$(JINJA_VERSION).tar.gz:
	$(call download_pkg,$(JINJA_URL),jinja)

.sum-jinja: jinja-$(JINJA_VERSION).tar.gz

jinja: jinja-$(JINJA_VERSION).tar.gz .sum-jinja
	$(UNPACK)
	$(MOVE)

.jinja: jinja .python-venv
	$(PYTHON_INSTALL)
	touch $@

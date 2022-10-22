GLAD_VERSION := 2.0.1
GLAD_URL := $(GITHUB)/Dav1dde/glad/archive/refs/tags/v$(GLAD_VERSION).tar.gz

DEPS_glad = jinja $(DEPS_jinja)

$(TARBALLS)/glad-$(GLAD_VERSION).tar.gz:
	$(call download_pkg,$(GLAD_URL),glad)

.sum-glad: glad-$(GLAD_VERSION).tar.gz

glad: glad-$(GLAD_VERSION).tar.gz .sum-glad
	$(UNPACK)
	$(MOVE)

.glad: glad .python-venv
	$(PYTHON_INSTALL)
	touch $@

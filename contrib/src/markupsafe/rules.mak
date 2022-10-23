MARKUPSAFE_VERSION := 2.1.1
MARKUPSAFE_URL := $(GITHUB)/pallets/markupsafe/archive/refs/tags/$(MARKUPSAFE_VERSION).tar.gz

$(TARBALLS)/markupsafe-$(MARKUPSAFE_VERSION).tar.gz:
	$(call download_pkg,$(MARKUPSAFE_URL),markupsafe)

.sum-markupsafe: markupsafe-$(MARKUPSAFE_VERSION).tar.gz

markupsafe: markupsafe-$(MARKUPSAFE_VERSION).tar.gz .sum-markupsafe
	$(UNPACK)
	$(MOVE)

.markupsafe: markupsafe .python-venv
	$(PYTHON_INSTALL)
	touch $@

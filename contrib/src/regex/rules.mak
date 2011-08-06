# REGEX
REGEX_VERSION := 0.12
REGEX_URL := http://ftp.gnu.org/old-gnu/regex/regex-$(REGEX_VERSION).tar.gz

PKGS_FOUND += regex

$(TARBALLS)/regex-$(REGEX_VERSION).tar.gz:
	$(call download,$(REGEX_URL))

.sum-regex: regex-$(REGEX_VERSION).tar.gz

regex: regex-$(REGEX_VERSION).tar.gz .sum-regex
	$(UNPACK)
	$(MOVE)

.regex: regex
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && make subirs=
	cd $< && $(AR) rcvu libregex.a regex.o && $(RANLIB) libregex.a && cp -v regex.h $(PREFIX)/include && cp -v libregex.a $(PREFIX)/lib
	touch $@

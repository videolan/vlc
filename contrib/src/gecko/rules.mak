# gecko (mozilla plugin headers)

PKGS += gecko

NPAPI_HEADERS_SVN_URL := http://npapi-sdk.googlecode.com/svn/trunk/headers
NPAPI_HEADERS_SVN_REV := HEAD # revision number, or just HEAD for the latest

$(TARBALLS)/gecko-svn.tar.xz:
	rm -Rf gecko-svn
	$(SVN) export $(NPAPI_HEADERS_SVN_URL) gecko-svn -r $(NPAPI_HEADERS_SVN_REV)
	tar cvJ gecko-svn > $@

.sum-gecko: gecko-svn.tar.xz
	$(warning Integrity check skipped.)
	touch $@

gecko: gecko-svn.tar.xz .sum-gecko
	$(UNPACK)
	$(MOVE)

.gecko: gecko
	cp $</*.h "$(PREFIX)/include"
	touch $@

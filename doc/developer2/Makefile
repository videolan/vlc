XSLTPROC=xsltproc
DOC := vlc-dev-doc

XML_DECL1 = /usr/lib/sgml/declaration/xml.decl
XML_DECL2 = /usr/share/sgml/declaration/xml.decl
XML_DECL = `test -f $(XML_DECL1) && echo $(XML_DECL1) || echo $(XML_DECL2)`

XSL= videolan-doc.xsl

all: en/$(DOC).html

init:
	mkdir -p en
	cp screen.css en

en/$(DOC).html : init
	xsltproc $(XSL)	$(DOC).xml > en/$(DOC).html
	mv *.html en

clean:
	rm -rf en

# Extract from the Debian SGML/XML HOWTO by Stéphane Bortzmeyer

MAX_TEX_RECURSION=4

# For Debian :
XML_DECL=/usr/lib/sgml/declaration/xml.decl
HTML_SS=/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl
PRINT_SS=/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/print/docbook.dsl

# For RedHat :
#XML_DECL=/usr/lib/sgml/stylesheets/nwalsh-modular/dtds/decls/xml.dcl
#HTML_SS=/usr/lib/sgml/stylesheets/nwalsh-modular/html/docbook.dsl
#PRINT_SS=/usr/lib/sgml/stylesheets/nwalsh-modular/print/docbook.dsl

all: manual

JADE=jade

manual: manual.txt manual.ps manual.html

%.tex: %.xml
	$(JADE) -t tex -V %section-autolabel% -d $(PRINT_SS) $(XML_DECL) $<
	perl -i.bak -pe 's/\000//g' $@ && rm $*.tex.bak
# No it's not a joke

%.html: %.xml
	$(JADE) -t sgml -V %section-autolabel% -V nochunks \
         -d $(HTML_SS) $(XML_DECL) $< > $@

%.dvi: %.tex
	jadetex $<
	jadetex $<
	jadetex $<

%.ps: %.dvi
	dvips -f $< > $@

%.txt: %.xml
	$(JADE) -t sgml -V nochunks -d $(HTML_SS) $(XML_DECL) $< > dump.html
	lynx -force_html -dump dump.html > $@
	-rm -f dump.html

clean:
	rm -f manual.txt
	rm -f *.html *.aux *.log *.dvi *.ps *.tex
	rm -f *.bck *~ .\#* \#*


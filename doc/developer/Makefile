# Extract from the Debian SGML/XML HOWTO by Stéphane Bortzmeyer

MAX_TEX_RECURSION=4

# For Debian :
#XML_DECL=/usr/lib/sgml/declaration/xml.decl
#HTML_SS=/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl
#PRINT_SS=/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/print/docbook.dsl

# For RedHat :
#XML_DECL=/usr/lib/sgml/stylesheets/nwalsh-modular/dtds/decls/xml.dcl
#HTML_SS=/usr/lib/sgml/stylesheets/nwalsh-modular/html/docbook.dsl
#PRINT_SS=/usr/lib/sgml/stylesheets/nwalsh-modular/print/docbook.dsl

# For Mac OS X :
#XML_DECL=/usr/lib/sgml/declaration/xml.decl
HTML_SS=/sw/share/sgml/dsssl/docbook-dsssl-nwalsh/html/docbook.dsl
PRINT_SS=/sw/share/sgml/dsssl/docbook-dsssl-nwalsh/print/docbook.dsl

all: manual

#JADE=jade
JADE=openjade

manual: manual.txt manual.ps manual.html

manual.tex: audio_output.xml debugging.xml decoders.xml gfdl.xml glossary.xml history.xml input.xml interface.xml manual.xml overview.xml ports.xml video_output.xml
	$(JADE) -t tex -V %section-autolabel% -d $(PRINT_SS) $(XML_DECL) manual.xml
	perl -i.bak -pe 's/\000//g' $@ && rm $*.tex.bak
# No it's not a joke

manual.html: audio_output.xml debugging.xml decoders.xml gfdl.xml glossary.xml history.xml input.xml interface.xml manual.xml overview.xml ports.xml video_output.xml
	$(JADE) -t sgml -V %section-autolabel% -V nochunks \
         -d $(HTML_SS) $(XML_DECL) manual.xml > $@

manual.dvi: manual.tex modules.eps ps.eps stream.eps ts.eps
	jadetex manual.tex
	jadetex manual.tex
	jadetex manual.tex

manual.ps: manual.dvi
	dvips -f $< > $@

manual.txt: audio_output.xml debugging.xml decoders.xml gfdl.xml glossary.xml history.xml input.xml interface.xml manual.xml overview.xml ports.xml video_output.xml
	$(JADE) -t sgml -V nochunks -d $(HTML_SS) $(XML_DECL) manual.xml > dump.html
	lynx -force_html -dump dump.html > $@
	-rm -f dump.html

clean:
	rm -f manual.txt
	rm -f *.html *.aux *.log *.dvi *.ps *.tex
	rm -f *.bck *~ .\#* \#*


# Makefile for VideoLAN Doc
#  Clément Stenac
#  Anil Daoud

XSLTPROC=xsltproc
LYNX=lynx
JADE=jade

XML_DECL := /usr/share/sgml/declaration/xml.decl
XSL_CHUNK = videolan-doc-chunk.xsl
XSL = videolan-doc.xsl
XSL_TXT = /usr/share/sgml/docbook/stylesheet/xsl/nwalsh/xhtml/html.xsl

all: manual

manual: manual.html all-html manual.pdf manual.txt

# HTML
all-html: *.xml
	xsltproc $(XSL_CHUNK) manual.xml
	mv *.html html

manual.html: *.xml
	xsltproc $(XSL) manual.xml > manual.html

# PDF
manual.pdf: *.xml
	jade -t tex -d stylesheet-print-noicones.dsl -o manual.tex $(XML_DECL) manual.xml
	pdfjadetex manual.tex
	pdfjadetex manual.tex
	pdfjadetex manual.tex
	rm -f *.tex *.out *.aux *.log
	
# TXT
manual.txt:  *.xml
	xsltproc $(XSL_TXT) manual.xml > manual.txt

clean:
	rm -f manual.txt manual.pdf
	rm -f *.html *.aux *.log *.tex *.out
	rm -f html/*.html

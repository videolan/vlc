all: html

html:
	rm -f *.html
	sgmltools -b html -s ../common/stylesheet-html.dsl faq-alone.sgml
	mv faq-alone/*.html .
	rmdir faq-alone
	ln -s book1.html index.html


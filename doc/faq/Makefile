all: clean html html-zip txt pdf ps rtf

html:
	rm -f *.html
	sgmltools -b html -s ../common/stylesheet-html.dsl faq-alone.sgml
	mv faq-alone/*.html .
	rmdir faq-alone
	ln -s book1.html index.html

clean:
	rm -f *.html *.txt *.zip *.pdf *.ps *.rtf

html-zip:
	zip FAQ-vlc-html.zip *.html

txt:
	sgmltools -b txt faq-alone.sgml
	mv -f faq-alone.txt FAQ-vlc.txt

pdf:
	sgmltools -b pdf -s ../common/stylesheet-print-noicones.dsl faq-alone.sgml
	mv -f faq-alone.pdf FAQ-vlc.pdf
	zip FAQ-vlc-pdf.zip FAQ-vlc.pdf

ps:
	sgmltools -b ps -s ../common/stylesheet-print-noicones.dsl faq-alone.sgml
	mv -f faq-alone.ps FAQ-vlc.ps
	zip FAQ-vlc-ps.zip FAQ-vlc.ps

rtf:
	sgmltools -b rtf -s ../common/stylesheet-print-noicones.dsl faq-alone.sgml
	mv -f faq-alone.rtf FAQ-vlc.rtf
	zip FAQ-vlc-rtf.zip FAQ-vlc.rtf

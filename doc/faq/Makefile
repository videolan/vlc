# Makefile for the VideoLAN Documentation

DOC := faq-alone
NAME := FAQ-vlc

# We build HTML, PS, PDF, RTF and TXT for the Web site
all: html ps pdf rtf txt

html:
	sgmltools -b onehtml -s ../common/stylesheet-html.dsl $(DOC).sgml
#	Delete empty file !
	rm -f faq-alone.html
	ln -sf faq.html index.html
	zip $(NAME)-html.zip *.html

ps:
	sgmltools -b ps -s ../common/stylesheet-print-noicones.dsl $(DOC).sgml
	mv -f $(DOC).ps $(NAME).ps
	zip $(NAME)-ps.zip $(NAME).ps

pdf:
	sgmltools -b pdf -s ../common/stylesheet-print-noicones.dsl $(DOC).sgml
	mv -f $(DOC).pdf $(NAME).pdf
	zip $(NAME)-pdf.zip $(NAME).pdf

rtf:
	sgmltools -b rtf -s ../common/stylesheet-print-noicones.dsl $(DOC).sgml
	mv -f $(DOC).rtf $(NAME).rtf
	zip $(NAME)-rtf.zip $(NAME).rtf

txt:
	sgmltools -b txt $(DOC).sgml
	mv -f $(DOC).txt $(NAME).txt

clean:
	rm -f *.zip *.html *.pdf *.ps *.txt *.rtf

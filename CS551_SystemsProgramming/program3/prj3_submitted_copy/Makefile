#Path definitions
PERL= 		/usr/bin/perl
UMT=		/home/zdu/projects/umt/umt
HTML2PS= 	html2ps
PS2PDF=		/usr/bin/ps2pdf

TARGET =	prj3

$(TARGET):		$(TARGET).html $(TARGET).pdf 

%.html:			%.umt
			$(UMT) $< >$@

%.pdf:			%.html
			$(HTML2PS) -N 0 -n $< > $$$$.ps ; \
			$(PS2PDF) $$$$.ps $@; \
			rm -f $$.ps


clean:
			rm -f $(TARGET).html $(TARGET).pdf

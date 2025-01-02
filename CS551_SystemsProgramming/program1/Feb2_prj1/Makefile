#Path definitions
PERL= 		/usr/bin/perl
UMT=		/home/zdu/projects/umt/umt
HTML2PS= 	html2ps
PS2PDF=		/usr/bin/ps2pdf

TARGET =	prj1

$(TARGET):		$(TARGET).html $(TARGET).pdf

$(TARGET).html:		$(TARGET).umt
			$(UMT) $< >$@

$(TARGET).pdf:		$(TARGET).html
			$(HTML2PS) -N 0 -n $(TARGET).html > $(TARGET).ps
			$(PS2PDF) $(TARGET).ps 
			rm -f $(TARGET).ps


clean:
			rm -f $(TARGET).html $(TARGET).pdf

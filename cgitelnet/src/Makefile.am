
CFLAGS = -I${top_srcdir}/include -I${top_srcdir}/support/cgilib

LDADD = -L${top_srcdir}/support/cgilib -lcgilib \
	-L${top_srcdir}/support/libxml -lxml

noinst_PROGRAMS = cgitelnet_input.cgi cgitelnet_msg.cgi cgitelnet.cgi

cgitelnet_input_cgi_SOURCES = \
	cgitelnet_input.cgi.c \
	cgitelnet_config.c

cgitelnet_msg_cgi_SOURCES = \
	cgitelnet_msg.cgi.c \
	cgitelnet_config.c

cgitelnet_cgi_SOURCES = \
	cgitelnet.cgi.c \
	cgitelnet_config.c


install: all-local

all-local:
	cp *.cgi ${top_srcdir}/binaries
	-strip ${top_srcdir}/binaries/*.cgi
	cat ${top_srcdir}/binaries/README

clean-local:
	-rm ${top_srcdir}/binaries/*.cgi


## Process this file with automake to produce Makefile.in

INCLUDES = -I@srcdir@ @CORBA_CFLAGS@ $(VERSION_FLAGS)

VERSION_FLAGS = -DLIBXML_VERSION=\"@LIBXML_VERSION@\"

noinst_LIBRARIES = libxml.a

libxml_a_SOURCES = \
		SAX.c \
		entities.c \
		encoding.c \
		error.c \
		parser.c \
		HTMLparser.c \
		HTMLtree.c \
		debugXML.c \
		tree.c \
		xpath.c \
		xmlIO.c \
		xmlmemory.c \
		nanohttp.c \
		nanoftp.c \
		valid.c \
		xlink.c


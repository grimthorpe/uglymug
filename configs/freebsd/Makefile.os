# Specifics for FreeBSD.
#
# THIS IS NOT A COMPLETE MAKEFILE.
# It is designed to be included from ../../Makefile.
# PJC, 20/4/2003.
WHOAMI:=whoami | sed 's, .*,,'
LIBS:= -lm -lncurses -lcrypt
CFLAGS:= -g -DJPK_FREEBSD

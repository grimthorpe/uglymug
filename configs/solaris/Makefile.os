# Specifics for Solaris.
#
# THIS IS NOT A COMPLETE MAKEFILE.
# It is designed to be included from ../../Makefile.
# PJC, 20/4/2003.
WHOAMI:=who am i | sed 's, .*,,'
LIBS:= -lcurses -lm -ltermcap -lsocket -lnsl
CFLAGS:= -g -DJPK_SPARC

# Specifics for Linux.
#
# THIS IS NOT A COMPLETE MAKEFILE.
# It is designed to be included from ../../Makefile.
# PJC, 20/4/2003.
WHOAMI:=whoami | sed 's, .*,,'
LIBS:= -lm -ldl -lcurses -lcrypt
CFLAGS:= -g -D_GNU_SOURCE

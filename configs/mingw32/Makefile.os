# Specifics for MinGW32.
#
# THIS IS NOT A COMPLETE MAKEFILE.
# It is designed to be included from ../../Makefile.
# PJC, 20/4/2003.
WHOAMI:=echo Somebody
LIBS:= -lws2_32 -lmsvcrt40
CFLAGS:=
EXESUFFIX:=.exe
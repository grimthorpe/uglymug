####################################
#          Which OS?
####################################
#FreeBSD is covered by the Solaris syntax

SOLMAKE_UNAME:sh=uname -sm|sed 's/ /-/g'
GMAKE_UNAME=$(shell uname -sm|sed 's/ /-/g')
SUFFIX:=.$(GMAKE_UNAME)$(SOLMAKE_UNAME)
 
####################################
#           Solaris 2.8
####################################
WHOAMI.sparc:=who am i
LIBS.sparc:= -lcurses -lm -ltermcap -lsocket -lnsl
CFLAGS.sparc:= -g -DJPK_SPARC

WHOAMI.SunOS-sun4c:=$(WHOAMI.sparc)
WHOAMI.SunOS-sun4m:=$(WHOAMI.sparc)
WHOAMI.SunOS-sun4u:=$(WHOAMI.sparc)
LIBS.SunOS-sun4c:=$(LIBS.sparc)
LIBS.SunOS-sun4m:=$(LIBS.sparc)
LIBS.SunOS-sun4u:=$(LIBS.sparc)
CFLAGS.SunOS-sun4c:=$(CFLAGS.sparc)
CFLAGS.SunOS-sun4m:=$(CFLAGS.sparc)
CFLAGS.SunOS-sun4u:=$(CFLAGS.sparc)
BUILD_ENVIRONMENT.SunOS-sun4c:=$(BUILD_ENVIRONMENT.sparc)
BUILD_ENVIRONMENT.SunOS-sun4m:=$(BUILD_ENVIRONMENT.sparc)
BUILD_ENVIRONMENT.SunOS-sun4u:=$(BUILD_ENVIRONMENT.sparc)

####################################
#              Linux
####################################
WHOAMI.i386-linux:=whoami
LIBS.i386-linux:= -lm -ldl -lcurses -lcrypt
CFLAGS.i386-linux:= -g -DJPK_Linux

WHOAMI.Linux-i386:=$(WHOAMI.i386-linux)
WHOAMI.Linux-i486:=$(WHOAMI.i386-linux)
WHOAMI.Linux-i586:=$(WHOAMI.i386-linux)
WHOAMI.Linux-i686:=$(WHOAMI.i386-linux)
LIBS.Linux-i386:=$(LIBS.i386-linux)
LIBS.Linux-i486:=$(LIBS.i386-linux)
LIBS.Linux-i586:=$(LIBS.i386-linux)
LIBS.Linux-i686:=$(LIBS.i386-linux)
CFLAGS.Linux-i386:=$(CFLAGS.i386-linux)
CFLAGS.Linux-i486:=$(CFLAGS.i386-linux)
CFLAGS.Linux-i586:=$(CFLAGS.i386-linux)
CFLAGS.Linux-i686:=$(CFLAGS.i386-linux)
BUILD_ENVIRONMENT.Linux-i386:=$(BUILD_ENVIRONMENT.i386-linux)
BUILD_ENVIRONMENT.Linux-i486:=$(BUILD_ENVIRONMENT.i386-linux)
BUILD_ENVIRONMENT.Linux-i586:=$(BUILD_ENVIRONMENT.i386-linux)
BUILD_ENVIRONMENT.Linux-i686:=$(BUILD_ENVIRONMENT.i386-linux)

####################################
#            FreeBSD
####################################
WHOAMI.freebsd:=whoami
LIBS.freebsd:= -lm -lncurses -lcrypt
CFLAGS.freebsd:= -g -DJPK_FREEBSD
BUILD_ENVIRONMENT.freebsd:=freebsd

WHOAMI.FreeBSD-i386:=$(WHOAMI.freebsd)
LIBS.FreeBSD-i386:=$(LIBS.freebsd)
CFLAGS.FreeBSD-i386:=$(CFLAGS.freebsd)
BUILD_ENVIRONMENT.FreeBSD-i386:=$(BUILD_ENVIRONMENT.freebsd)


####################################
#            MinGW32
####################################
WHOAMI.mingw32:=echo Somebody
LIBS.mingw32:= -lws2_32
CFLAGS.mingw32:= -Iconfigs/mingw32
BUILD_ENVIRONMENT.mingw32:=mingw32

# Windows XP on Athlon MP+ (may also work with other systems, untested. PJC 11/4/03)
WHOAMI.MINGW32_NT-5.1-i686:=$(WHOAMI.mingw32)
LIBS.MINGW32_NT-5.1-i686:=$(LIBS.mingw32)
CFLAGS.MINGW32_NT-5.1-i686:=$(CFLAGS.mingw32)
BUILD_ENVIRONMENT.MINGW32_NT-5.1-i686:=$(BUILD_ENVIRONMENT.mingw32)


#############################################

WHOAMI:=$(WHOAMI$(SUFFIX))
LIBS:=$(LIBS$(SUFFIX))
CFLAGS:=$(CFLAGS$(SUFFIX))
BUILD_ENVIRONMENT:=$(BUILD_ENVIRONMENT$(SUFFIX))

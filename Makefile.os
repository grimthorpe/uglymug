####################################
#          Which OS?
####################################
#FreeBSD is covered by the Solaris syntax
SOLMAKE_UNAME:sh=uname -sm|sed 's/ /-/g'
GMAKE_UNAME=$(shell uname -sm|sed 's/ /-/g')
SUFFIX:=$(GMAKE_UNAME)$(SOLMAKE_UNAME)
 
####################################
#          OS-specific strings
####################################
# Solaris 2.8
BUILD_ENVIRONMENT.SunOS-sun4c:=solaris
BUILD_ENVIRONMENT.SunOS-sun4m:=solaris
BUILD_ENVIRONMENT.SunOS-sun4u:=solaris

# Linux
BUILD_ENVIRONMENT.Linux-i386:=linux
BUILD_ENVIRONMENT.Linux-i486:=linux
BUILD_ENVIRONMENT.Linux-i586:=linux
BUILD_ENVIRONMENT.Linux-i686:=linux
# Yes, some people are running this.
BUILD_ENVIRONMENT.Linux-sparc:=linux

# FreeBSD
BUILD_ENVIRONMENT.FreeBSD-i386:=freebsd

# MinGW32
# Windows XP on Athlon MP+ (may also work with other systems, untested. PJC 11/4/03)
BUILD_ENVIRONMENT.MINGW32_NT-5.1-i686:=mingw32

####################################
#  Our specific build environment
####################################
BUILD_ENVIRONMENT:=$(BUILD_ENVIRONMENT.$(SUFFIX))

/** \file os.h
 * Configurations that are specific to each operating system.
 */

#ifndef	OS_H_DEFINED
#define	OS_H_DEFINED

// Linux
#ifdef linux
#	define	HAS_CURSES	1
#	define	HAS_FORK	1
#	define	HAS_SIGNALS	1
#	define	NEEDS_CRYPT_H	1
#	define	NEEDS_RESOURCES	0
#	define	NEEDS_STRSIGNAL	0
#	define	USE_BSD_SOCKETS	1
#	define	USE_WINSOCK2	0
#endif /* linux */ 

// Sun - typically Solaris these days.
#if sun
#	define	HAS_CURSES	1
#	define	HAS_FORK	1
#	define	HAS_SIGNALS	1
#	define	NEEDS_CRYPT_H	1
#	define	USE_BSD_SOCKETS	1
#	define	USE_WINSOCK2	0
#endif /* sun */ 

// MinGW
#ifdef __MINGW32__
#	define	HAS_CURSES		0
#	define	HAS_FORK		0
#	define	HAS_SIGNALS		0
#	define	NEEDS_CRYPT_H		1
#	define	NEEDS_GETDTABLESIZE	1
#	define	NEEDS_GETOPT		1
#	define	NEEDS_RESOURCES		0
#	define	NEEDS_STRSIGNAL		1
#	define	USE_BSD_SOCKETS		0
#	define	USE_WINSOCK2		1
#endif

// Windows (2000)
#ifdef _WIN32
#	define	HAS_CURSES		0
#	define	HAS_FORK		0
#	define	HAS_SIGNALS		0
#	define	NEEDS_CRYPT_H		1
#	define	NEEDS_GETDTABLESIZE	1
#	define	NEEDS_GETOPT		1
#	define	NEEDS_RESOURCES		0
#	define	NEEDS_STRSIGNAL		1
#	define	USE_BSD_SOCKETS		0
#	define	USE_WINSOCK2		1
#	define	strncasecmp		strnicmp
#	define	vsnprintf		_vsnprintf
#	define	snprintf		_snprintf
#endif

// Defaults - assume a UNIX of some kind.
#ifndef	HAS_CURSES
#	define	HAS_CURSES	1
#endif
#ifndef	HAS_FORK
#	define	HAS_FORK	1
#endif
#ifndef	HAS_SIGNALS
#	define	HAS_SIGNALS	1
#endif
#ifndef	NEEDS_GETDTABLESIZE
#	ifdef	SYSV
#		define	NEEDS_GETDTABLESIZE	1
#	else
#		define	NEEDS_GETDTABLESIZE	1
#	endif	/* SYSV */
#endif
#ifndef	NEEDS_RESOURCES
#	define	NEEDS_RESOURCES	1
#endif
#ifndef	NEEDS_STRSIGNAL
#	define	NEEDS_STRSIGNAL	0
#endif
#ifndef	USE_WINSOCK2
#	define	USE_WINSOCK2	0
#endif
#ifndef	USE_BSD_SOCKETS
#	define	USE_BSD_SOCKETS	1
#endif

#endif	/* OS_H_DEFINED */

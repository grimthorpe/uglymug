/** \file
 * Configurations that are specific to each operating system.
 */

#ifndef	OS_H_DEFINED
#define	OS_H_DEFINED

// Linux
#ifdef linux
#	define	HAS_FORK	1
#	define	HAS_SIGNALS	1
#	define	NEEDS_CRYPT_H	1
#	define	NEEDS_RESOURCES	0
#	define	USE_BSD_SOCKETS	1
#	define	USE_WINSOCK2	0
#endif /* linux */ 

// Sun - typically Solaris these days.
#if sun
#	define	HAS_FORK	1
#	define	HAS_SIGNALS	1
#	define	NEEDS_CRYPT_H	1
#	define	USE_BSD_SOCKETS	1
#	define	USE_WINSOCK2	0
#endif /* sun */ 

// MinGW
#ifdef __MINGW32__
#	define	HAS_FORK	0
#	define	HAS_SIGNALS	0
#	define	HAS_SRAND48	0
#	define	NEEDS_CRYPT_H	1
#	define	NEEDS_GETOPT	1
#	define	NEEDS_RESOURCES	0
#	define	USE_BSD_SOCKETS	0
#	define	USE_WINSOCK2	1
#endif

// Defaults - assume a UNIX of some kind.
#ifndef	HAS_FORK
#	define	HAS_FORK	1
#endif
#ifndef	HAS_SIGNALS
#	define	HAS_SIGNALS	1
#endif
#ifndef	HAS_SRAND48
#	define	HAS_SRAND48	1
#endif
#ifndef	NEEDS_RESOURCES
#	define	NEEDS_RESOURCES	1
#endif
#ifndef	USE_WINSOCK2
#	define	USE_WINSOCK2	0
#endif
#ifndef	USE_BSD_SOCKETS
#	define	USE_BSD_SOCKETS	1
#endif

#endif	/* OS_H_DEFINED */

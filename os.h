/** \file
 * Configurations that are specific to each operating system.
 */

#ifndef	OS_H_DEFINED
#define	OS_H_DEFINED

// Linux
#ifdef linux
#	define	NEEDS_CRYPT_H	1
#	define	NEEDS_RESOURCES	0
#endif /* linux */ 

// Sun
#if sun
#	define NEEDS_CRYPT_H	1
#endif /* sun */ 

// MinGW
#ifdef __MINGW32__
#	define	HAS_FORK	0
#	define	HAS_SIGNALS	0
#	define	NEEDS_CRYPT_H	1
#	define	NEEDS_GETOPT	1
#	define	NEEDS_RESOURCES	0
#endif

// Defaults
#ifndef	HAS_FORK
#	define	HAS_FORK	1
#endif
#ifndef	HAS_SIGNALS
#	define	HAS_SIGNALS	1
#endif
#ifndef	NEEDS_RESOURCES
#	define	NEEDS_RESOURCES	1
#endif

#endif	/* OS_H_DEFINED */

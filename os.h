/** \file
 * Configurations that are specific to each operating system.
 */

#ifndef	OS_H_DEFINED
#define	OS_H_DEFINED

// Linux
#ifdef linux
#	define	HAS_RESOURCES	0
#	define	NEEDS_CRYPT_H	1
#endif /* linux */ 

// Sun
#if sun
#	define NEEDS_CRYPT_H	1
#endif /* sun */ 

// MinGW
#ifdef __MINGW32__
#	define	HAS_RESOURCES	0
#	define	NEEDS_CRYPT_H	1
#endif

// Defaults
#ifndef	HAS_RESOURCES
#	define	HAS_RESOURCES	1
#endif

#endif	/* OS_H_DEFINED */

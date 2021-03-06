/**
 * Header for cryptography functions.
 */

#ifndef	_CRYPT_H
#define	_CRYPT_H

#ifdef	__cplusplus
extern "C" {
#endif

	char *crypt (const char *pw, const char *salt);

#ifdef	__cplusplus
};
#endif

#endif	/* _CRYPT_H */

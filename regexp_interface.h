/*
 * regexp_interface.h: Interface to regular expression stuff,
 *	to keep it in C with only one copy.
 *
 *	Peter Crowther, 28/9/93.
 */

#ifdef	__cplusplus
	extern "C"
	{
#endif
		char	*compile	(const char *, char *, char *, int);
		int	step		(const char *, const char *);
#ifdef	__cplusplus
	};
#endif

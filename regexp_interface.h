/*\file
 * Interface to regular expression stuff, to keep it in C with only one copy.
 */

char	*compile	(const char *, char *, char *, int);
int	step		(const char *, const char *);

/*
 * regexp.c: A fixed place to put regular-expression stuff.
 *
 *	Peter Crowther, 28/9/93.
 */

#include <stdio.h>

#define INIT		register const char *sp = instring;
#define GETC()		(*sp++)
#define PEEKC()		(*sp)
#define UNGETC(c)	(--sp)
#define RETURN(c)	return;
#define ERROR(c)	fprintf (stderr, "BUG: Problem with regexp: %d\n", c);

/* static char SCCSid[] = "@(#)regexp.c	1.4\t8/12/94"; */

#include "regexp.h"

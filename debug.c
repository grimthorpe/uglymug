/*
 * Debug.c
 *
 * Helpful routines to help debugging.
 */

#include "copyright.h"
#include "debug.h"
#include "config.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>


void
Trace(const char* fmt, ...) {

#ifndef NEW_LOGGING	
	static time_t lasttime = 0;
	time_t currtime;

	currtime = time(0);
	if(currtime - lasttime >= 10)
	{
		fprintf(stderr, "TIME: %s", asctime(localtime(&currtime)));
		lasttime = currtime;
	}
#endif

	va_list vl;
	va_start(vl, fmt);
/* JPK Hack */
/* fprintf(stderr, "Argument fmt string was: %s\n", fmt); */
	vfprintf(stderr, fmt, vl);
	va_end(vl);
}


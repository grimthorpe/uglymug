/*
 * Debug.c
 *
 * Helpful routines to help debugging.
 */

#include "copyright.h"
#include "debug.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>

void
Trace(const char* fmt, ...)
{
static time_t lasttime = 0;
time_t currtime;

	currtime = time(0);
	if(currtime - lasttime >= 10)
	{
		fprintf(stderr, "TIME: %s", asctime(localtime(&currtime)));
		lasttime = currtime;
	}

	va_list vl;
	va_start(vl, fmt);
	vfprintf(stderr, fmt, vl);
	va_end(vl);
}


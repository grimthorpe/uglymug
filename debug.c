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
	va_list vl;
	va_start(vl, fmt);

	vfprintf(stderr, fmt, vl);
	va_end(vl);
}

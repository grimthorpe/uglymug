static char SCCSid[] = "@(#)memory.c	1.2\t7/19/94";
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

#define MEMORY_LOG_FILE "memory.log"

static	FILE	*mem_file = NULL;

extern	void	free		(void *mptr);
extern	void	*malloc		(int size);
extern	void	*realloc	(void *, int);

void init_mud_memory ()
{
	if ((mem_file = fopen (MEMORY_LOG_FILE, "w")) == NULL)
		Trace( "Cannot open \"%s\" for writing.\n",
			MEMORY_LOG_FILE);
}


void
close_mud_memory ()

{
	if (mem_file != NULL)
	{
		fclose (mem_file);
		mem_file = NULL;
	}
}

void *mud_malloc (int size)
{
	void
		*mptr;

	mptr = malloc (size);
	if (mem_file != NULL)
	{
		fprintf (mem_file, "m %5d %9x\n", size, mptr);
		fflush (mem_file);
	}
	if (mptr == NULL)
	{
		panic ("Out Of Memory...");
	}
	return mptr;
}

void mud_free (void *mptr)
{
	if (mem_file != NULL)
	{
		fprintf (mem_file, "f       %9x\n", mptr);
		fflush (mem_file);
	}
	free (mptr);
}

void *mud_realloc (void *mptr, int size)
{
	void
		*nptr;

	nptr = realloc (mptr, size);
	if (mem_file != NULL)
	{
		fprintf (mem_file, "r %5d %9x %9x\n", size, mptr, nptr);
		fflush (mem_file);
	}
	if (nptr == NULL)
		panic ("Out Of Memory...");
	return nptr;
}

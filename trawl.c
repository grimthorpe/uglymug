static char SCCSid[] = "@(#)trawl.c	1.3\t7/19/94";
/* 
 * Name		: OneShot
 *
 * Purpose	: Utility for UglyMug. Resets all the player logon times to
 *		  zero. Reads a db from stdin, and writes one to stdout.
 *
 * Author	: Ian 'Efficient' Whalley.
 *
 * Requires	: Usual Ugly util modules.
 *
 * Instructions	: Reads from stdin, writes diagnostics to stderr; and
 *		  the list to stdout. This the usual method of calling
 *		  is:-	sadness < ugly.db.new > LIST
 *
 * Credits	: The phrase 'Awww _BIZ_' appears courtesy of Captain
 *                Whimsey productions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "db.h"
#include "externs.h"
#include "command.h"
#include "config.h"
#include "context.h"
#include "copyright.h"
#include "game.h"
#include "interface.h"
#include "match.h"
#include "memory.h"
#include "regexp_interface.h"
#include "stack.h"

#define APPNAME				"OneShot"

// Make game.c happy
const char* version = "trawl; unknown version.";
int dump_interval = 10000000;

/*
 * Function.
 */

void print_helpful_message(const char *msg)
{
	fprintf(stderr, "%s: %s\n",
			APPNAME, msg);
}

/*
 * Main.
 */

int main ()
{
	int		i;

	fprintf(stderr, "%s diagnostic: Starting db.read() from stdin.\n",
				APPNAME);
	if (db.read(stdin) < 0)
	{
		fprintf(stderr, "%s error: Cannot read DB\n",
				APPNAME);
		exit(1);
	}
	print_helpful_message("db.read() completed. Beginning sensor sweep.");
	for (i = 0 ; i < db.get_top(); i++)
	{
		if (Typeof(i) == TYPE_COMMAND)
			db[i].set_flag(FLAG_BACKWARDS);
		if (Typeof(i) == TYPE_PLAYER)
		{
			if (Builder(i) || Wizard(i) || (Apprentice(i)))
				db[i].set_flag(FLAG_LINENUMBERS);
			db[i].clear_flag(FLAG_ERIC);
			db[i].clear_flag(FLAG_OFFICER);
			db[i].set_flag(FLAG_FCHAT);
		}
	}
	print_helpful_message("Sweep complete.");
	db.write(stdout);
	return 0;
}

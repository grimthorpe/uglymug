/* static char SCCSid[] = "@(#)trawl.c	1.3\t7/19/94"; */
/* 
 * Name		: SpoonTrawl
 *
 * Purpose	: Utility for UglyMug. Does a trawl and a function
 *		   Reads a db from stdin, and writes one to stdout.
 *
 * Author	: ReaperMoo
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

#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include "mudstring.h"
#include "db.h"
#include "externs.h"
#include "command.h"
#include "config.h"
#include "context.h"
#include "copyright.h"
#include "game.h"
#include "interface.h"
#include "match.h"
#include "regexp_interface.h"

#define APPNAME				"SpoonTrawl"

// Make game.c happy
const char* version = "trawl; unknown version.";
const char* release = "UNKNOWN";

int dump_interval = 10000000;

/*
 * Function.
 */

void print_helpful_message(const char *msg)
{
	Trace("%s: %s\n",
			APPNAME, msg);
}

/*
 * Main.
 */

int main ()
{
	int		i;

	Trace("%s diagnostic: Starting db.read() from stdin.\n",
				APPNAME);
	if (db.read(stdin) < 0)
	{
		Trace("%s error: Cannot read DB\n",
				APPNAME);
		exit(1);
	}
	print_helpful_message("db.read() completed. Begin zeroing.");
	int drogna=0;

	for (i = 0 ; i < db.get_top(); i++)
	{
		if ((Typeof(i) == TYPE_PLAYER) || (Typeof(i) == TYPE_PUPPET))
		{
			drogna = drogna + db[i].get_money();
			db[i].set_money(0);
		}
	}
	Trace("%s: Total Drogna removed:%d\n",APPNAME, drogna);
	db.write(stdout);
	return 0;

}

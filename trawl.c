/* static char SCCSid[] = "@(#)trawl.c	1.3\t7/19/94"; */
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

#define APPNAME				"OneShot"

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
	int		total = 0;

	Trace("%s diagnostic: Starting db.read() from stdin.\n",
				APPNAME);
	if (db.read(stdin) < 0)
	{
		Trace("%s error: Cannot read DB\n",
				APPNAME);
		exit(1);
	}
	print_helpful_message("db.read() completed. Beginning sensor sweep.");
	time_t now;
	now = time(0);
#define YEARSECS (60*60*24*365)
#define MONTHSECS (60*60*24*31)

	for (i = 0 ; i < db.get_top(); i++)
	{
		if (Typeof(i) == TYPE_PLAYER)
		{
			if(!Builder(i) && !Apprentice(i) && !Welcomer(i) && !Wizard(i))
			if(true)
			{
				if((atoi(db[i].get_ofail().c_str()) < 3600) // Less than 1 hour connection
				&& (now - atoi(db[i].get_fail_message().c_str()) > YEARSECS))
				{
					printf("#%d\n", i);
					total++;
				}
			}
		}
	}
	printf("Total: %d\n", total);
	return 0;
}

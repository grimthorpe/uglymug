/* static char SCCSid[] = "@(#)mondodestruct.c	1.4\t7/19/94"; */
/* 
 * Name		: Mondodestruct
 *
 * Purpose	: Compiles a list of rooms that haven't been used
 *		  for ages
 *
 * Author	: Ian 'Efficient' Whalley. (Hacked by The ReaperMan)
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

/*
 * A few of these header files may well seem unnecessary, but they
 * are all needed, because the code is split up so badly.
 */

#include "db.h"
#include "externs.h"
#include "command.h"
#include "config.h"
#include "context.h"
#include "copyright.h"
#include "game.h"
#include "interface.h"
#include "destroy.h"
#include "match.h"
#include "regexp_interface.h"

#define APPNAME				"Mondo Destruction"

typedef struct {dbref			who;
		long			howlong; } record;

/*
 * Global variables.
 */

time_t			starttime;
int			maxposs = 0;

/*
 * Predeclarations.
 */

void			add_to_list(dbref, long);
char			*my_time_string (time_t);
char			*short_time_string (time_t);
void			print_helpful_message(char *);


/*
 * I nicked this from look.c. Ian 'Flup' Chard wrote it.
 */

char *my_time_string (time_t interval)
{
	static char	buffer[80];
	char		scratch_buffer[200];
	int		days, hours, minutes;

	interval -= (days = interval / 86400) * 86400;
	interval -= (hours = interval / 3600) * 3600;
	interval -= (minutes = interval / 60) * 60;

	*buffer = '\0';

	if (days > 0)
	{
		sprintf (scratch_buffer, "%d day%s, ", days, PLURAL (days));
		strcat (buffer, scratch_buffer);
	}
	if (hours > 0 || days > 0)
	{
		sprintf (scratch_buffer, "%d hour%s, ", hours, PLURAL (hours));
		strcat (buffer, scratch_buffer);
	}
	if (minutes > 0 || hours > 0 || days > 0)
	{
		sprintf (scratch_buffer, "%d minute%s and ", minutes, PLURAL (minutes));
		strcat (buffer, scratch_buffer);
	}
	sprintf (scratch_buffer, "%d second%s", interval, PLURAL (interval));
	strcat (buffer, scratch_buffer);

	return buffer;
}

/*
 * This is fluff. It timestamps the diagnostics printed to stderr.
 */

char *short_time_string(long interval)
{
	static char	buffer[12];
	int		minutes;

	interval -= (minutes = interval / 60) * 60;

	sprintf(buffer, "%d.%d", minutes, interval);
	return buffer;
}

void print_helpful_message(char *msg)
{
	time_t		now;
	char		*scratch;

	time(&now);
	scratch = strdup(short_time_string(now - starttime));
	fprintf(stderr, "%s [%s]: %s\n",
			APPNAME, scratch, msg);
}

/*
 * Main.
 */

int main ()
{
	int		i;
	int		month;
	time_t		threshtime;

	time(&starttime);
	fprintf(stderr, "%s diagnostic: Starting db.read() from stdin.\n",
				APPNAME);
	if (db.read(stdin) < 0)
	{
		fprintf(stderr, "%s error: Cannot read DB\n",
				APPNAME);
		exit(1);
	}

	print_helpful_message("db.read() completed.  Begining first trawl");

	fprintf(stdout, "\n**** ROOMS NOT USED SINCE TIME BEGAN ****\n\n");
	for (i = 0 ; i < db.get_top(); i++)
	{
		if ((Typeof(i) == TYPE_ROOM) && !Wizard(i))
		{
			if(db[i].get_last_entry_time() == 0)
			{
				fprintf(stdout, "#%d   : %s [%s]\n", i, db[i].get_name(), db[db[i].get_owner()].get_name());
				db[i].set_flags(db[i].get_flags() | WIZARD);
			}
		}
	}

	for(month = 14 ; month > 3 ; month--)
	{
		fprintf(stdout, "\n**** ROOMS LAST USED %d MONTHS AGO ****\n\n",month);
		threshtime = starttime - (31*24*60*60*month);
		for (i = 0 ; i < db.get_top(); i++)
		{
			if ((Typeof(i) == TYPE_ROOM) && (!Wizard(i)))
			{
				if(db[i].get_last_entry_time() < threshtime)
				{
					fprintf(stdout, "#%d   : %s [%s]\n", i, db[i].get_name(), db[db[i].get_owner()].get_name());
					db[i].set_flags(db[i].get_flags() | WIZARD);
				}
			}
		}
		print_helpful_message("Month scan completed.");
	}
	print_helpful_message("Run complete.");
	return 0;
}

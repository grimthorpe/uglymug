/* static char SCCSid[] = "@(#)sadness.c	1.4\t7/19/94"; */
/* 
 * Name		: Sadness
 *
 * Purpose	: Utility for UglyMug. Compiles a list of logon times for
 *		  players sorted into order.
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
#include "match.h"
#include "memory.h"
#include "regexp_interface.h"
#include "stack.h"

/*
 * The define 'DIV' is used to determine in what size steps the player
 * array increases in size. It has been tested down to 1, which works
 * perfectly well, but is not recomended, as this will result in a lot
 * of realloc()ing. And I mean _a_lot_. A value of 1000 is recommended.
 */

#define DIV				500
#define APPNAME				"Sadness"

typedef struct {dbref			who;
		long			howlong; } record;

/*
 * Global variables.
 */

record			*bigarray;
int			numberofplayers = 0;
time_t			starttime;
int			maxposs = 0;

/*
 * Predeclarations.
 */

void			add_to_list(dbref, long);
int			record_compare(record *, record *);
void			sort_list_out();
void			print_list_out();
char			*my_time_string (time_t);
char			*short_time_string (time_t);
void			print_helpful_message(char *);

/*
 * Functions.
 */

void add_to_list(dbref i, long timeything)
{
	record			*banana;

	if (numberofplayers > (maxposs - 1))
	{
		fprintf(stderr, "Realloc()ing array - size %d --> ", maxposs);
		maxposs += DIV;
		fprintf(stderr, "%d.\n", maxposs);
		if ((banana = (record *)realloc(bigarray, (sizeof(record) * maxposs))) == NULL)
		{
			fprintf(stderr, "%s error: Awww _BIZ_! Failed in realloc().\n", APPNAME);
			exit(1);
		}
		bigarray = banana;
	}
	bigarray[numberofplayers].who = i;
	bigarray[numberofplayers].howlong = timeything;
	numberofplayers++;
}

int record_compare(record *one, record *two)
{
	return((two->howlong) - (one->howlong));
}

void sort_list_out()
{
	qsort(bigarray, numberofplayers, sizeof(record), record_compare);
}

void print_list_out()
{
	int		i = 0;

	for (i = 0; i < numberofplayers; i++)
		printf("%4d) %-20s: %s\n", (i+1), db[bigarray[i].who].get_name(),
					my_time_string(bigarray[i].howlong));
}

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
	long		total;

	time(&starttime);
	bigarray = (record *)calloc(DIV, sizeof(record));
	maxposs = DIV;
	fprintf(stderr, "%s diagnostic: Starting db.read() from stdin.\n",
				APPNAME);
	if (db.read(stdin) < 0)
	{
		fprintf(stderr, "%s error: Cannot read DB\n",
				APPNAME);
		exit(1);
	}
	print_helpful_message("db.read() completed. Constructing initial list.");
	for (i = 0 ; i < db.get_top(); i++)
	{
		if (Typeof(i) == TYPE_PLAYER)
		{
			if (db[i].get_ofail())
				total = atol(db[i].get_ofail());
			else
				total = 0;
			if (total)
				add_to_list(i, total);
		}
	}
	print_helpful_message("Initial scan completed.");
	print_helpful_message("Now sorting list.");
	sort_list_out();
	print_list_out();
	print_helpful_message("Run complete.");
	return 0;
}

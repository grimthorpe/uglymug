/* 
 * Name		: AshCan Alley
 *
 * Purpose	: Utility for UglyMug. Find all players who are good
 *		  contenders for the ASHCAN.
 *
 * Author	: Chisel
 *
 * Requires	: Usual Ugly util modules.
 *
 * Instructions	: Reads from stdin, writes diagnostics to stderr; and
 *		  the list to stdout. This the usual method of calling
 *		  is:-	
 *                       ashit > /dev/null 2>STDERR
 *
 * Credits	: The phrase 'Awww _BIZ_' appears courtesy of Captain
 *                Whimsey productions.
 *
 */


/*
	Players:

	fail message  = last connect time
	ofail message = total time connected
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
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


#define dfprintf			if (debug && !silent) fprintf
#define dprint_helpful_message		if (debug && !silent) print_helpful_message
#define sfprintf			if (!silent) fprintf

#define	RED				"\033[31m"
#define	GREEN				"\033[32m"
#define YELLOW				"\033[33m"
#define	BLUE				"\033[34m"
#define MAGENTA				"\033[35m"
#define	CYAN				"\033[36m"

#define	RED_BG				"\033[41m"
#define	GREEN_BG			"\033[42m"
#define YELLOW_BG			"\033[43m"
#define	BLUE_BG				"\033[44m"
#define MAGENTA_BG			"\033[45m"
#define	CYAN_BG				"\033[46m"

#define INVERSE				"\033[7m"
#define BRIGHT				"\033[1m"
#define REVERT				"\033[0m"

#define APPNAME				"\033[33mAshCan Alley\033[0m"
#define ID_STRING			"\033[41m#%-5d\033[0m"

// Make game.c happy
const char* version = "trawl; unknown version.";
int dump_interval = 10000000;


/* Define a strucure for our ash-canning rules:
   min_connect:		the minimum amount of time to be connected (seconds)
   last_connect:	the interval you must have connected in (seconds)
   min_string:		a message to explain min_connect
   last_string:		a message to explain last_connect

   e.g.
	{ 10, 10, "< 10s", "not connected on last 10s" }
//   means:
	player needs to have connected in for at least 10 seconds, and also last connected in the last 10 seconds
*/




typedef struct ash_rule
{
	unsigned	int	min_connect;
	unsigned	int	last_connect;
	const		char	*min_string;
	const		char	*last_string;
} Ash_Rule;


Ash_Rule	non_builder[] =
{
 {	60,		172800,		"connected less than 60 seconds",	"not connected in last 2 days"		},
 {	600,		604800,		"connected less than 10 minutes",	"not connected in last week"		},
 {	3600,		2419200,	"connected less than 1 hour",		"not connected in last 4 weeks"		},
 {	86400,		31536000,	"connected less than 1 day",		"not connected in last year"		},
 {	0,		0,		NULL,					NULL					}
};


Ash_Rule	builder[] =
{
// {	3600,		15724800,	"connected less than 1 hour",		"not connected in last 26 weeks"	},
 {	0,		0,		NULL,					NULL					}
};


static	int	debug		= false;
static	int	dump		= false;
static	int	save_all	= false;
static	int	set_ash		= false;
static	int	silent		= false;
static	FILE	*trash_list	= NULL;

/*
 * Usage
 */
static void usage()
{
	fprintf (stderr, "\nusage: ashcheck [-a] [-c] [-d] [-f infile] [-h] [-l listfile] [-s]\n\n");
	fprintf (stderr, "\t-a,      --ashcan         set AshCan flag on qualifying players\n");
	fprintf (stderr, "\t-c,      --clear          remove AshCan flag from all players\n");
	fprintf (stderr, "\t-d,      --debug          show diagnostic/debugging messages\n");
	fprintf (stderr, "\t-f FILE, --file FILE      read from FILE instead of stdin\n");
	fprintf (stderr, "\t-h,      --help           show this message\n");
	fprintf (stderr, "\t-l FILE, --list FILE      output to file commands to set players ashcan\n");
	fprintf (stderr, "\t-s,      --silent         suppress messages\n");
	fprintf (stderr, "\n");
}

/*
 * helpful_message
 */

void print_helpful_message(const char *msg)
{
	sfprintf(stderr, "%s: %s\n",
			APPNAME, msg);
}


/*
 * realtime
 */

const char *
realtime (const char *in_time)
{
	long	now;
	static	char time_buf[25];

	if (in_time == NULL || *in_time == '\0')
		time (&now);
	else
		now = atol (in_time);


	strcpy (time_buf, asctime (localtime (&now)));

	time_buf[24] = 0;

	return (time_buf);
}


/*
 * check_time
 */

int check_player(const int player_id)
{
	static		int		lc;		/* Time last connected */
	static		int		lci;		/* Interval last connected (X days ago...) */
	static		int		tc;		/* Total connected time */
	static		long		now;
	static		int		i;
	static		bool		AshCan_Player = false;

	time(&now);
	
	if (Typeof(player_id) != TYPE_PLAYER)
	{
		sfprintf(stderr, "You must supply the #id of a PLAYER!\n");
		return 0;
	}

	/* Get the last connect, and total connect times (in seconds) */
	if (db[player_id].get_ofail() != NULL)
		tc = atol(db[player_id].get_ofail());
	else
		tc = -1;

	if (db[player_id].get_fail_message() != NULL)
	{
		lc = atol(db[player_id].get_fail_message());
		lci = now - lc;
	}
	else
	{
		lc = -1;
	}

	dfprintf (stderr, "--------\n[#%-5d] %s (%sP%s%s%s)\n",
			player_id,
			db[player_id].get_name(),
			(Ashcan(player_id)	? "!" : ""),
			(Wizard(player_id)	? "W" : ""),
			(Apprentice(player_id)	? "A" : ""),
			(Builder(player_id)	? "B" : "")
		);

	if (Wizard(player_id))
	{
		dfprintf(stderr, "[#%-5d] *** We never Ashcan a Wizard ***\n", player_id);
		return false;
	}
	else if (Apprentice(player_id))
	{
		dfprintf(stderr, "[#%-5d] *** We never Ashcan an Apprentice ***\n", player_id);
		return false;
	}


	dfprintf(stderr, "[#%-5d] Last Connected: %s\n",		player_id, realtime(db[player_id].get_fail_message()));
	dfprintf(stderr, "[#%-5d]                 %s ago\n",		player_id, time_string(lci));
	dfprintf(stderr, "[#%-5d] Total Connect:  %s\n",		player_id, time_string(tc));


	if ((tc < 0) && (lc < 0))
	{
		dfprintf (stderr, "[#%-5d] %sPlayer has never connected%s.\n", player_id, RED, REVERT);
		AshCan_Player = false;
	}
	else
	{
		i = 0;
		AshCan_Player = false;
		
		if (Builder(player_id))
		{
			while ((!AshCan_Player) && (builder[i].min_connect != 0))
			{
				if ((tc < (signed long) builder[i].min_connect) && (lci > (signed long) builder[i].last_connect))
				{
					sfprintf (stderr, "[#%-5d] %s%s, %s%s\n", player_id, GREEN, builder[i].min_string, builder[i].last_string, REVERT);
					AshCan_Player = true;
				}

				i++;
			}
		}
		else
		{
			while ((!AshCan_Player) && (non_builder[i].min_connect != 0))
			{
				if ((tc < (signed long) non_builder[i].min_connect) && ((signed long) non_builder[i].last_connect < lci))
				{
					sfprintf (stderr, "[#%-5d] %s%s, %s%s\n", player_id, GREEN, non_builder[i].min_string, non_builder[i].last_string, REVERT);
					AshCan_Player = true;
				}

				i++;
			}
		}
	}

	if (AshCan_Player == true)
	{
		sfprintf(stderr, "[%s#%-5d%s] %sThis Player Is A Candidate For Being%s %s%s%s AshCanned %s\n",
				RED_BG, player_id, REVERT,
				CYAN, REVERT,
				RED_BG, YELLOW, YELLOW, REVERT);

		if (trash_list != NULL)
		{
			time_t last;
			last = atol(db[player_id].get_fail_message());
			fprintf (trash_list, "@rem -- %s (#%d)\n@rem == Last connect: %s", db[player_id].get_name(), player_id, ctime(&last));
			fprintf (trash_list, "@rem -- %s, %s\n", non_builder[i].min_string, non_builder[i].last_string);			
			fprintf (trash_list, "@set #%-5d = ASHCAN\n\n", player_id);
		}
	}
	else
	if (tc > 0)
	{
		dfprintf(stderr, "[#%-5d] %sNo Matching Rules - Player Safe%s\n", player_id, GREEN, REVERT);
	}
	

	
	return AshCan_Player;
}

/*
 * Main.
 */

int main (int argc, char *argv[])
{
	int		i;
	FILE		*dbfile = stdin;

	argc = argc;

	/*
	 * Process Command-Line Arguments
	 */
	for (i=1; argv[i]; i++)
	{
		if (!strcmp(argv[i], "-Ash"))
		{
			sfprintf (stderr, "Setting ASHCAN flags on qualifying players\n");
			set_ash = true;
		}
		else
		if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--clear"))
		{
			sfprintf(stderr, "Clearing all AshCan flags on players.");
			save_all = true;
		}
		else
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug"))
		{
			sfprintf (stderr, "Debugging Messages Enabled\n");
			debug = true;
		}
		else
		if (!strcmp(argv[i], "-dump"))
		{
			sfprintf (stderr, "Dumping Database to stdout\n");
			dump = true;
		}
		else
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--file"))
		{
			if (argv[i+1])
			{
				sfprintf (stderr, "Using file '%s'\n", argv[i+1]);
				dbfile = fopen (argv[i+1], "r");
				if (dbfile == NULL)
				{
					sfprintf (stderr, "Could not open '%s' for reading\n", argv[i+1]);
					exit (errno);
				}
				i++;
			}
			else
			{
				fprintf (stderr, "Missing argument for %s switch\n", argv[i]);
				exit (-1);
			}
		}
		else
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
		{
			usage();
			exit (0);
		}
		else
		if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list"))
		{
			if (argv[i+1])
			{
				sfprintf (stderr, "Creating Trash-List as '%s'\n", argv[i+1]);
				trash_list = fopen (argv[i+1], "w");
				if (trash_list == NULL)
				{
					sfprintf (stderr, "Could not open '%s' for writing\n", argv[i+1]);
					exit (errno);
				}
				i++;
			}
			else
			{
				fprintf (stderr, "Missing argument for %s switch\n", argv[i]);
				exit (-1);
			}
		}
		else
		if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--silent"))
		{
			silent = true;
		}
		else
		{
			fprintf(stderr, "Unrecognised option '%s'\n", argv[i]);
			usage();
			exit (-1);
		}
	}

	if (dump && !set_ash && !save_all)
	{
		fprintf (stderr, "Setting '-dump' is pointless without '-Ash' or '-c'!\n");
		exit (-1);
	}

	if (save_all && !dump)
	{
		fprintf (stderr, "Save all is pointless if you don't use '-dump'!\n");
	}

	if (dbfile == stdin)
	{
		sfprintf(stderr, "%s: Starting db.read() from stdin.\n", APPNAME);
	}


	if (db.read(dbfile) < 0)
	{
		fprintf(stderr, "%s error: Cannot read DB\n",	APPNAME);
		exit(1);
	}

	dprint_helpful_message("db.read() completed. Beginning sensor sweep.");

	for (i = 0 ; i < db.get_top(); i++)
	{
		if (Typeof(i) == TYPE_PLAYER)
		{
			if (!save_all)
				check_player(i);
			else
			{
				if (Ashcan(i))
				{
					sfprintf (stderr, "[#%-5d] Player is set Ashcan. Flag removed.\n", i);
					db[i].clear_flag(FLAG_ASHCAN);
				}
			}
		}
	}

	
	dprint_helpful_message("Player Sweep complete.");

	if (dump)
	{
		db.write(stdout);
		fprintf(stderr, "\n");
	}

	if (trash_list != NULL)
		fclose (trash_list);
	
	return 0;
}

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
#include "version.h"
#include "os.h"
#include <sys/time.h>
#include <time.h>
#include "mudstring.h"
#include "log.h"

#define APPNAME				"Trawl"

// Make game.c happy
//const char* version = "trawl; unknown version.";
//const char* release = "UNKNOWN";

//int dump_interval = 10000000;


#if NEEDS_GETOPT
#	include "getopt.h"
#endif /* NEEDS_GETOPT */
#if NEEDS_RESOURCES
#	include <sys/resource.h>
#endif /* NEEDS_RESOURCES */

/* SunOS doesn't define these anywhere useful, apparently. */
#ifdef	__sun
extern "C"	int	getrlimit	(int, struct rlimit *);
//extern "C"	int	setrlimit	(int, struct rlimit *);
#endif	/* __sun */


int	zero_drogna=0;
int	list_drogna=0;
int db_write_out=1;

	int	dump_interval=DUMP_INTERVAL;
const	char*	fakeversion = "@(#)" VERSION;
const	char	*version=fakeversion + 4;
const	char*	release=RELEASE;
void print_helpful_message(const char *msg);
void zeroDrogna();
void listDrogna();

int
main (
int	argc,
char	**argv)

{
	int c;
	int errflg = 0;

#ifdef sun
	/* Tell the operating system that we access pages randomly */
	/* vadvise(VA_ANOM); */
#endif


	print_helpful_message("db.read() completed.");

	fprintf(stderr, "%s\n", version);

	while ((c = getopt(argc, argv, "dfcshBCHPRWA:i:")) != -1)
	{
		switch (c)
		{
			case 'z':
				zero_drogna=1;
				break;
			case 'd':
				list_drogna=1;
				break;
			case 'h':
			default:
				errflg++;
				break;
		}
	}
	
	if (errflg)
	{
           Trace( "usage:  trawl [-dz]\n\n");
           Trace( "        -d: Provide a sorted list of users with more than zero drogna. \n");
           Trace( "        -z: Zeros all the drogna. \n");
           Trace( "h gives this help.\n\n");
           exit (2);
        }
	init_strings ();

	Trace("%s diagnostic: Starting db.read() from stdin.\n",
				APPNAME);

	if (db.read(stdin) < 0)
	{
		Trace("%s error: Cannot read DB\n",
				APPNAME);
		exit(1);
	}

	if(zero_drogna)
	{
		zeroDrogna();
	}

	if(list_drogna)
	{
		listDrogna();
	}

	if(db_write_out)
	{
		db.write(stdout);
	}
}

void print_helpful_message(const char *msg)
{
	Trace("%s: %s\n",
			APPNAME, msg);
}

void zeroDrogna()
{

	int		i;
	int		drogna=0;

	print_helpful_message("zeroing Drogna");

	for (i = 0 ; i < db.get_top(); i++)
	{
		if ((Typeof(i) == TYPE_PLAYER) || (Typeof(i) == TYPE_PUPPET))
		{
			drogna = drogna + db[i].get_money();
			db[i].set_money(0);
		}
	}
	Trace("%s: Total Drogna removed:%d\n",APPNAME, drogna);

}

void listDrogna()
{
	db_write_out=0;

	int		i;

	int drogna=0;
	int players=0;

	print_helpful_message("listing Drogna");

	for (i = 0 ; i < db.get_top(); i++)
	{
		if ((Typeof(i) == TYPE_PLAYER) || (Typeof(i) == TYPE_PUPPET))
		{
			players++;
		}
	}

	int drogs[players];
	int ids[players];
	int player=0;

	for (i = 0 ; i < db.get_top(); i++)
	{
		if ((Typeof(i) == TYPE_PLAYER) || (Typeof(i) == TYPE_PUPPET))
		{
			ids[player] = i;
			drogs[player] = db[i].get_money();
			player++;
		}
	}

	for (i = 0 ; i < players; i++)
	{
		for (int j = i+1 ; j < players; j++)
		{
			if (drogs[i] < drogs[j])
			{
				int hold_drogs=drogs[i];
				int hold_ids=ids[i];
				drogs[i]=drogs[j];
				ids[i]=ids[j];
				drogs[j]=hold_drogs;
				ids[j]=hold_ids;
			}
		}
	}

	for (i = 0 ; i < players; i++)
	{
		if(drogs[i] > 0)
		{
			printf("%d:%s(#%d)\n", drogs[i], db[ids[i]].get_name().c_str(), ids[i]);
		}
	}


}

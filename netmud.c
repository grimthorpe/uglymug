/* static char SCCSid[] = "@(#)netmud.c	1.13\t7/19/94"; */
/*
 *	@(#)netmud.c 2.4 91/01/22
 */

#include "copyright.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>

#include "mudstring.h"
#include "db.h"
#include "interface.h"
#include "game.h"
#include "config.h"
#include "externs.h"

/* Code using this has been commented out anyway */ 
/* From sys/resource.h, gcc doesn't like it */
/*#define  PRIO_PROCESS  0  */

/* SunOS doesn't define these anywhere useful, apparently. */
#ifdef	__sun
extern "C"	int	getrlimit	(int, struct rlimit *);
//extern "C"	int	setrlimit	(int, struct rlimit *);
#endif	/* __sun */


extern	int	bp_check;
extern	int	chown_check;
extern	int	hack_check;
extern	int	room_check;
extern	int	wizard_check;
extern	int	puppet_check;
extern	int	fix_things;

	int	dump_interval=DUMP_INTERVAL;
const	char*	fakeversion = "@(#)" VERSION;
const	char	*version=fakeversion + 4;
const	char*	release=RELEASE;

String NULLSTRING;
CString NULLCSTRING;

int
main (
int	argc,
char	**argv)

{
	int c;
	int errflg = 0;
	extern char *optarg;
	extern int optind;

#ifdef sun
	/* Tell the operating system that we access pages randomly */
	/* vadvise(VA_ANOM); */
#endif

	fprintf(stderr, "%s\n", version);

	while ((c = getopt(argc, argv, "dfcshBCHPRWA:i:")) != -1)
	{
		switch (c)
		{
			case 'c':
				sanity_check_db = 1;
				break;
			case 'i':
				sscanf(optarg, "%d", &dump_interval);
				if(dump_interval<1)
				{
					Trace( "Dump interval must be at least 1 minute - setting to %d minutes.\n", DUMP_INTERVAL/60);
					dump_interval=DUMP_INTERVAL*60;
				}
				break;
			case 'f':
				fix_things = 1;
				break;
			case 's':
				sanity_only = 1;
				break;
			case 'A':
				sscanf(optarg,"%d",&bp_check);
				break;
			case 'B':
				bp_check = MAX_BUILDING_POINTS;
				break;
			case 'C':
				chown_check = 1;
				break;
			case 'H':
				hack_check = 1;
				break;
			case 'P':
				puppet_check = 1;
				break;
			case 'R':
				room_check = 1;
				break;
			case 'W':
				wizard_check = 1;
				break;
			case 'h':
			default:
				errflg++;
				break;
		}
	}
	
	if (errflg)
	{
		Trace( "usage:  netmud [-fcishABCHPRW]\n\n
	        h gives this help.

		c does a sanity-check. Game run.
		i <value> sets the dump interval to <value> minutes.
		s does a sanity-check only. Game not run.
		f runs sanity-check and attempts to fix any db errors.

		A <value> checks for players with <value> bp's.
		B checks for rich players
		C checks for chown abnormalities
		H checks for hacks.
		P checks puppet rules.
		R checks for room/area rules
		W checks for wizard set items\n\n"); 
		exit (2);
	}
#ifndef linux
	struct	rlimit	lim;
	/* Ignore command line nice level & use MUD_NICE_LEVEL */
	/* nice (MUD_NICE_LEVEL - getpriority (PRIO_PROCESS, 0)); */

	getrlimit(RLIMIT_NOFILE,&lim);
	lim.rlim_cur=MAX_USERS+4;
	if(setrlimit(RLIMIT_NOFILE,&lim)<0)
		Trace( "BUG: setrlimit() failed\n");
#endif

	srand (time(NULL));
	game_start_time = time(NULL);
	if (argc-optind<2)
	{
		Trace( "Usage: %s infile dumpfile [port]\n", *argv);
		exit (1);
	}
	init_strings ();
	if (init_game (argv[optind], argv[optind + 1]) < 0)
	{
		Trace( "Couldn't load %s!\n", argv[optind]);
		exit (2);
	}
	set_signals ();
	mud_main_loop (argc-optind-2, argv+optind+2);
	close_sockets ();
	dump_database ();
	exit (0);
}

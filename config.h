// vi:ts=4:sw=4:ai:
#include "copyright.h"
#include "command.h"
#ifndef _Config_h_
#define _Config_h_

/*
 * Things that used to be in the makefile, but aren't any more, because
 * they're in here now, so as to take them out of the makefile, because
 * that was too big. So now they're here, so this is bigger, but at
 * least the makefile is smaller.
 */

#define	STARTUP					/* Look for .startup and .shutdown in #4 */
#define	LOG_FAILED_COMMANDS		/* Log all instances of failed commands */
#define	RESTRICTED_BUILDING		/* Only BUILDERs may... well, build */
#define	HACK_HUNTING			/* Log use of wizard @chpids not set W */
#define	LOG_RECURSION			/* Log recursion overruns */
#define	QUIET_WHISPER			/* Other people's whispers are not shown */
#define	ALIASES					/* Are aliases operational? */
#define	ABORT_FUSES				/* Abort fuses are allowed */
#undef	DARK_PLAYERS			/* Dark players are allowed */

#undef	LOG_COMMANDS			/* Log _all_ commands. _BIG_ output */
#undef	LOG_COMMAND_LINES		/* Log only top-level commands - Not quite so big */
#undef	CONCENTRATOR			/* If the concentrator is to be used */
#undef	LOG_NAME_CHANGES		/* Log all player name changes */
#define	LOG_WALLS				/* Log all usage of @wall */
#undef	LOCAL_CONNECTIONS_ONLY	/* For use with the concentrator */

#undef	DEBUG					/* For debugging */
#undef	MATCH_DEBUG				/* Match debug lines */

#define BUFFER_LEN      ((MAX_COMMAND_LEN)*4)

/* Name change time in seconds */
#define NAME_TIME		300

/* Ability maxima */
#define	MAX_STRENGTH		2500
#define	MAX_CONSTITUTION	2500
#define	MAX_DEXTERITY		2500
#define	MAX_PERCEPTION		2500
#define	MAX_INTELLIGENCE	2500
#define	MAX_ARMOURCLASS		10
#define	MIN_ARMOURCLASS		0
#define	MAX_LEVEL		30
#define	XP_PER_LEVEL		2000

#define STANDARD_ROOM_MASS		0
#define STANDARD_ROOM_VOLUME		0
#define STANDARD_ROOM_MASS_LIMIT	HUGE_VAL
#define	STANDARD_ROOM_VOLUME_LIMIT	HUGE_VAL
#define STANDARD_ROOM_GRAVITY		0
#define STANDARD_PLAYER_MASS		70
#define STANDARD_PLAYER_VOLUME		75
#define STANDARD_PLAYER_MASS_LIMIT	15
#define STANDARD_PLAYER_VOLUME_LIMIT	400
#define STANDARD_PLAYER_GRAVITY		1
#define STANDARD_THING_MASS		0	/* Non-zero so that players can't have an easy life	*/
#define STANDARD_THING_VOLUME		0	/* with mass and volume - Lee				*/
#define STANDARD_THING_MASS_LIMIT	0
#define	STANDARD_THING_VOLUME_LIMIT	0
#define	STANDARD_THING_GRAVITY		1

#define NUM_INHERIT			(-HUGE_VAL)	/* if we come across this number it means inherit from parent */

#define LIMBO			0
#define	GOD_ID			1
#define	PLAYER_START		3
#define	COMMAND_LAST_RESORT	4

/* minimum cost to create various things */
#define ALARM_COST		100
#define ARRAY_COST		10
#define	COMMAND_COST		10
#define DICTIONARY_COST		10
#define	EXIT_COST		1
#define FUSE_COST		10
#define	LINK_COST		1
#define PROPERTY_COST		1
#define	PUPPET_COST		200
#define	ROOM_COST		20
#define	THING_COST		10
#define VARIABLE_COST		3

/* cost to do a scan */
#define	LOOKUP_COST		0
#define	FIND_COST		0
#define	PAGE_COST		0
#define ALARM_EXECUTE_COST	0
#define FUSE_EXECUTE_COST	0

/* magic cookies */
#define	NOT_TOKEN		'!'
#define	LOOKUP_TOKEN		'*'
#define	NUMBER_TOKEN		'#'
#define	AND_TOKEN		'&'
#define	OR_TOKEN		'|'

/* magic command cookies */
#define	SAY_TOKEN		'"'
#define	ALT_SAY_TOKEN		'\''
#define	POSE_TOKEN		':'
#define NOTIFY_TOKEN		';'
#define COMMAND_TOKEN		'@'
#define	EXIT_DELIMITER		';'
#define TELL_TOKEN		'>'
#define TELL_EMOTE_TOKEN	'<'

#define	MAX_BUILDING_POINTS	5000

/* timing stuff */
#define	DUMP_INTERVAL		2400	/* seconds between dumps (may be overridden on command line) */
#define	COMMAND_TIME_MSEC	250	/* time slice length in milliseconds */
#define	COMMAND_BURST_SIZE	2	/* commands per user in a burst */
#define	COMMANDS_PER_TIME	1	/* commands/time slice after burst */

/* Maximum number of compound commands executed in response to one user-typed command */
#define COMPOUND_COMMAND_BASE_LIMIT	100
#define	COMPOUND_COMMAND_MAXIMUM_LIMIT	1500

/* maximum amount of queued output */
#define	MAX_OUTPUT 16384

/* Absolute nice level at which to run mud */
#define	MUD_NICE_LEVEL		0

/* Maximum number of users permitted */
#define	MAX_USERS		120

#define	TINYPORT 6239
#define	WELCOME_MESSAGE "\n\tWelcome to UglyMUG!\nTo connect to your existing character, use \"connect name password\".\nTo create a new character, use \"create name password\".\n\nDisconnect with \"QUIT\", which must be capitalized as shown.\n\n\"WHO\" finds out who is currently active.\n\n"

#define	LEAVE_MESSAGE	"\nBye - see you soon!\n"

#define	CONC_PORT	16309
#define	CONC_MACHINE	0x82580dbe	/* pasteur.cs.man.ac.uk */

#define	QUIT_COMMAND	"QUIT"
#define	WHO_COMMAND	"WHO"
#define	INFO_COMMAND	"INFO"
#define IDLE_COMMAND	"IDLE"
#define	PREFIX_COMMAND	"OUTPUTPREFIX"
#define	SUFFIX_COMMAND	"OUTPUTSUFFIX"
#define	HALFQUIT_COMMAND	"HALFQUIT"

/* The amount of time that the IDLE command increases your idle to in seconds */
#define IDLE_TIME_INCREASE 180

#define	WELCOME_FILE	"motd.txt"
#define SMD_FILE	"smd.list"
#define EMAIL_FILE	"email.list"

const unsigned int MAX_MORTAL_ARRAY_ELEMENTS=100;
const unsigned int MAX_WIZARD_ARRAY_ELEMENTS=750;

#define MAX_MORTAL_DICTIONARY_ELEMENTS 100
/* Increased to make help work as everyone is too lazy to fix it. */
#define MAX_WIZARD_DICTIONARY_ELEMENTS 750

#define MAX_MORTAL_DESC_SIZE 2048
#define MAX_WIZARD_DESC_SIZE 2048

#define MAX_NAME_LENGTH		20
#define MAX_ALIASES		5
#define MIN_ALIAS_LENGTH	3
#define MAX_ALIAS_LENGTH	MAX_NAME_LENGTH

#define MAX_WHO_STRING		80

#define DEFAULT_RECALL_LINES	50
#define MAX_RECALL_LINES	400

#define CURRENCY_NAME		"Drogna"

/* The termcap compat libraries on linux are becoming increasingly unsupported.
 * I recommend using this so you don't have to link with two versions of libc
 * which seems rather dodgey.  -Abs
 */
#define	USE_TERMINFO 

/* Chat channels.*/

#define CHANNEL_MAGIC_COOKIE		'#'
#define CHANNEL_INVITE_FREQUENCY	60

#endif


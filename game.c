/* static char SCCSid[] = "@(#)game.c 1.120 99/02/11@(#)"; */
#include "copyright.h"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include "db.h"
#include "config.h"
#include "game.h"
#include "match.h"
#include "externs.h"
#include "colour.h"
#include "command.h"
#include "context.h"
#include "interface.h"
#include "log.h"

/* Bits'n'pieces that SunOS appears not to define in any of its header files. PJC 15/4/96. 
#ifdef	__sun
extern	int	nice	(int);
extern	int	wait3	(union wait *, int, struct rusage *);
extern	void	srand48	(long);
#endif
*/

#define	ALARM_RECURSION_LIMIT	5
#define	MAX_DOLLAR_COUNT	50

/* PREFIX_INDICATOR must be greater than the difference between any two character values in the system */
#define	PREFIX_INDICATOR	0x100


void				execute_startup(void);
void				execute_shutdown(void);
static	void			fork_and_dump (void);
void				dump_database (void);


/* JPK static	const	char		*dumpfile	= NULL; */
static	char		*dumpfile	= NULL;
static	int			epoch		= 0;
static	int			alarm_triggered	= 0;
static  time_t			dump_start_time = 0;
static	int			alarm_block	= 0;
	int			sanity_check_db	= 0;
	int			sanity_only	= 0;
	int			abort_from_fuse;

const	char			*shutdown_reason;


enum	command_flags
{
	NO_COMMAND_FLAGS	= 0x0,
	LEGAL_COMMAND		= 0x01,
	FULL_COMPARE		= 0x02,
	COMMAND_LINE_ONLY	= 0x04
};


class	command_details
{
    public:
	const	char		*name;
	void			(context::*context_address) (const CString&, const CString&);
	command_flags		flags;
};


command_details			command_table [] =
{
	/* Command, function, arg-type, legal */
	/* @?-commands */
	{"<",			&context::do_tellemote,			NO_COMMAND_FLAGS},
	{">",			&context::do_tell,			NO_COMMAND_FLAGS},
	{"@?address",		&context::do_query_address,		LEGAL_COMMAND},
#ifdef ALIASES
	{"@?aliases",		&context::do_query_aliases,		LEGAL_COMMAND},
#endif /* ALIASES */
	{"@?area",		&context::do_query_area,		LEGAL_COMMAND},
	{"@?arrays",		&context::do_query_arrays,		LEGAL_COMMAND},
	{"@?article",		&context::do_query_article,		LEGAL_COMMAND},
	{"@?bps",		&context::do_query_bps,			LEGAL_COMMAND},
	{"@?can",		&context::do_query_can,			LEGAL_COMMAND},
	{"@?cfail",		&context::do_query_cfail,		LEGAL_COMMAND},
	{"@?channel",		&context::do_query_channel,		LEGAL_COMMAND},
	{"@?colour",		&context::do_query_colour,		LEGAL_COMMAND},
	{"@?commands",		&context::do_query_commands,		LEGAL_COMMAND},
	{"@?connected",		&context::do_query_connected,		LEGAL_COMMAND},
	{"@?contents",		&context::do_query_contents,		LEGAL_COMMAND},
	{"@?controller",	&context::do_query_controller,		LEGAL_COMMAND},
	{"@?cstring",		&context::do_query_cstring,		LEGAL_COMMAND},
	{"@?csucc",		&context::do_query_csucc,		LEGAL_COMMAND},
	{"@?desc",		&context::do_query_description,		LEGAL_COMMAND},
	{"@?descendantfrom",	&context::do_query_descendantfrom,	LEGAL_COMMAND},
	{"@?description",	&context::do_query_description,		LEGAL_COMMAND},
	{"@?destination",	&context::do_query_destination,		LEGAL_COMMAND},
	{"@?dictionaries",	&context::do_query_dictionaries,	LEGAL_COMMAND},
	{"@?drop",		&context::do_query_drop,		LEGAL_COMMAND},
	{"@?elements",		&context::do_query_elements,		LEGAL_COMMAND},
	{"@?email",		&context::do_query_email,		LEGAL_COMMAND},
	{"@?exist",		&context::do_query_exist,		LEGAL_COMMAND},
	{"@?exits",		&context::do_query_exits,		LEGAL_COMMAND},
	{"@?fail",		&context::do_query_fail,		LEGAL_COMMAND},
	{"@?firstname",		&context::do_query_first_name,		LEGAL_COMMAND},
	{"@?fuses",		&context::do_query_fuses,		LEGAL_COMMAND},
	{"@?gravityfactor",	&context::do_query_gravity_factor,	LEGAL_COMMAND},
	{"@?id",		&context::do_query_id,			LEGAL_COMMAND},
	{"@?idletime",		&context::do_query_idletime,		LEGAL_COMMAND},
	{"@?interval",		&context::do_query_interval,		LEGAL_COMMAND},
	{"@?key",		&context::do_query_key,			LEGAL_COMMAND},
	{"@?lastentry",		&context::do_query_last_entry,		LEGAL_COMMAND},
	{"@?location",		&context::do_query_location,		LEGAL_COMMAND},
	{"@?lock",		&context::do_query_lock,		LEGAL_COMMAND},
	{"@?mass",		&context::do_query_mass,		LEGAL_COMMAND},
	{"@?masslimit",		&context::do_query_mass_limit,		LEGAL_COMMAND},
	{"@?money",		&context::do_query_money,		LEGAL_COMMAND},
	{"@?my",		&context::do_query_my,			LEGAL_COMMAND},
	{"@?myself",		&context::do_query_myself,		LEGAL_COMMAND},
	{"@?name",		&context::do_query_name,		LEGAL_COMMAND},
	{"@?next",		&context::do_query_next,		LEGAL_COMMAND},
	{"@?numconnected",	&context::do_query_numconnected,	LEGAL_COMMAND},
	{"@?odrop",		&context::do_query_odrop,		LEGAL_COMMAND},
	{"@?ofail",		&context::do_query_ofail,		LEGAL_COMMAND},
	{"@?osuccess",		&context::do_query_osuccess,		LEGAL_COMMAND},
	{"@?owner",		&context::do_query_owner,		LEGAL_COMMAND},
	{"@?parent",		&context::do_query_parent,		LEGAL_COMMAND},
	{"@?pending",		&context::do_query_pending,		LEGAL_COMMAND},
	{"@?properties",	&context::do_query_properties,		LEGAL_COMMAND},
	{"@?race",		&context::do_query_race,		LEGAL_COMMAND},
	{"@?rand",		&context::do_query_rand,		LEGAL_COMMAND},
	{"@?realtime",		&context::do_query_realtime,		LEGAL_COMMAND},
	{"@?score",		&context::do_query_score,		LEGAL_COMMAND},
	{"@?set",		&context::do_query_set,			LEGAL_COMMAND},
	{"@?size",		&context::do_query_size,		LEGAL_COMMAND},
	{"@?success",		&context::do_query_success,		LEGAL_COMMAND},
	{"@?terminal",		&context::do_query_terminal,		LEGAL_COMMAND},
	{"@?time",		&context::do_query_time,		LEGAL_COMMAND},
	{"@?typeof",		&context::do_query_typeof,		LEGAL_COMMAND},
	{"@?variables",		&context::do_query_variables,		LEGAL_COMMAND},
	{"@?version",		&context::do_query_version,		LEGAL_COMMAND},
	{"@?volume",		&context::do_query_volume,		LEGAL_COMMAND},
	{"@?volumelimit",	&context::do_query_volume_limit,	LEGAL_COMMAND},
	{"@?weight",		&context::do_query_weight,		LEGAL_COMMAND},
	{"@?who",		&context::do_query_who,			LEGAL_COMMAND},
	/* @-commands */
	{"@alarm",		&context::do_alarm,		NO_COMMAND_FLAGS},
#ifdef ALIASES
	{"@alias",		&context::do_alias,		NO_COMMAND_FLAGS},
#endif /* ALIASES */
	{"@areanotify",		&context::do_at_areanotify,	NO_COMMAND_FLAGS},
	{"@array",		&context::do_array,		NO_COMMAND_FLAGS},
	{"@beep",		&context::do_beep,		LEGAL_COMMAND},
	{"@boot",		&context::do_boot,		NO_COMMAND_FLAGS},
	{"@censor",		&context::do_at_censor,		NO_COMMAND_FLAGS},
	{"@cfailure",		&context::do_cfailure,		NO_COMMAND_FLAGS},
	{"@channel",		&context::do_channel,		NO_COMMAND_FLAGS},
	{"@chpid",		&context::do_chpid,		LEGAL_COMMAND},
	{"@color",		&context::do_colour,		NO_COMMAND_FLAGS},
	{"@colour",		&context::do_colour,		NO_COMMAND_FLAGS},
	{"@command",		&context::do_at_command,	NO_COMMAND_FLAGS},
	{"@connect",		&context::do_at_connect,	NO_COMMAND_FLAGS},
	{"@controller",		&context::do_controller,	NO_COMMAND_FLAGS},
	{"@create",		&context::do_create,		NO_COMMAND_FLAGS},
	{"@credit",		&context::do_credit,		NO_COMMAND_FLAGS},
	{"@cstring",		&context::do_cstring,		NO_COMMAND_FLAGS},
	{"@csuccess",		&context::do_csuccess,		NO_COMMAND_FLAGS},
	{"@debit",		&context::do_debit, 		NO_COMMAND_FLAGS},
	{"@decompile",		&context::do_decompile,		NO_COMMAND_FLAGS},
	{"@describe",		&context::do_describe,		NO_COMMAND_FLAGS},
	{"@destroy",		&context::do_destroy,		NO_COMMAND_FLAGS},
	{"@dictionary",		&context::do_dictionary,	NO_COMMAND_FLAGS},
	{"@dig",		&context::do_dig,		NO_COMMAND_FLAGS},
	{"@disconnect",		&context::do_at_disconnect,	NO_COMMAND_FLAGS},
	{"@drop",		&context::do_at_drop,		NO_COMMAND_FLAGS},
	{"@dump",		&context::do_dump,		NO_COMMAND_FLAGS},
	{"@echo",		&context::do_echo,		LEGAL_COMMAND},
	{"@else",		&context::do_at_else,		NO_COMMAND_FLAGS},
	{"@elseif",		&context::do_at_elseif,		NO_COMMAND_FLAGS},
	{"@email",		&context::do_email,		NO_COMMAND_FLAGS},
	{"@empty",		&context::do_empty,		NO_COMMAND_FLAGS},
	{"@end",		&context::do_at_end,		NO_COMMAND_FLAGS},
	{"@endif",		&context::do_at_endif,		NO_COMMAND_FLAGS},
	{"@evaluate",		&context::do_full_evaluate,	LEGAL_COMMAND},
#if 0	/* PJC 24/1/97 */
	{"@event",		&context::do_event,		NO_COMMAND_FLAGS},
#endif
	{"@exclude",		&context::do_at_exclude,	NO_COMMAND_FLAGS},
	{"@fail",		&context::do_fail,		NO_COMMAND_FLAGS},
	{"@false",		&context::do_at_false,		LEGAL_COMMAND},
	{"@find",		&context::do_find,		NO_COMMAND_FLAGS},
	{"@for",		&context::do_at_for,		NO_COMMAND_FLAGS},
	{"@force",		&context::do_force,		NO_COMMAND_FLAGS},
	{"@from",		&context::do_from,		NO_COMMAND_FLAGS},
	{"@fuse",		&context::do_fuse,		NO_COMMAND_FLAGS},
	{"@gc",			&context::do_garbage_collect,	NO_COMMAND_FLAGS},
	{"@global",		&context::do_at_global,		NO_COMMAND_FLAGS},
	{"@gravityfactor",	&context::do_gravity_factor,	NO_COMMAND_FLAGS},
	{"@group",		&context::do_group,		NO_COMMAND_FLAGS},
	{"@if",			&context::do_at_if,		NO_COMMAND_FLAGS},
	{"@insert",		&context::do_at_insert,		NO_COMMAND_FLAGS},
	{"@key",		&context::do_at_key,		NO_COMMAND_FLAGS},
	{"@link",		&context::do_link,		NO_COMMAND_FLAGS},
	{"@list",		&context::do_at_list,		NO_COMMAND_FLAGS},
#ifdef ALIASES
	{"@listaliases",	&context::do_listaliases,	NO_COMMAND_FLAGS},
#endif /* ALIASES */
	{"@listcensor",		&context::do_at_censorinfo,	NO_COMMAND_FLAGS},
	{"@listcolours",	&context::do_at_listcolours,	NO_COMMAND_FLAGS},
	{"@local",		&context::do_at_local,		NO_COMMAND_FLAGS},
	{"@location",		&context::do_location,		NO_COMMAND_FLAGS},
	{"@lock",		&context::do_at_lock,		NO_COMMAND_FLAGS},
	{"@mass",		&context::do_mass,		NO_COMMAND_FLAGS},
	{"@masslimit",		&context::do_mass_limit,	NO_COMMAND_FLAGS},
	{"@motd",		&context::do_at_motd,		NO_COMMAND_FLAGS},
	{"@name",		&context::do_name,		NO_COMMAND_FLAGS},
	{"@natter",		&context::do_natter,		NO_COMMAND_FLAGS},
	{"@newpassword",	&context::do_newpassword,	NO_COMMAND_FLAGS},
	{"@note",		&context::do_at_note,		NO_COMMAND_FLAGS},
	{"@notify",		&context::do_at_notify,		NO_COMMAND_FLAGS},
	{"@npc",		&context::do_puppet,		NO_COMMAND_FLAGS},
	{"@odrop",		&context::do_odrop,		NO_COMMAND_FLAGS},
	{"@oecho",		&context::do_at_oecho,		NO_COMMAND_FLAGS},
	{"@oemote",		&context::do_at_oemote,		NO_COMMAND_FLAGS},
	{"@ofail",		&context::do_ofail,		NO_COMMAND_FLAGS},
	{"@open",		&context::do_at_open,		NO_COMMAND_FLAGS},
	{"@osuccess",		&context::do_osuccess,		NO_COMMAND_FLAGS},
	{"@owner",		&context::do_owner,		NO_COMMAND_FLAGS},
	{"@parent",		&context::do_parent,		NO_COMMAND_FLAGS},
	{"@password",		&context::do_password,		NO_COMMAND_FLAGS},
	{"@pcreate",		&context::do_pcreate,		NO_COMMAND_FLAGS},
	{"@peak",		&context::do_peak,		NO_COMMAND_FLAGS},
	{"@pemote",		&context::do_pemote,		NO_COMMAND_FLAGS},
	{"@pose",		&context::do_pose,		NO_COMMAND_FLAGS},
	{"@property",		&context::do_property,		NO_COMMAND_FLAGS},
	{"@puppet",		&context::do_puppet,		NO_COMMAND_FLAGS},
	{"@queue",		&context::do_at_queue,		FULL_COMPARE},
	{"@race",		&context::do_race,		NO_COMMAND_FLAGS},
	{"@recall",		&context::do_recall,		NO_COMMAND_FLAGS},
	{"@recursionlimit",	&context::do_at_recursionlimit, NO_COMMAND_FLAGS},
	{"@rem",		&context::do_at_rem,		FULL_COMPARE},
	{"@remote",		&context::do_remote,		NO_COMMAND_FLAGS},
	{"@return",		&context::do_return,		NO_COMMAND_FLAGS},
	{"@returnchain",	&context::do_returnchain,	NO_COMMAND_FLAGS},
	{"@score",		&context::do_at_score,		NO_COMMAND_FLAGS},
	{"@set",		&context::do_set,		NO_COMMAND_FLAGS},
	{"@shutdown",		&context::do_shutdown,		FULL_COMPARE},
	{"@smd",		&context::do_smd,		NO_COMMAND_FLAGS},
	{"@sort",		&context::do_at_sort,		NO_COMMAND_FLAGS},
	{"@stats",		&context::do_stats,		NO_COMMAND_FLAGS},
	{"@success",		&context::do_success,		NO_COMMAND_FLAGS},
	{"@teleport",		&context::do_location,		LEGAL_COMMAND},
	{"@terminal",		&context::do_terminal_set,	NO_COMMAND_FLAGS},
	{"@test",		&context::do_test,		LEGAL_COMMAND},
	{"@true",		&context::do_at_true,		LEGAL_COMMAND},
	{"@truncate",		&context::do_truncate,		LEGAL_COMMAND},
#ifdef ALIASES
	{"@unalias",		&context::do_unalias,		NO_COMMAND_FLAGS},
#endif /* ALIASES */
	{"@uncensor",		&context::do_at_uncensor,	LEGAL_COMMAND},
	{"@unchpid",		&context::do_unchpid,		LEGAL_COMMAND},
	{"@unexclude",		&context::do_at_unexclude,	LEGAL_COMMAND},
	{"@unlink",		&context::do_unlink,		NO_COMMAND_FLAGS},
	{"@unlock",		&context::do_at_unlock,		NO_COMMAND_FLAGS},
	{"@variable",		&context::do_variable,		NO_COMMAND_FLAGS},
	{"@version",		&context::do_version,		NO_COMMAND_FLAGS},
	{"@volume",		&context::do_volume,		NO_COMMAND_FLAGS},
	{"@volumelimit",	&context::do_volume_limit,	NO_COMMAND_FLAGS},
	{"@wall",		&context::do_wall,		FULL_COMPARE},
	{"@warn",		&context::do_at_warn,		FULL_COMPARE},
	{"@welcomer",		&context::do_welcome,		NO_COMMAND_FLAGS},
	{"@who",		&context::do_at_who,		NO_COMMAND_FLAGS},
	{"@with",		&context::do_at_with,		NO_COMMAND_FLAGS},
	/* Basic commands */
	{"chat",		&context::do_chat,		NO_COMMAND_FLAGS},
	{"close",		&context::do_close,		NO_COMMAND_FLAGS},
	{"cwho",		&context::do_channel_who,	NO_COMMAND_FLAGS},
	{"drop",		&context::do_drop,		NO_COMMAND_FLAGS},
	{"emote",		&context::do_pose,		NO_COMMAND_FLAGS},
	{"enter",		&context::do_enter,		NO_COMMAND_FLAGS},
	{"examine",		&context::do_examine,		NO_COMMAND_FLAGS},
	{"fchat",		&context::do_fchat,		NO_COMMAND_FLAGS},
	{"fwho",		&context::do_fwho,		NO_COMMAND_FLAGS},
	{"get",			&context::do_get,		NO_COMMAND_FLAGS},
	{"give",		&context::do_give,		NO_COMMAND_FLAGS},
	{"goto",		&context::do_move,		NO_COMMAND_FLAGS},
	{"gripe",		&context::do_gripe,		NO_COMMAND_FLAGS},
	{"inventory",		&context::do_inventory,		NO_COMMAND_FLAGS},
	{"l",			&context::do_look_at,		NO_COMMAND_FLAGS},
	{"ladd",		&context::do_ladd,		NO_COMMAND_FLAGS},
	{"leave",		&context::do_leave,		NO_COMMAND_FLAGS},
	{"llist",		&context::do_llist,		NO_COMMAND_FLAGS},
	{"lock",		&context::do_lock,		NO_COMMAND_FLAGS},
	{"look",		&context::do_look_at,		NO_COMMAND_FLAGS},
	{"lremove",		&context::do_lremove,		NO_COMMAND_FLAGS},
	{"lset",		&context::do_lset,		NO_COMMAND_FLAGS},
	{"move",		&context::do_move,		NO_COMMAND_FLAGS},
	{"notify",		&context::do_notify,		FULL_COMPARE},
	{"open",		&context::do_open,		NO_COMMAND_FLAGS},
	{"page",		&context::do_page,		NO_COMMAND_FLAGS},
	{"pose",		&context::do_pose,		NO_COMMAND_FLAGS},
	{"quit",		&context::do_quit,		FULL_COMPARE},
	{"read",		&context::do_look_at,		NO_COMMAND_FLAGS},
	{"say",			&context::do_say,		NO_COMMAND_FLAGS},
	{"score",		&context::do_score,		NO_COMMAND_FLAGS},
	{"set",			&context::do_terminal_set,	FULL_COMPARE},
	{"swho",		&context::do_swho,		NO_COMMAND_FLAGS},
	{"take",		&context::do_get,		NO_COMMAND_FLAGS},
	{"tell",		&context::do_tell,		NO_COMMAND_FLAGS},
	{"throw",		&context::do_drop,		NO_COMMAND_FLAGS},
	{"unlock",		&context::do_unlock,		NO_COMMAND_FLAGS},
	{"whisper",		&context::do_whisper,		NO_COMMAND_FLAGS},
	{"who",			&context::do_who,		FULL_COMPARE},
};


static int
cmdcmp (
const	char	*name,
command_details	&cmd)

{
	int	value = string_compare (name, cmd.name);

	if (cmd.flags & FULL_COMPARE)
		return (value);
	else
	{
		/* If it's an exact match, we win. If the key is greater than the value, it cannot be a prefix. */
		if (value >= 0)
			return (value);

		/* Only done in the case of a possible prefix */
		if (string_prefix (cmd.name, name))
			return (PREFIX_INDICATOR);

		/* It wasn't. Value should always be 1 here, but we'll make sure. */
		return (value);
	}
}


command_details *
find_basic_command (
const	char	*key)

{
	const	int	table_top = (sizeof (command_table) / sizeof (command_details)) - 1;
	int		bottom = 0;
	int		top = table_top;
	int		mid;
	int		value;

	while (bottom <= top)
	{
		mid = (top + bottom) / 2;
		value = cmdcmp (key, command_table [mid]);
		if (value < 0)
			top = mid - 1;
		else if (value == 0)
			return (command_table + mid);
		else if (value == PREFIX_INDICATOR)
		{
			/* Key is prefix of current, but may be ambiguous or may have proper prefix */

			/* Keep hunting backwards for the earliest true prefix. It can't be further back than low. */
			while (mid >= bottom && string_prefix (command_table [mid].name, key))
				mid--;
			mid++;

			/* Check for exact command. This is where we handle commands that are proper prefixes of others. */
			if (!string_compare (command_table [mid].name, key))
				return (command_table + mid);

			/* Check for ambiguity forwards. It can't be ambiguous backwards, 'cos we're at the earliest one. */
			if (mid < table_top)
			{
				if (string_prefix (command_table [mid + 1].name, key))
					return (NULL);
			}

			return (command_table + mid);
		}
		else /* value > 0 */
			bottom = mid + 1;
	}

	/* We get here if the command wasn't found */
	return (NULL);
}


void
context::do_version (
const	CString&,
const	CString&)

{
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);

	notify_colour (player, player, COLOUR_MESSAGES, version);
}

void
context::do_query_version (
const	CString&,
const	CString&)
{
	return_status = COMMAND_SUCC;
	set_return_string(release);
}


void
context::do_dump (
const	CString&,
const	CString&)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if (Wizard (player))
	{
		alarm_triggered = 1;
		notify_colour (player, player, COLOUR_MESSAGES, "Dumping...");
		return_status = COMMAND_SUCC;
		set_return_string(ok_return_string);
	}
	else
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
}


void
context::do_shutdown (
const	CString& arg1,
const	CString& arg2)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!Wizard (player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	if (in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No shutting down within a command please.");
		return;
	}

	execute_shutdown();
	shutdown_reason = alloc_string(reconstruct_message(arg1, arg2));
	if (!blank(shutdown_reason))
	{
		log_shutdown(player, getname(player), shutdown_reason);
	}
	else
	{
		log_shutdown(player, getname(player), "no apparent reason");
	}
	shutdown_flag = 2;
	shutdown_player = player;
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


static void
alarm_handler (
int			sig)

{
	alarm_triggered = 1;
#if 0
	if (!alarm_block)
		fork_and_dump ();
#endif

	signal(SIGALRM, alarm_handler);
}

static void
hup_handler (
int			sig)

{
/* Unconditional dump. Do it now, we don't care what state the game is in. */
	alarm_triggered = 1;
	fork_and_dump ();

	signal(SIGHUP, alarm_handler);
}


static void
dump_database_internal ()

{
	char	tmpfile [2048];
	FILE	*db_writefile;

	sprintf (tmpfile, "%s.#%d#", dumpfile, epoch - 1);
	unlink (tmpfile);	/* nuke our predecessor */

	sprintf (tmpfile, "%s.#%d#", dumpfile, epoch);

	if ((db_writefile = fopen (tmpfile, "w")) != NULL)
	{
		db.write (db_writefile);
		if(ferror(db_writefile))
		{
			notify_wizard("ERROR: DATABASE DIDN'T DUMP CLEANLY. Error %d\n",ferror(db_writefile));
			log_bug("ERROR: DATABASE DIDN'T DUMP CLEANLY. Error %d", ferror(db_writefile));
		}
		if(fclose (db_writefile))
		{
			notify_wizard("Database dump didn't close properly\n");
			log_bug("ERROR: Database dump #%d didn't close properly", epoch);
		}
		if (rename (tmpfile, dumpfile) < 0)
			perror(dumpfile);
	}
	else
		perror(tmpfile);
}


static void
double_panic (
int	sig)

{
	log_panic("DOUBLE PANIC: Got signal %d during panic dump!", sig);

	/* gi's a core */
	kill (getpid (), SIGQUIT);
	_exit(1);
}


void
panic (
const	char	*message)

{
	char	panicfile [2048];
	FILE	*f;
	int	i;
	int	exitcode;

	log_panic(message);

	/* turn off signals */
	signal(0, SIG_IGN);
	signal(1, SIG_IGN);
	signal(2, SIG_IGN);
	for (i = 4; i < NSIG; i++)
		signal(i, SIG_IGN);
	signal(SIGSTOP, double_panic);
	signal(SIGCONT, double_panic);
	signal(SIGTSTP, double_panic);

	/* dump panic file */
	sprintf (panicfile, "%s.PANIC", dumpfile);
	if ((f = fopen (panicfile, "w")) == NULL)
	{
		perror ("CANNOT OPEN PANIC FILE, DOES IT HURT");
		exitcode = 135;
	}
	else
	{
		log_dumping(false, panicfile);
		db.write (f);
		fclose (f);
		log_dumping(true, panicfile);
		exitcode = 136;
	}

	/** Interface shutdown moved here while debugging interface problems. PJC 20/2/95. **/
	/* shut down interface */
	emergency_shutdown ();

	/* Get a core dump */
	kill (getpid (), SIGQUIT);
	_exit (exitcode);
}


void
dump_database ()

{
	epoch++;
	log_dumping(false, "%s.#%d#", dumpfile, epoch);
	dump_database_internal ();
	log_dumping(true, "%s.#%d#", dumpfile, epoch);
}


static void
fork_and_dump ()

{
	int	child;

	epoch++;
	log_checkpointing(dumpfile, epoch);
	child = fork ();
	if(child == 0)
	{	/* in the child */
		nice(15);
		close(0);	/* get that file descriptor back */
		dump_database_internal();
		_exit(0);
	}
	else if(child < 0)
	{
		perror("fork_and_dump: fork()");
		notify_wizard("ERROR:  FORK FAILED FOR DATABASE DUMP!\n");
	}
	else
	{	/* in the parent */
		/* Give message */
		dump_start_time = time(NULL);
		/* reset alarm */
		alarm_triggered = 0;
		alarm (dump_interval);
	}
}


static void
reaper (
int			sig)

{
#ifndef sun
	union	wait	stat;
#else
	int *stat;
#endif

	while (wait3 ((int*)&stat, WNOHANG, NULL) > 0);
	alarm_triggered = -1;
	signal(SIGCHLD, reaper);	/* needed on some OS's */
}

void
execute_startups(void)
{
#ifdef STARTUP

	dbref item;

	DOLIST (item, db[COMMAND_LAST_RESORT].get_commands())
	{
		if (!string_compare(db[item].get_name(),".startup"))
			if (Wizard(db[item].get_owner()))
			{
				context *c = new context (db[item].get_owner());
				c->do_compound_command (item, ".startup", "", "");
				delete mud_scheduler.push_express_job (c);
			}
			else
				log_hack(GOD_ID, "non-wizard character %s(#%d) owns .startup command #%d in #%d",
							getname(db[item].get_owner()),
							db[item].get_owner(),
							item,
							COMMAND_LAST_RESORT);
	}
#endif
}

void
execute_shutdown(void)
{
#ifdef STARTUP

        dbref item;

        DOLIST (item, db[COMMAND_LAST_RESORT].get_commands())
        {
		if (!string_compare(db[item].get_name(),".shutdown"))
			if (Wizard(db[item].get_owner()))
			{
				context *c = new context (db[item].get_owner());
				c->do_compound_command(item, ".shutdown", "", "");
				delete mud_scheduler.push_express_job (c);
			}
			else
				log_hack(GOD_ID, 	"%s(#%d) owns .shutdown command #%d in #%d",
							getname(db[item].get_owner()),
							db[item].get_owner(),
							item,
							COMMAND_LAST_RESORT
				);
							
        }
#endif
}


int
init_game (
const	char	*infile,
const	char	*outfile)

{
	FILE	*f;
	int	total_failures;

	if ((f = fopen (infile, "r")) == NULL)
		return -1;
	
	/* ok, read it in */
	log_loading(infile, false);
	if (db.read (f) < 0)
		return (-1);

	/* Run the sanity-checker if required */
	if (sanity_check_db || sanity_only || fix_things)
	{
		log_message("Running Sanity Check");
		db.sanity_check();
		if(range || fatal)
		{
			total_failures = fatal + range;
			log_message("SANITY-CHECK failed on %d count%s", total_failures, total_failures == 1 ? "" : "s");
			log_message("SANITY-CHECK %d fatal%s, %d range%s ", fatal, fatal == 1 ? "":"s", range, range == 1 ? "":"d");
			if(hack_check)
				log_message("%d hacks%s ", hack, hack == 1 ? "":"s");
			log_message("%d other%s", broken, broken == 1 ? "":"s");
			exit(1);
		}
		if(fixed)
			log_message("SANITY-CHECK has repaired %d faults", fixed);
	}

	/* If we were only running the checker, give up now */
	if (sanity_only)
		exit(0);

	/* Keep going */
	db_patch_alarms ();
	log_loading(infile, true);

	/* everything ok */
	fclose(f);

	/* initialize random number generator */
	srand48 (getpid ());

	/* set up dumper */
	if (dumpfile)
		free(dumpfile);
	dumpfile = alloc_string(outfile);
	signal(SIGALRM, alarm_handler);
	signal(SIGHUP, hup_handler);
	signal(SIGCHLD, reaper);
	alarm_triggered = 0;
	alarm(dump_interval);
	
	/* read smd file */
	context	*read_context = new context (GOD_ID);
	read_context->do_smd("read", (char *)NULL); /* Read the SMD list */
	delete mud_scheduler.push_express_job (read_context);

	/* Do startup comands */
	execute_startups ();

	return (0);
}


String
context::sneakily_process_basic_command (
const	CString& original_command,
Command_status	&sneaky_return_status)

{
	Command_status	cached_return_status;
	String cached_return_string;
	String sneaky_return_string;

	int depth = get_sneaky_executed_depth();
	if(depth > get_depth_limit()) // Give them a chance!
	{
		sneaky_return_string = error_return_string;
	}
	else
	{
		set_sneaky_executed_depth(depth+1);

		/* Fill in our caches */
		cached_return_status = return_status;
		cached_return_string = return_string;

		/* Remember how deeply nested we were before */
		const	int	old_depth = call_stack.depth ();

		/* Do the command */
		process_basic_command (original_command.c_str());

		/* If the call stack is deeper, we just ran a nested command.  Run it to completion. */
		while (call_stack.depth () > old_depth)
			step ();

		/* Fill in our returns */
		sneaky_return_status = return_status;
		sneaky_return_string = return_string;

		/* Fix the old returns */
		return_status = cached_return_status;
		return_string = cached_return_string;

		set_sneaky_executed_depth(depth);
	}
	return (sneaky_return_string);
}


void
context::process_basic_command (
const	char	*original_command)

{
	const	char	*a0;
	char		*q;
	const	char	*p;
	char		smashed_original [MAX_COMMAND_LEN];
	char		command_buf [2 * MAX_COMMAND_LEN];
	char		*command = command_buf;
	bool		command_done = false;
	int		legal_command = 0;
	dbref		tracer;
	//time_t		dumptime = 0;
	command_details	*entry;
	const colour_at& ca= db[get_player()].get_colour_at();

	if(original_command == NULL)
		return;

	/* robustify player */
	if(player < 0 || player >= db.get_top () || ((Typeof(player) != TYPE_PLAYER) && (Typeof(player) != TYPE_PUPPET))) {
		log_bug("process_basic_command: bad player #%d", player);
		return;
	}

#if	defined(LOG_COMMANDS) || defined(LOG_COMMAND_LINES)
#ifdef	LOG_COMMAND_LINES
	if (!in_command())
	{
#endif /* LOG_COMMAND_LINES */
		log_command(	player,
						getname(player),
						get_effective_id(),
						getname(get_effective_id()),
						db[player].get_location(),
						getname(db[player].get_location()),
						original_command
		);
#ifdef	LOG_COMMAND_LINES
	}
#endif /* LOG_COMMAND_LINES */
#endif	/* defined(LOG_COMMANDS) || defined(LOG_COMMAND_LINES) */

	/* eat leading whitespace */
	while(*original_command && isspace(*original_command))
		original_command++;

	/* Tracing */

	/*
	 * Warning: in_command() doesn't mean that we're _actually_ in
	 * 	a command: it could be from an @force in a command
	 *	so if in_command() && get_current_command() == NOTHING
	 *	then this must be the case - a quick fix is to assume
	 *	that we don't want tracing. - Lee
	 */

	dbref effective_player=(Typeof(player)==TYPE_PUPPET) ? db[player].get_owner() : player;

	if (in_command () && get_current_command () == NOTHING)
		tracer = NOTHING;
	else if(in_command() && (db + get_current_command () != NULL) && (Wizard(effective_player) || Apprentice(effective_player) || XBuilder(effective_player) || (player==db[get_current_command()].get_owner()) || (effective_player==db[get_current_command()].get_owner())) && (Tracing(player) || Tracing(get_current_command())))
		tracer = effective_player;
	else if((!in_command()) && Tracing(player))
		tracer = effective_player;
	else
		tracer = NOTHING;

	if (tracer != NOTHING)
	{
		notify(tracer, "%s[%s, %s]%s %s%s",
			ca[COLOUR_TITLES],
			value_or_empty(getname (player)),
			in_command() ?
				value_or_empty(getname (get_current_command ())) :
				"(top): ",
			ca[COLOUR_TRACING],
			value_or_empty(original_command),
			COLOUR_REVERT);
	}
	else if (Debug(player) && ( !in_command() || (trace_command != get_current_command())))
	{
		trace_command= get_current_command();
		notify(effective_player, "%s[%s: %s%s]%s", ca[COLOUR_TITLES], getname (player), in_command() ? "" : "(top) ", in_command() ? getname (get_current_command ()) : original_command, COLOUR_REVERT);
	}

	/* eat extra white space */
	p = original_command;
	q = smashed_original;
	while(*p)
	{
		/* scan over word */
		while(*p && !isspace(*p))
			*q++ = *p++;

		/* smash spaces */
		while(*p && (*p != '\n') && isspace(*++p))
			;

		/* After a newline, everything is literal */
		if (*p == '\n')
		{
			while(*p)
				*q++ = *p++;
			break;
		}
		else if (*p)
			/* add a space to separate next word */
			*q++ = ' ';
	}

	/* terminate */
	*q = '\0';

	/* block dump to prevent db inconsistencies from showing up */
	alarm_block++;

#ifdef ABORT_FUSES
	if (!in_command())
	{
		set_simple_command (smashed_original);
		set_arg1 (NULL);
		set_arg2 (NULL);
		abort_from_fuse = 0;
		count_down_abort_fuses (*this, player);
		count_down_abort_fuses (*this, db[player].get_location());

		if (abort_from_fuse
			&& !((Apprentice(player) || Wizard(player))
				&& !abortable_command (smashed_original)))
		{
			alarm_block--;
			return;
		}
	}
#endif

	/* Substitute nested commands and variables in the typed string */
	/*
	 * This may not be able to complete if there is brace-substitution in the command.
	 *	In this case, it returns false, and we back out of any immediate command
	 *	execution as the compound-command code will catch it later.
	 *
	 * If this returns false, it will have placed a Brace_scope on the scope stack.
	 *
	 * PJC 18/2/97.
	 */
	if (!variable_substitution (smashed_original, command, MAX_COMMAND_LEN))
	{
#ifdef	DEBUG
		log_debug ("Backing out of braced command.\n");
#endif

		/* Unblock alarms during the back-out, but don't bother checking anything else */
		alarm_block--;
		return;
	}

	/* It's possible that variable_substitution has left us with nothing to do inside a 
           braced command - eg {${commandlist[blah]}} where 'blah' doesn't exist within
           commandlist. This then bombs. If you get other crashes this may need a more general
	   fix but I -think- this should do it. - Duncan */
	/* It isn't good enough, because variable expansion can leave us with just a space
	   so we need to get rid of leading spaces first - Adrian */

	/* find command word- This is necessary as variable_substitution may have
	   left us with a command such as: ' arg1 arg2' if the substitution returned
	   a blank, as described above. We want to skip that space. */

	for (; *command && isspace(*command); command++)
		;

	if ((command==NULL) || (*command=='\0'))
	{
		alarm_block--;
		return;
	}

	/* Tracing */
	if (tracer != NOTHING)
	{
		notify(tracer, "%s[exec]%s %s%s", ca[COLOUR_TITLES], ca[COLOUR_TRACING], command, COLOUR_REVERT);
	}

	if (strlen (command) >= MAX_COMMAND_LEN)
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Command is too long.");
	else if (*command == SAY_TOKEN || *command == ALT_SAY_TOKEN) /* single character token */
	{
		set_simple_command ("say");
		set_arg1 (command + 1);
		set_arg2 (NULL);
	}
	else if (*command == POSE_TOKEN)
	{
		set_simple_command ("pose");
		set_arg1 (command + 1);
		set_arg2 (NULL);
		legal_command = 1;
	}
	else if (*command == NOTIFY_TOKEN)
	{
		set_simple_command ("@areanotify");
		set_arg1 ("here");
		set_arg2 (command + 1);
	}
	else if ((*command != '@') && (can_move (*this, command)))
	{
		/* command is an exact match for an exit */
		set_simple_command ("go");
		set_arg1 (command);
		set_arg2 (NULL);
		legal_command = 1;
	}
	else
	{
		char		*a1;
		char		*a2 = NULL;	/* In case we don't set it later */
		char		*endarg;

		/* parse arguments */
		endarg = command + strlen(command);	/* Get the end of the command line */

		/* find arg1 */
		/* move over command word */
		for (a1 = command; *a1 && !isspace(*a1); a1++)
			;

		/* NUL-terminate command word */
		if (*a1)
			*a1++ = '\0';

		/* Make sure arg2 points to NULL if there isn't a 2nd argument */
		if (a1 <= endarg)
		{
			/* move over spaces */
			while (*a1 && isspace(*a1))
				a1++;
			if(a1 <= endarg)
			{
				/* find end of arg1, start of arg2 */
				for(a2 = a1; *a2 && *a2 != '='; a2++)
					;

				/* NUL-fill between end of arg1 and arg2 */
				for(q = a2 - 1; q >= a1 && isspace(*q); q--)
					*q = '\0';

				/* go past '=' and leading whitespace if present */
				if (*a2)
					*a2++ = '\0';

				while(*a2 && isspace(*a2))
					a2++;
			}
		}

		set_simple_command (command);
		set_arg1 (a1);
		set_arg2 (a2);
	}

	/* Do it */
	if (!in_command ())
	{
		count_down_fuses (*this, player, !TOM_FUSE);
		count_down_fuses (*this, db [player].get_location (), !TOM_FUSE);
	}

	a0 = get_simple_command ().c_str();

	/* Try to do a compound command */
	if ((*a0 != '|')
		&& (*a0 != '@')
		&& can_do_compound_command (get_simple_command (), get_arg1 (), get_arg2 ()))
		command_done = true;
	/* Try to do a compound command, if we're not overridden with | */
	else if (*a0 == '|')
	{
		/* || means an override even of COMMAND_LAST_RESORT */
		if ((*++a0 == '|')
			&& in_command ()
			&& (Wizard (get_current_command ())))
		{
			a0++;
		}
		/* It wasn't ||; try COMMAND_LAST_RESORT */
		else if (can_override_command (a0, get_arg1 (), get_arg2 ()))
		{
			command_done = true;
		}
	}
	else if ((*a0 == '@')
		&& can_override_command (a0, get_arg1 (), get_arg2 ()))
	{
		command_done = true;
	}

	/* We've bombed out everywhere else; is this a straight, simple command? */
	if (!command_done && ((entry = find_basic_command (a0)) != NULL))
	{
		(this->*(entry->context_address)) (get_arg1 (), get_arg2 ());
		command_done = true;
	}

	/* If we're still not done, we couldn't find it */
	if (!command_done)
	{
		if (in_command() && (Typeof(player)==TYPE_PLAYER))
		{
			sprintf (scratch_buffer,
				"%sCan't find basic command in %s%s%s. Full command line was:%s",
				 ca[COLOUR_ERROR_MESSAGES],
				 ca[COLOUR_TRACING],
				 unparse_object (*this, get_current_command ()),
				 ca[COLOUR_ERROR_MESSAGES],
				 COLOUR_REVERT);

			notify(player, "%s", scratch_buffer);
			strcpy (scratch_buffer, command);
			if (get_arg1())
			{
				strcat (scratch_buffer, " ");
				strcat (scratch_buffer, get_arg1().c_str());
			}
			if (get_arg2())
			{
				strcat (scratch_buffer, " = ");
				strcat (scratch_buffer, get_arg2().c_str());
			}
			notify_colour (player, player, COLOUR_TRACING, "%s", scratch_buffer);
			notify_colour (player, player, COLOUR_MESSAGES, "This was expanded from the original:");
			notify_colour (player, player, COLOUR_TRACING, "%s", original_command);
		}
		else
			notify_colour(player, player, COLOUR_MESSAGES, "Huh?    (Type \"help\" for help.)");
#ifdef LOG_FAILED_COMMANDS
		if((!controls_for_write (db[player].get_location())) && (!NoHuhLogs(db[db[player].get_location()].get_owner()))) {
			/* We want to know if the room is owned by an NPC, if so 'redirect'
			 * the HUH to the owner of the NPC
			 */
			dbref npc_owner;

			if (Typeof(db[db[player].get_location()].get_owner()) == TYPE_PUPPET) {
				npc_owner = db[db[db[player].get_location()].get_owner()].get_owner();
			}
			else {
				npc_owner = -1; // this will always be a room
			}
			
			log_huh(	player,											/* player-id */
						getname(player),								/* player-name */
						db[player].get_location(),						/* location-id */
						getname (db[player].get_location()),			/* loccation-name */
						db[db[player].get_location()].get_owner(),		/* location-owner-id */
						npc_owner,										/* NPC owner; if room is owned by an NPC */
						command,										/* command */
						reconstruct_message (get_arg1 (), get_arg2 ())	/* command arguments */
			);
		}
#endif /* LOG_FAILED_COMMANDS */
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
	}

#ifdef	HACK_HUNTING
	if (get_current_command () > 0)
	{
		if ((get_effective_id () != player)
			&& (Wizard (get_effective_id ()))
			&& (!legal_command)
			&& (!Wizard(get_current_command ())))
		log_hack(db[get_current_command()].get_owner(), "%s(%d) hacked %s(%d) (originally %s) giving %s %s=%s",
					getname(player),						player,
					getname (get_current_command ()),		get_current_command(),
					original_command,
					command,
					get_arg1().c_str(),
					get_arg2().c_str()
		);
	}
#endif	/* HACK_HUNTING */

	/* Tracing */
	if (tracer != NOTHING)
	{
		notify(tracer, "%s[%s]%s %s%s", 
		       (return_status == COMMAND_SUCC) ? ca[COLOUR_SUCCESS] : ca[COLOUR_FAILURE],
		       (return_status == COMMAND_SUCC) ? "succ" : "fail", 
		       ca[COLOUR_TRACING],
		       return_string.c_str(),
		       COLOUR_REVERT);
	}

/*
	if (!return_string)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Basic command returned NULL string (wizards have been notified).");
		notify_wizard ("BUG: Basic command returned NULL. Command was:");
		notify_wizard ("%s", original_command);
		log_bug("Command returned NULL return_string: %s (%s)", command, original_command);
		//set_return_string (error_return_string);
	}
*/
	/* unblock alarms */
	alarm_block--;


}


void
mud_command (
dbref		player,
const	char	*command)

{
	/* Sanity checks */
	if ((Typeof (player) != TYPE_PLAYER) && (Typeof (player) != TYPE_PUPPET))
	{
		log_bug("Non-player %s(#%d) trying to execute the command \"%s\"", getname(player), player, command);
		return;
	}
	if ((db[player].get_location() < 0) && (db[player].get_location() >= db.get_top ()))
	{
		log_bug("Player #%s at location #%d", unparse_object (context (GOD_ID), player), db[player].get_location());
		return;
	}

	/* Do the command */
	context	*root_context = new context (player);
	root_context->process_basic_command (command);
	/* If the root context was never scheduled, it won't have had a chance to fire any sticky fuses, so... */
	if (!root_context->get_scheduled ())
		root_context->fire_sticky_fuses (*root_context);
	while (mud_scheduler.runnable ())
		if (mud_scheduler.step () == root_context && root_context)
		{
			delete root_context;
			root_context = 0;
		}

/* Delete any contexts that didn't enter the while loop (non compound commands 
 * I think)  FIX THIS?  I suspect may cause problems when the scheduler is
 * running proper -Abs */
	if(root_context)
		delete root_context;
	/* Check for alarms that went off */
	mud_time_sync ();
}


void mud_run_dotcommand(dbref player, const CString& command)
{
	dbref the_command;
	/** Needs to call can_do_compound_command **/
	Matcher player_matcher (player, command, TYPE_COMMAND, player);
	player_matcher.check_keys ();
	player_matcher.match_player_command ();
	if ((the_command = player_matcher.match_result ()) != NOTHING)
	{
		context *login_context = new context (player);
		if (!Dark (the_command) && could_doit (*login_context, the_command))
		{
			login_context->do_compound_command (the_command, command, getname (player), "");
			delete mud_scheduler.push_express_job (login_context);
		}
	}
	Matcher area_matcher (player, command, TYPE_COMMAND, player);
	area_matcher.check_keys ();
	area_matcher.match_command_from_location (db[player].get_location());
	if (((the_command = area_matcher.match_result ()) != NOTHING) && (db[the_command].get_location() != COMMAND_LAST_RESORT))
	{
		context *login_context = new context (player);
		if (!Dark (the_command) && could_doit (*login_context, the_command))
		{
			login_context->do_compound_command (the_command, command, getname (player), "");
			delete mud_scheduler.push_express_job (login_context);
		}
	}

	DOLIST (the_command, db[COMMAND_LAST_RESORT].get_commands())
	{
		if (!string_compare(db[the_command].get_name(),command))
			if (Wizard(db[the_command].get_owner()))
			{
				context *login_context = new context (player);
				login_context->do_compound_command (the_command, command, getname(player), "");
				delete mud_scheduler.push_express_job (login_context);
			}
			else
				log_hack(GOD_ID, "Global .login command (#%d) not owned by a Wizard", the_command);
	}

}

void mud_connect_player (dbref player)
{
	time_t now;

	if (!Connected (player))
	{
		db[player].set_colour_at(new colour_at(db[player].get_colour()));
		db[player].set_colour_play(make_colour_play(player, db[player].get_colour()));
		db[player].set_colour_play_size(find_number_of_players(db[player].get_colour()));
		time(&now);
		sprintf(scratch_buffer, "%ld", (long int)now);
		db [player].set_fail_message (scratch_buffer);
	}

	db [player].set_flag(FLAG_CONNECTED);

	mud_run_dotcommand(player, ".login");
}


void mud_disconnect_player (dbref player)
{
	time_t last, total, now;

	mud_run_dotcommand(player, ".logout");

	if (connection_count (player) == 0)
	{
		channel_disconnect(player);

		db [player].clear_flag(FLAG_CONNECTED);

		if (db [player].get_ofail())
			total = atol (db [player].get_ofail().c_str());
		else
			total = 0;
		if (db [player].get_fail_message())
			last = atol (db [player].get_fail_message().c_str());
		else
			last = 0;

		time (&now);
		total += (now - last);
		sprintf (scratch_buffer, "%ld", (long int)total);
		db [player].set_ofail (scratch_buffer);

		/* The following section will free the colour_at array which
		   is only needed during connect time */
		db[player].set_colour_at(0);
	}
}


void mud_time_sync ()
{
	dbref		the_player;
	dbref		an_alarm;
	dbref		a_command;
	dbref		a_location;
	dbref		cached_location;
	int		recursion_counter = 0;
        time_t		now;

#ifdef SYSV
	wait(NULL);        /* Cludge to clean up SYS V */
#endif	/* SYSV */

	if((alarm_triggered > 0) && !alarm_block)
		fork_and_dump ();
	if (alarm_triggered < 0)
	{
		long dumptime = time(NULL) - dump_start_time;
		notify_wizard("[Database backed up in %s]", small_time_string(dumptime));
		alarm_triggered = 0;
	}

        alarm_block++;
        time (&now);
        while ((an_alarm = db.alarm_pending (now)) != NOTHING)
        {
		if (recursion_counter++ > ALARM_RECURSION_LIMIT)
		{
			alarm_block--;
			return;
		}
		if (Typeof (an_alarm) == TYPE_ALARM)
		{
			a_command = db[an_alarm].get_destination();
			if (a_command != NOTHING)
			{
				if (Typeof (a_command) != TYPE_COMMAND)
				{
					notify_wizard ("Pended alarm command is not working.");
					alarm_block--;
					return;
				}
				the_player = db[a_command].get_owner();
				if (payfor (the_player, ALARM_EXECUTE_COST))
				{
					cached_location = db[the_player].get_location();
					a_location = db[a_command].get_location();
					if ((Typeof (a_location) == TYPE_PLAYER) || (Typeof (a_location) == TYPE_PUPPET))
						a_location = db[a_location].get_location();
					moveto (the_player, a_location);
					context *alarm_context = new context (the_player);
					alarm_context->do_compound_command (a_command, "ALARM", "", "");
					delete mud_scheduler.push_express_job (alarm_context);
					moveto (the_player, cached_location);
				}
				else
					notify_colour(the_player, the_player,
						      COLOUR_ERROR_MESSAGES,
						"Alarm %s failed to execute, due to lack of money.",
						unparse_object (context (the_player), an_alarm));
			}
			if (db[an_alarm].get_description())
				db.pend (an_alarm);
		}
		else
		{
			notify_wizard("PENDED ALARM IS BROKEN - Object %s found.", unparse_object (context (GOD_ID), an_alarm));
			alarm_block--;
			return;
		}
        }
        alarm_block--;
}

/*
 * @rem
 *
 * Do nothing. Cheaply, rather than as a soft-code command.
 */
void
context::do_at_rem(
const CString&,
const CString&)
{
	return;		// Do nothing.
}

/*
 * Print a warning, unless the context is gagged.
 */
void
context::do_at_warn(
const CString& arg1,
const CString& arg2)
{
	if(!gagged_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s", reconstruct_message(arg1, arg2));
	}
	RETURN_SUCC;
}


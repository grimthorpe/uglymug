/* static char SCCSid[] = "@(#)interface.c 1.155 00/06/29@(#)"; */
/** \file interface.c
 * The socket interface, along with all the other grubbiness that means players can connect.
 *
 * This file is long overdue for splitting into a number of smaller files.
 */

#include "copyright.h"

#include "os.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/errno.h>
#include <ctype.h>
#include <sys/param.h>

#if	USE_BSD_SOCKETS
#	include <sys/wait.h>
#	include <fcntl.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netdb.h>

	// Whatever Microsoft's shortcomings, they like their typedefs.
	// Some of the code below uses Microsoft-ish Winsock typedefs;
	// here are their non-MS definitions.  PJC 19/4/2003.
	typedef	int	SOCKET;
#	define	SETSOCKOPT_OPTCAST	(int *)
#	define	closesocket	close
#endif	/* USE_BSD_SOCKETS */

#if	USE_WINSOCK2
#	include <winsock2.h>
#	define	MAXHOSTNAMELEN	1024
#	define	ECONNREFUSED	WSAECONNREFUSED
#	define	EWOULDBLOCK	WSAEWOULDBLOCK
#	define	SETSOCKOPT_OPTCAST (char*)
typedef	int	socklen_t;

static int write(SOCKET s, const char * buf, int len)
{
	int result = send(s, buf, len, 0);

	return result;
}

static int read(SOCKET s, unsigned char * buf, int len)
{
	int result = recv(s, (char *)buf, len, 0);

	return result;
}
/**
 * Shockingly naive implementation. TODO: Improve. PJC 20/4/2003.
 */
void gettimeofday (struct timeval *tv, void *)
{
	time (&tv->tv_sec);
	tv->tv_usec = 0;
}
#endif	/* USE_WINSOCK2 */

#include "db.h"
#include "descriptor.h"
#include "externs.h"
#include "interface.h"
#include "config.h"
#include "command.h"
#include "concentrator.h"
#include "context.h"
#include "lists.h"
#include "match.h"
#include "colour.h"
#include "game_predicates.h"
#include "log.h"

/* JPK - regexp needed for conditional output */
#include "regexp_interface.h"

#if	HAS_CURSES
#	ifdef USE_TERMINFO
#		if defined (sun) || (linux)
#			include <curses.h>
#		else
#			include <ncurses.h> 
#		endif
#		include <term.h>
#	endif	/* USE_TERMINFO */
#	ifdef clear // Stupid curses/ncurses defines this as a macro.
#		undef clear
#	endif /* clear */
#endif	/* HAS_CURSES */

#ifdef DEBUG_TELNET
#	define	TELOPTS		/* make telnet.h define telopts[] */
#	define	TELCMDS
#endif /* DEBUG_TELNET */

#ifdef ABORT
#	undef	ABORT
#endif
#ifdef linux
#	include <arpa/telnet.h>
#else
#	include "telnet.h"
#endif

#define safe_strdup(x) ((x==0)?strdup(""):strdup(x))

#if	NEEDS_GETDTABLESIZE
#	define	getdtablesize()		MAX_USERS + 4
#endif	/* NEEDS_GETDTABLESIZE */

#if	NEEDS_STRSIGNAL
const char *strsignal (const int sig)
{
	static	char	buf [20];	// Assume 20 digits is good enough. PJC 20/4/2003.
	sprintf (buf, "%d", sig);
	return buf;
}
#endif	/* NEEDS_STRSIGNAL */

#define	HELP_FILE		"help.txt"
#define	SIGNAL_STACK_SIZE	200000
#define	LOGTHROUGH_HOST		INADDR_LOOPBACK	/* 127.0.0.1 */
#define	LOCAL_ADDRESS_MASK	0x7f000000	/* 127.0.0.0 */
#define	DOMAIN_STRING		""

#define	DUMP_QUIET		1
#define	DUMP_WIZARD		2
#define SELF_BOOT_TIME		10

#define COMPLETE_NAME		0
#define COMPLETE_ALIAS		1
#define PARTIAL_NAME		2
#define PARTIAL_ALIAS		3
#define NOTHING_YET		10

#define NO_COLOUR_SUBSTITUTION	1

	time_t	game_start_time;
extern	int	errno;
extern	char	player_booting[BUFFER_LEN];
extern	char	boot_reason[BUFFER_LEN];
int		shutdown_flag = 0;
dbref		shutdown_player = NOTHING;
static	char	vsnprintf_result[BUFFER_LEN];
static	char	scratch[BUFFER_LEN];
bool descriptor_data::check_descriptors = false;
int			peak_users;
String	descriptor_data::LastCommandName;
dbref	descriptor_data::LastCommandCaller = NOTHING;
int	descriptor_data::LastCommandCount = 0;
int	descriptor_data::LastCommandDepth = 0;

String str_on = "on";
String str_off = "off";

static const char *connect_fail =
	"Either that player does not exist, or has a different password.\n";
static const char *create_fail =
	"Either there is already a player with that name, or that name is illegal.\n";
static const char *flushed_message =
	"<Output Flushed>\n";
static const char *emergency_shutdown_message =
	"Emergency shutdown.\n\n";
static const char *first_shutdown_message =
	"All hands brace for real life.\n";
static const char *second_shutdown_message =
	" has shut down the game";
static const char *reject_message =
	"Connections to UglyMUG from your site have been banned.\n"
	"Please email uglymug@uglymug.org.uk for more information.\n";
static const char *reject_updatedsmd =
	"Connections from your current location have been stopped\n"
	"Please email uglymug@uglymug.org.uk for more information.\n";
static const char *create_banned =
	"Your site is banned from creating characters, owing to\n"
	"abuse in the recent past. Please email uglymug@uglymug.org.uk\n"
	"and ask us to create a character for you.\n";
static const char *guest_create_banned =
	"Your site is banned from using the guest character, owing to\n"
	"abuse in the recent past. Please email uglymug@uglymug.org.uk\n"
	"and ask us to create a character for you.\n";
static const char *name_prompt =
	"Please enter your name: ";
static const char *cancel_create =
	"\nCharacter creation cancelled.\n";
static const char *password_prompt =
	"Please enter your password: ";
static const char *new_password_prompt =
	"Please enter a password for this character: ";
static const char *confirm_password =
	"\nPlease re-enter your password: ";
static const char *character_not_found=
	"Character not found.\n";
static const char *new_character_prompt =
	"Do you wish to create a character with this name (y/n)? ";
static const char *player_is_thick =
	"Please answer yes or no (y/n)\n";
static const char *too_many_attempts =
	"\n\nToo many attempts, disconnecting.\n";
static const char *passwords_dont_match =
	"\nThe passwords didn't match. Character creation cancelled.\n";
static const char* HALFQUIT_MESSAGE = "\nYour connection will be available for 5 minutes.\n";
static const char* HALFQUIT_FAIL_MESSAGE = "\nYou never connected as a player!\n";

/* Anything that's not in here will get a WONT/DONT. */

struct iac_command
{
	unsigned char	command;
	unsigned char	option;
};

// This is the list of initial commands to send.
iac_command iac_initial[] =
{
	{ DO,	TELOPT_LFLOW	},	/* Do LFLOW */
	{ DO,	TELOPT_TTYPE	},	/* Send terminal type */
	{ DO,	TELOPT_NAWS	},	/* Send window size */
	{ WONT,	TELOPT_ECHO	},	/* Local echo */
	{ DO,	TELOPT_SNDLOC	},	/* Send location */
//	{ DO,	TELOPT_LINEMODE },	/* Do Linemode negotiation */
	{ DONT,	TELOPT_SGA	},	/* Suppress Go-Ahead */
	{ 0,	0		}
};

// This is the list of commands we'll use
// Anything else that we receive a DO/WILL for we reply WONT/DONT
int	iac_we_use[] =
{
	TELOPT_ECHO,		// Echo on/off
	TELOPT_SNDLOC,		// Send location
	TELOPT_TTYPE,		// Terminal Type
	TELOPT_NAWS,		// Window size
	TELOPT_LFLOW,		// Remote flow control
	TELOPT_SGA,		// Suppress go ahead
	TELOPT_LINEMODE,	// LineMode
	-1
};

void
descriptor_data::do_write(const char * c, int i)
{
/*
 * There is a logic behind this function!
 *
 * This is called to echo everything that is typed.
 * Unfortunately, some clients that don't require stuff to be echo'd
 * back don't support telnet IAC stuff (notably older tinyfugue, tinytalk
 * and some Win32 clients).
 *
 * The way we detect this is we *ONLY* echo if our detection routine picks
 * up on echo being needed, and we've received an IAC ever during this
 * connection.
 *
 * Don't ask me why this is, it just is.
 * When I originally wrote the code I knew what I was doing. Now I am
 * replacing code that was ripped out by people not understanding what
 * this does.
 *
 * Grimthorpe 6-June-1997
 */
	if(_got_an_iac && t_echo) write(get_descriptor(), c, i);
}

void
descriptor_data::save_command(String command)
{
	input.push_back(command);
}

struct	descriptor_data		*descriptor_list = NULL;

static	SOCKET*			ListenPorts;
static	int			NumberListenPorts;

#ifdef CONCENTRATOR
static	SOCKET			conc_listensock, conc_sock, conc_connected;
#endif
static	int			ndescriptors = 0;
static	int			outgoing_conc_data_waiting = 0;

void				process_commands	(void);
void				make_nonblocking	(int s);
const	char			*convert_addr 		(unsigned long);
struct	descriptor_data		*new_connection		(SOCKET sock);
void				parse_connect 		(const char *msg, char *command, char *user, char *pass);
SOCKET				make_socket		(int, unsigned long);
void				bailout			(int);
u_long				addr_numtoint		(char *, int);
u_long				addr_strtoint		(char *, int);
//void				get_value_from_subnegotiation (struct descriptor_data *d, unsigned char *, unsigned char, int);
#ifdef CONCENTRATOR
int				connect_concentrator	(int);
int				send_concentrator_data	(int channel, char *data, int length);
int				process_concentrator_input(SOCKET sock);
void				concentrator_disconnect	(void);
#endif

struct terminal_set_command
{
	const char	*name;
	Command_status	(descriptor_data::*set_function) (const String&, bool);
	String		(descriptor_data::*query_function) ();
	bool		deprecated;
} terminal_set_command_table[] =
{
	{ "termtype",	&descriptor_data::terminal_set_termtype, &descriptor_data::terminal_get_termtype },
	{ "type",	&descriptor_data::terminal_set_termtype, &descriptor_data::terminal_get_termtype, true },
	{ "width",	&descriptor_data::terminal_set_width, &descriptor_data::terminal_get_width },
	{ "height",	&descriptor_data::terminal_set_height, &descriptor_data::terminal_get_height },
	{ "wrap",	&descriptor_data::terminal_set_wrap, &descriptor_data::terminal_get_wrap },
	{ "lftocr",	&descriptor_data::terminal_set_lftocr, &descriptor_data::terminal_get_lftocr },
	{ "pagebell",	&descriptor_data::terminal_set_pagebell, &descriptor_data::terminal_get_pagebell },
	{ "echo",	&descriptor_data::terminal_set_echo, &descriptor_data::terminal_get_echo },
	{ "recall",	&descriptor_data::terminal_set_recall, &descriptor_data::terminal_get_recall },
	{ "effects",	&descriptor_data::terminal_set_effects, &descriptor_data::terminal_get_effects },
	{ "halfquit",	&descriptor_data::terminal_set_halfquit, &descriptor_data::terminal_get_halfquit },
	{ "noflush",	&descriptor_data::terminal_set_noflush, &descriptor_data::terminal_get_noflush },
	{ "sevenbit",	&descriptor_data::terminal_set_sevenbit, &descriptor_data::terminal_get_sevenbit },
	{ "colour_terminal",	&descriptor_data::terminal_set_colour_terminal, &descriptor_data::terminal_get_colour_terminal },
	{ "commandinfo",	&descriptor_data::terminal_set_emit_lastcommand, &descriptor_data::terminal_get_emit_lastcommand },

	{ NULL, NULL }
};

#define MALLOC(result, type, number) do {			\
	if (!((result) = (type *) malloc ((number) * sizeof (type))))	\
		panic("Out of memory");				\
	} while (0)

#ifdef DEBUG
#define FREE(x) { if((x)==NULL) \
			log_bug("WARNING: attempt to free NULL pointer (%s, line %d)", __FILE__, __LINE__); \
		  else \
		  { \
		  	free((x)); \
		  	(x)=NULL; \
		  } \
	  	}
#else
#define FREE(x) free((x))
#endif /* DEBUG */

#define MAX_LAST_COMMANDS 16
class LogCommand
{
private:
	LogCommand(const LogCommand&); // DUMMY
	LogCommand& operator=(const LogCommand&); // DUMMY

	String	command;
	int	player;
public:
	LogCommand(int _player, String _command) : command(_command), player(_player)
	{
	}
	~LogCommand()
	{
	}
	int get_player() { return player; }
	String get_command() { return command; }
};

LogCommand* LastCommands[MAX_LAST_COMMANDS];
int LastCommandsPtr = 0;



void set_signals()
{
#if	HAS_SIGNALS
	/* we don't care about SIGPIPE, we notice it in select() and write() */
	signal (SIGPIPE, SIG_IGN);

	/* standard termination signals */
	signal (SIGINT, bailout);
	signal (SIGTERM, bailout);

	/* catch these because we might as well */
	signal (SIGILL, bailout);
	signal (SIGTRAP, bailout);
	signal (SIGIOT, bailout);
#ifdef SIGEMT
	signal (SIGEMT, bailout);
#endif
	signal (SIGFPE, bailout);
	signal (SIGBUS, bailout);
	signal (SIGSEGV, bailout);
#ifdef SIGSYS
	signal (SIGSYS, bailout);
#endif
	signal (SIGXCPU, bailout);
	signal (SIGXFSZ, bailout); 
	signal (SIGVTALRM, bailout);
	signal (SIGUSR2, bailout);
#endif	/* HAS_SIGNALS */
}


int
count_connect_types (
int	type)
{
	dbref	player;
	int count,wizard,god,mortal,apprentice,builder,xbuilder,admin,welcomer = 0;
	count=wizard=god=mortal=apprentice=builder=xbuilder=admin=welcomer = 0;

	int blob = 1;
	descriptor_data	*d;

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (player = d->get_player()))
		{
			if(Builder(player))
			{
				builder++;
				blob = 0;
			}
			if(XBuilder(player))
			{
				xbuilder++;
				blob = 0;
			}
			if(Welcomer(player))
			{
				welcomer++;
				blob = 0;
			}
			if(Wizard(player))
			{
				wizard++;
				admin++;
				blob = 0;
			}
			if(Apprentice(player))
			{
				apprentice++;
				if(!Wizard(player))
					admin++; //some people have both flags
				blob = 0;
			}
			if(blob == 1)
				mortal++;
			else
				blob = 1;

			count++;
			
		}

	switch(type)
	{
		case T_MORTAL:
			return(mortal);
		case T_BUILDER:
			return(builder);
		case T_XBUILDER:
			return(xbuilder);
		case T_WIZARD:
			return(wizard);
		case T_WELCOMER:
			return(welcomer);
		case T_GOD:
			return(god);
		case T_ADMIN:
			return(admin);
		case T_APPRENTICE:
			return(apprentice);
		case T_NONE:
		default:
			return(count);
	}
}

int
connection_count (
dbref	player)

{
	struct	descriptor_data	*d;
	int			count = 0;

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (d->get_player() == player))
			count++;
	return (count);
}

void notify_area (dbref area, dbref originator, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;
	bool deruded = false,
		censorall= false;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result, sizeof(vsnprintf_result),fmt, vl);
	va_end (vl);

	if (Censored(originator))
	{
		censorall= true;
		censored=censor(vsnprintf_result);
		deruded=true;
	}

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->IS_CONNECTED() && in_area (d->get_player(), area))
		{
			if (censorall || Censorall(d->get_player()) ||
			    (Censorpublic(d->get_player()) && Public(db[d->get_player()].get_location())))
			{
				if (deruded==false)
				{
					deruded=true;
					censored=censor(vsnprintf_result);
				}
				d->queue_string (censored);
				d->queue_string ("\n");
			}
			else
			{
				d->queue_string (vsnprintf_result);
				d->queue_string ("\n");
			}
			d->queue_string(COLOUR_REVERT);
		}
	}
}

void notify_listeners (const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (Listen(d->get_player())))
		{
			d->queue_string (vsnprintf_result);
			d->queue_string(COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

/* This is only used for Wall */
/* I am assuming that much, which is why I'm not passing
   a colour parameter */
void notify_all (const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED())
		{
			d->queue_string (db[d->get_player()].get_colour_at()[COLOUR_SHOUTS]);
			d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify_wizard(const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (Wizard (d->get_player()) || Apprentice(d->get_player())) && (!(Haven(d->get_player()))))
		{
			d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify_wizard_unconditional(const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (Wizard (d->get_player()) || Apprentice(d->get_player())))
		{
			d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify_wizard_natter(const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start(vl, fmt);
	vsnprintf(vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end(vl);

	for (d = descriptor_list; d; d=d->next)
		if (d->IS_CONNECTED() && ((Wizard (d->get_player())) || (Apprentice (d->get_player())) || (Natter(d->get_player()))))
		{
			d->queue_string(db[d->get_player()].get_colour_at()[COLOUR_NATTER_TITLES]);
			d->queue_string("[WIZARD]");
			d->queue_string(db[d->get_player()].get_colour_at()[COLOUR_NATTERS]);
			d->queue_string(vsnprintf_result);
			d->queue_string(COLOUR_REVERT);
			d->queue_string("\n");
		}
}

void notify_welcomer_natter(const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start(vl, fmt);
	vsnprintf(vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end(vl);

	for (d = descriptor_list; d; d=d->next)
		if (d->IS_CONNECTED() && (Welcomer(d->get_player())))
		{
			d->queue_string(db[d->get_player()].get_colour_at()[COLOUR_WELCOMER_TITLES]);
			d->queue_string("[WELCOMER]");
			d->queue_string(db[d->get_player()].get_colour_at()[COLOUR_NATTERS]);
			d->queue_string(vsnprintf_result);
			d->queue_string(COLOUR_REVERT);
			d->queue_string("\n");
		}
}

/* This function is passed:
 * The player who is being notified
 * The talker who is the person notifying
 * The type of operation being done (say, tell etc...)
 * And the strings
 */
void notify_colour(
dbref player,
dbref talker,
int colour,
const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	/* I'm relying on the coders either calling it with 
	 * a colour or a player. It will do odd things if you don't
	 * set either
	 */


	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			d->queue_string (player_colour(player, talker, colour));
			d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify_censor_colour(
dbref player,
dbref talker,
int colour,
const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;
	bool deruded=false;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	/* I'm relying on the coders either calling it with 
	 * a colour or a player. It will do odd things if you don't
	 * set either
	 */


	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			d->queue_string (player_colour(player, talker, colour));
			if (Censored(talker) || Censorall(player))
			{
				if (deruded==false)
				{
					deruded=true;
					censored=censor(vsnprintf_result);
				}
				d->queue_string(censored);
			}
			else
				d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify_public_colour(
dbref player,
dbref talker,
int colour,
const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	bool deruded=false;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	/* I'm relying on the coders either calling it with 
	 * a colour or a player. It will do odd things if you don't
	 * set either
	 */


	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			d->queue_string (player_colour(player, talker, colour));
			if (Censorall(player) || Censored(talker) || (Censorpublic(player) && Public(db[player].get_location())) )
			{
				if (deruded==false)
				{
					deruded=true;
					censored=censor(vsnprintf_result);
				}
				d->queue_string (censored);
			}
			else
				d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

static char *chop_string(const char *string, int size)
{
	static char whostring[256];
	char *p=whostring;
	const char *q=string;
	int visible=0,
	    copied=0;
	int percent_primed =0;
	while (q && *q && (visible < size) && (copied < 255))
	{
		if (*q == '%')
		{
			if (percent_primed)
			{
				visible++;
				percent_primed=0;
			}
			else
				percent_primed=1;
		}
		else
		{
			if (percent_primed)
				percent_primed=0;
			else
				visible++;
		}
		*p++=*q++;
		copied++;
	}


	while ((visible++ < size) && (copied < 255))
	{
		copied++;
		*p++=' ';
	}
	*p='\0';
	return whostring;
}

void terminal_underline(dbref player, const char *string)
{

	struct descriptor_data *d;
	int twidth=0;
	int ulen=strlen(string);
	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			twidth= d->terminal.width-1;
			if (twidth<=0)
				twidth = 79; // A reasonable default
			if (twidth > ulen)
				twidth=ulen;
			d->queue_string (chop_string(string, twidth));
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify(dbref player, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void notify_norecall_conditional(String match,dbref player, const char *fmt, ...)
{

        struct descriptor_data *d;
        va_list vl;

        va_start (vl, fmt);
        vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
        va_end (vl);
	
	// JPK *FIXME*
	if (match)
	{
		RegularExpression re(match);
		if (re.Match(vsnprintf_result))
		{
       	 		for (d = descriptor_list; d; d = d->next)
       	        		if (d->IS_CONNECTED() && d->get_player() == player)
       	        		{
       	        		        d->queue_string (vsnprintf_result, 0, 0);
       	        		}
		}
	}
	else
	{
		for (d = descriptor_list; d; d = d->next)
			if (d->IS_CONNECTED() && d->get_player() == player)
			{
				d->queue_string (vsnprintf_result, 0, 0);
			}
	}
}

void notify_norecall(dbref player, const char *fmt, ...)
{
        struct descriptor_data *d;
        va_list vl;

        va_start (vl, fmt);
        vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
        va_end (vl);

        for (d = descriptor_list; d; d = d->next)
                if (d->IS_CONNECTED() && d->get_player() == player)
                {
                        d->queue_string (vsnprintf_result, 0, 0);
                }
}


/* notify_censor used when we want to take account of the censorship
   flags 'censored' and 'censorall' */

void notify_censor(dbref player, dbref originator, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;
	bool deruded=false;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			if (Censorall(player) || Censored(originator))
			{
				if (deruded==false)
				{
					deruded=true;
					censored=censor(vsnprintf_result);
				}
				d->queue_string(censored);
			}
			else
				d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

/* Notify public is used when we want to take notice of
   the flags: 'censored', 'censorall', and 'censorpublic' */

void notify_public(dbref player, dbref originator, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	bool deruded=false;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			if (Censorall(player) || Censored(originator) || (Censorpublic(player) && Public(db[player].get_location())))
			{
				if (deruded==false)
				{
					deruded=true;
					censored=censor(vsnprintf_result);
				}
				d->queue_string (censored);
			}
			else
				d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
		}
}

void beep (dbref player)
{
	struct descriptor_data *d;

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->IS_CONNECTED()
			&& (d->get_player() == player)
			&& d->terminal.pagebell)
		{
			d->queue_write("\007", 1);
			d->process_output();
		}
	}
}

struct timeval timeval_sub(struct timeval now, struct timeval then)
{
	now.tv_sec -= then.tv_sec;
	now.tv_usec -= then.tv_usec;
	if (now.tv_usec < 0)
	{
		now.tv_usec += 1000000;
		now.tv_sec--;
	}
	return now;
}

int msec_diff(struct timeval now, struct timeval then)
{
	return ((now.tv_sec - then.tv_sec) * 1000
		+ (now.tv_usec - then.tv_usec) / 1000);
}

struct timeval msec_add(struct timeval t, int x)
{
	t.tv_sec += x / 1000;
	t.tv_usec += (x % 1000) * 1000;
	if (t.tv_usec >= 1000000)
	{
		t.tv_sec += t.tv_usec / 1000000;
		t.tv_usec = t.tv_usec % 1000000;
	}
	return t;
}

struct timeval update_quotas(struct timeval last, struct timeval current)
{
	int nslices;
	struct descriptor_data *d;

	nslices = msec_diff (current, last) / COMMAND_TIME_MSEC;

	if (nslices > 0)
	{
		for (d = descriptor_list; d; d = d -> next)
		{
			d -> quota += COMMANDS_PER_TIME * nslices;
			if (d -> quota > COMMAND_BURST_SIZE)
			d -> quota = COMMAND_BURST_SIZE;
		}
	}
	return msec_add (last, nslices * COMMAND_TIME_MSEC);
}


/* Well this is it.  Basically:

   - Build a new fd_set of all connected players (and the concentrator
     if it's around)
   - select() on them, and deal with connected player I/O, and new
     connects/disconnects
   - Watch for concentrator disconnects, and boot all concentrated players
     if this should happen
   - Check for player timeouts and issue appropriate warnings
   - Keep going until shutdown (or crash :-) ). */

void mud_main_loop(int argc, char** argv)
{
	fd_set			input_set, output_set;
	time_t			now;
	long			diff;
	struct	timeval		last_slice, current_time;
	struct	timeval		next_slice;
	struct	timeval		timeout, slice_timeout;
	SOCKET			maxd;
	int			select_value;
	struct	descriptor_data	*d, *dnext;
	struct	descriptor_data	*newd;
	int			avail_descriptors;
#ifdef CONCENTRATOR
	int			conc_data_sent;
#endif

#if	USE_WINSOCK2
	// For Winsock, ensure the socket system is initialised.
	WSADATA wsaData;
	WORD versionRequested = MAKEWORD (2, 0);
	int err;
	err = WSAStartup (versionRequested, &wsaData);
#endif	/* USE_WINSOCK2 */

#ifdef CONCENTRATOR
	conc_listensock = make_socket (CONC_PORT, INADDR_ANY);
#endif
	maxd = 0;
	if(argc > 0)
	{
		NumberListenPorts = argc;
		ListenPorts = new SOCKET [NumberListenPorts];
		for(int i = 0; i < argc; i++)
		{
			ListenPorts[i] = make_socket(atoi(argv[i]), INADDR_ANY);
			maxd = MAX(maxd, ListenPorts[i]);
		}
	}
	else
	{
		NumberListenPorts = 1;
		ListenPorts = new SOCKET [1];
		ListenPorts[0] = make_socket(TINYPORT, INADDR_ANY);
		maxd = ListenPorts[0];
	}
	maxd++;

	gettimeofday(&last_slice, (struct timezone *) NULL);

	avail_descriptors = getdtablesize() - 5;

	while (shutdown_flag == 0)
	{
		gettimeofday(&current_time, (struct timezone *) NULL);
		last_slice = update_quotas (last_slice, current_time);

		process_commands();

		// We're going to mark the decriptors for termination inside
		// the functions and then destroy them in this little
		// routine. This will save all that faffing around with
		// killing descriptors inside functions that are using them.

		// The Leech & ReaperMan Consortium (1995)

		if (descriptor_data::check_descriptors)
		{
			descriptor_data::check_descriptors = 0;
			for (d = descriptor_list; d; d = dnext)
			{
				dnext = d->next;
				switch(d->get_connect_state())
				{
				case DESCRIPTOR_LIMBO:
				case DESCRIPTOR_LIMBO_RECONNECTED:
				case DESCRIPTOR_HALFQUIT:
					d->shutdownsock ();
					break;
				default:
					// Do nothing
					break;
				}
			}
		}


		if (shutdown_flag)
			break;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		next_slice = msec_add (last_slice, COMMAND_TIME_MSEC);
		slice_timeout = timeval_sub (next_slice, current_time);

		FD_ZERO (&input_set);
		FD_ZERO (&output_set);
		if (ndescriptors < avail_descriptors)
		{
#ifdef CONCENTRATOR
			if (conc_connected)
			{
				FD_SET (conc_sock, &input_set);

				/* If we just add conc_sock regardless,
				   we never mud_time_sync() */

				if (outgoing_conc_data_waiting)
					FD_SET (conc_sock, &output_set);
			}
			else
				FD_SET (conc_listensock, &input_set);
#endif
			for(int i = 0; i < NumberListenPorts; i++)
			{
				FD_SET (ListenPorts[i], &input_set);
			}
		}
		(void) time (&now);
		for (d = descriptor_list; d; d=d->next)
		{
			if ((d->get_descriptor())	/* Don't want to watch concentrator descriptors */
			  && (!d->IS_FAKED()) /* Don't forget to exclude NPCs either */
			  && (!d->IS_HALFQUIT())) /* Or HALFQUIT people */
			{
				if (d->input.begin() != d->input.end())
					timeout = slice_timeout;
				else
					FD_SET (d->get_descriptor(), &input_set);

				if (!d->output.empty())
					FD_SET (d->get_descriptor(), &output_set);
			}
		}

		if ((select_value = select (maxd, &input_set, &output_set, NULL, &timeout)) < 0)
		{
			if (errno != EINTR)
			{
				perror ("select");
				return;
			}
		}
		else if (select_value == 0)
		{
			mud_time_sync ();
			for(d = descriptor_list; d; d=d->next)
			{
				if(d->get_connect_state() == DESCRIPTOR_UNCONNECTED)
				{
					d->set_connect_state(DESCRIPTOR_NAME);
					d->set_echo(true);
					d->welcome_user();
				}
			}
		}
		else
		{
			/* Check for new direct connection */
			for(int i = 0; i < NumberListenPorts; i++)
			{
				if (FD_ISSET (ListenPorts[i], &input_set))
				{
					if (!(newd = new_connection (ListenPorts[i])))
					{
						if (errno != EINTR && errno != EMFILE && errno != ECONNREFUSED && errno != EWOULDBLOCK && errno != ENFILE)
						{
							perror ("new_connection");
							//abort();
						}
					}
					else
					{
						if (newd->get_descriptor() >= maxd)
							maxd = newd->get_descriptor() + 1;
						newd->initial_telnet_options();
					}
				}
			}
#ifdef CONCENTRATOR
			/* Check for concentrator connect */
			if(!conc_connected)
			{
				if (FD_ISSET (conc_listensock, &input_set))
				{
					if (!(conc_sock = connect_concentrator (conc_listensock)))
					{
						perror("connect_concentrator");
						return;
					}
					if(conc_sock >= maxd)
						maxd=conc_sock+1;
					conc_connected=1;
				}
			}
			else
			{
				/* Check for concentrator traffic */
				if (FD_ISSET (conc_sock, &input_set))
				{
					/* This bit processes stuff from the concentrator */

					if (!process_concentrator_input (conc_sock))
					{
						concentrator_disconnect();
					}
				}
			}

			conc_data_sent=0;

			/* If you'd have asked me five years ago if I understand
			   this code, you probably wouldn't be alive to read it. */
#endif
			for (d = descriptor_list; d; d = dnext)
			{
				dnext = d->next;

				/* Check for direct I/O */

				if (d->get_descriptor())
				{
					if (FD_ISSET (d->get_descriptor(), &input_set))
					{
						d->last_time = now;
						if(d->warning_level > 1)
						{
							d->time_since_idle=now;
						}
						d->warning_level = 0;
						if (!d->process_input ())
						{
							if(d->terminal.halfquit)
							{
								d->HALFQUIT();
							}
							else
							{
								if(d->get_player())
									d->announce_player(ANNOUNCE_DISCONNECTED);
								d->NUKE_DESCRIPTOR ();
							}
							continue;
						}
					}
					if (FD_ISSET (d->get_descriptor(), &output_set))
					if (!d->output.empty())
						if (!d->process_output ())
						{
							if(d->get_player()) d->announce_player(ANNOUNCE_DISCONNECTED);
							d->NUKE_DESCRIPTOR ();
						}
				}

#ifdef CONCENTRATOR
				/* This is concentrator stuff - the input from the concentrator has
				   already been dealt with (check for concentrator output).
				   We don't want to reset outgoing_conc_data_waiting unless
				   we're completely sure there's nothing left, so we set
				   conc_data_sent even if nothing actually got sent this time. */

				else if (d->output.head)
				{
					conc_data_sent=1;
					if(FD_ISSET (conc_sock, &output_set))
					{
						d->last_time=now;
						if(d->warning_level > 1)
						{
							d->time_since_idle=now;
						}
						d->warning_level=0;
						if(!d->process_output ())
							log_message("Wank dick, channel %d", d->channel);
					}
				}
#endif
			}

#ifdef CONCENTRATOR
			if(!conc_data_sent)
				outgoing_conc_data_waiting=0;
#endif
		}
/*
 * Check for IDLE connections.
 *
 */
		for (d = descriptor_list; d; d = dnext)
		{
			dnext = d->next;
			if (d->IS_CONNECTED() && (d->IS_FAKED()))
				continue;

			diff = now - d->last_time;
			switch (d->warning_level)
			{
				case 0: /* 5 mins */
					if (diff > 300)
					{
						if (!d->IS_CONNECTED())
						{
							d->queue_string ("Connect timeout.\n");
							d->process_output ();
							d->NUKE_DESCRIPTOR ();
						}
						else if(d->IS_HALFQUIT())
						{
							d->announce_player(ANNOUNCE_TIMEOUT);
							d->NUKE_DESCRIPTOR();
						}
						else
							d->warning_level ++;
					}
					break;
			}
		}
	}
}

#ifdef CONCENTRATOR
SOCKET connect_concentrator(SOCKET sock)
{
	struct	sockaddr_in	addr;
	int			addr_len;
	SOCKET			newsock;

	log_message("CONCENTRATOR: connected");

	addr_len=sizeof(addr);
	newsock=accept(sock, (struct sockaddr *) &addr, &addr_len);

	return newsock;
}
#endif

struct descriptor_data *new_connection(SOCKET sock)
{
	SOCKET			newsock;
	struct	sockaddr_in	addr;
#if defined (linux) || defined (__FreeBSD__)
	socklen_t		addr_len;
#else
	int			addr_len;
#endif /* defined (linux) */
	u_long			host_addr;

	addr_len = sizeof (addr);
	newsock = accept (sock, (struct sockaddr *) &addr, &addr_len);
	if (newsock < 0)
		return 0;
	host_addr = ntohl (addr.sin_addr.s_addr);

	if (is_banned (host_addr))
	{
		int i;
		int len = sizeof(reject_message);
		int offset = 0;
		do
		{
			i = write (newsock, reject_message+offset, len);
			if(i < 0)
			{
				if(errno == EWOULDBLOCK)
					continue;
				break;
			}
			offset+= i;
			len -=i;
		} while(len > 0);
		shutdown (newsock, 2);
		closesocket (newsock);
		errno = ECONNREFUSED;
		return 0;
	}

	int one = 1;
	if(setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, SETSOCKOPT_OPTCAST &one, sizeof(one)) != 0)
	{
		log_bug("KeepAlive option not set, errno=%d", errno);
	}
	log_accept(newsock, ntohs (addr.sin_port), convert_addr (addr.sin_addr.s_addr));
	return new descriptor_data (newsock, &addr);
}

void
descriptor_data::send_telnet_option(unsigned char command, unsigned char option)
{
	char sendbuf[3];

	if(IS_FAKED())
		return;

	sendbuf[0]=IAC;
	sendbuf[1]=command;
	sendbuf[2]=option;

#ifdef DEBUG_TELNET
	log_debug("Sent option '%s %s'", TELCMD(command), TELOPT(option));
#endif /* DEBUG_TELNET */
	write(get_descriptor(), sendbuf, 3);
}

int
descriptor_data::process_telnet_options(unsigned char *src, int length)
{
	unsigned char* dst = src;
	unsigned char* end = src + length;
	int size = 0;
	/* Look through it for telnet options */

	while(src < end)
	{
		size += __process_telnet_options(src, dst, end);
	}

	return size;
}

int
descriptor_data::__process_telnet_options(unsigned char*&buf, unsigned char*& dst, unsigned char* end)
{
int j;
unsigned char posreply, negreply;
int count = 0;
/* We may already be in the middle of a telnet option.
 * NOTE: The top-level switch is written so that it falls through
 *       all the different cases in turn. This will (hopefully)
 *       speed up the state machine so it doesn't have to return
 *       the the while() loop above once it has got enough data
 *       for each stage.
 */
	switch(t_iac)
	{
	case IAC_GOT_FIRST_IAC:
		_got_an_iac = true;	// We got an iac, we can now sort out echo handling properly.
		t_iac_command = *(buf++);
		t_iac = IAC_GOT_COMMAND;
		if(buf >= end)
		{
			return 0;
		}
	case IAC_GOT_COMMAND:
		t_iac_option = *(buf++);
#ifdef DEBUG_TELNET
		log_debug("Got option '%s %s'", TELCMD(t_iac_command), TELOPT(t_iac_option));
#endif /* DEBUG_TELNET */

		switch(t_iac_command)
		{
		case DO: /* Is this needed? */
		case WILL:
			if(t_iac_command == DO)
			{
				negreply=WONT;
				posreply=WILL;
			}
			else
			{
				negreply=DONT;
				posreply=DO;
			}

			t_iac = IAC_OUTSIDE_IAC;

			for(j=0; iac_we_use[j]!=-1; j++)
				if(iac_we_use[j]==t_iac_option)
					break;

// Automatic wont/dont for options we don't support
			if(iac_we_use[j] == -1)
			{
				send_telnet_option(negreply, t_iac_option);
				return 0;
			}

			switch(t_iac_option)
			{
			case TELOPT_LINEMODE:
// Client will do LINEMODE! We're in luck, as we can now negotiate
// using LINEMODE stuff in stead of normal stuff.
				t_linemode = 1;
				break;
			case TELOPT_LFLOW:
				t_lflow = 1;
				t_echo = false;
				break;
			}
			return 0; // We've handled a telnet option

		case WONT:
// Client wont do LINEMODE! We're out of luck, so provide standard support
// but nothing fancy. If the client end can't cope with this, then tough,
// as we don't want the hassle of providing full line editing functions.
			switch(t_iac_option)
			{
			case TELOPT_LINEMODE:
				t_linemode = 0;
				send_telnet_option(WILL, TELOPT_SGA);
				send_telnet_option(WILL, TELOPT_ECHO);
				break;
			case TELOPT_LFLOW:
// Client wont LFLOW for us, but it may still work in line-at-a-time mode.
// If t_lflow == 2, then we need to check the real data from the client,
// which if the system only does char-at-a-time will probably be only
// one or two chars.
				t_lflow = 2;
// We will enable echo as soon as we know it is char-at-a-time mode we
// need.
				t_echo = false;
				break;
			}
			t_iac = IAC_OUTSIDE_IAC;
			return 0; // We've handled the telnet option.
		case SB:
			t_iac = IAC_INSIDE_IAC;
			t_iacbuf = (unsigned char*)malloc(128);
			t_piacbuf = 0;
			t_liacbuf = 128;
			if(buf >= end)
			{
				// We're in the middle of an IAC,
				// but run out of data.
				return 0;
			}
			break;
		default:
			t_iac = IAC_OUTSIDE_IAC;
			return 0;
		}
	case IAC_INSIDE_IAC:
	case IAC_GOT_ANOTHER_IAC:
		{
			int endit=0;
			unsigned char c = 0;
			while((buf<end) && !endit)
			{
				c = *(buf++);
				if(t_iac == IAC_GOT_ANOTHER_IAC)
				{
					switch(c)
					{
					case IAC:
						t_iacbuf[t_piacbuf++] = IAC;
						t_iac = IAC_INSIDE_IAC;
						break;
					default:
						endit = 1;
						t_iac = IAC_OUTSIDE_IAC;
					}
				}
				else if(c == IAC)
				{
					t_iac = IAC_GOT_ANOTHER_IAC;
				}
				else
				{
					t_iacbuf[t_piacbuf++] = c;
				}
				if(t_piacbuf >= t_liacbuf)
				{
					t_liacbuf += 128;
					unsigned char* tmp = (unsigned char*)malloc(t_liacbuf);
					memcpy(tmp, t_iacbuf, t_piacbuf);
					free(t_iacbuf);
					t_iacbuf = tmp;
				}
			}
			if(t_iac != IAC_OUTSIDE_IAC)
			{
				return 0;
			}
			if(c!=SE)
				log_debug("Telnet option warning, descriptor %d:  %x isn't IAC SE", get_descriptor(), c);
			t_iacbuf[t_piacbuf]=0;
#ifdef DEBUG_TELNET
			log_debug("Got command '%s' for '%s'", TELCMD(t_iac_command), TELOPT(t_iac_option));
#endif /* DEBUG_TELNET */
			get_value_from_subnegotiation(t_iacbuf, t_iac_option, t_piacbuf);
			free(t_iacbuf); t_iacbuf = 0;
			t_iac = IAC_OUTSIDE_IAC;
		}
	case IAC_OUTSIDE_IAC:
		{
			unsigned char c = 0;
			while((buf < end) && ((c = *(buf++)) != IAC))
			{
				*(dst++) = c;
				count ++;
			}
			if(c == IAC)
			{
				t_iac = IAC_GOT_FIRST_IAC;
			}
			return count;
		}
	}
	return 0;
}


void
descriptor_data::initial_telnet_options()
{
	int i;
	static char buf[20];

	/* First send the things we want the client to DO. */

	for(i=0; iac_initial[i].command; i++)
		send_telnet_option(iac_initial[i].command, iac_initial[i].option);

	/* Now ask for the terminal type... */

	buf[0]=IAC;
	buf[1]=SB;
	buf[2]=TELOPT_TTYPE;
	buf[3]=TELQUAL_SEND;
	buf[4]=IAC;
	buf[5]=SE;
	buf[6]=0;
	write(get_descriptor(), buf, 6);

	/* Any replies will go to process_telnet_options() */
}

void
descriptor_data::set_echo(bool echo)
{
static char buf[20];
	if(IS_FAKED())
		return;

	if(t_linemode)
	{
		buf[0] = IAC;
		buf[1] = SB;
		buf[2] = TELOPT_LINEMODE;
		buf[3] = 1; // MODE
		if(echo)
			buf[4] = 1 | 16;
		else
			buf[4] = 1;
		buf[5] = IAC;
		buf[6] = SE;
		buf[7] = 0;
		write(get_descriptor(), buf, 7);
	}
/*
 * This is a crude attempt to start/stop local echo on the client side.
 * It is *NOT* guaranteed to work as we intend, as we are not using it
 * for the use RFC 857 intends.
 *
 * The protocol is only talking about what the remote end will echo,
 * NOT what the local end should echo to the user.
 *
 * Fortunately, most telnet clients are happy to assume that if we're going
 * to echo any characters send then they don't have to, and if we're not
 * then they should echo them. This is what is *SUGGESTED* in RFC 857.
 */
	if(!t_lflow)
		t_echo = echo;
	if(echo)
	{
		if(t_lflow)
		{
// Unknown why we need to send LFLOW, but it makes Linux clients work
			send_telnet_option(DO, TELOPT_LFLOW);
			send_telnet_option(WONT, TELOPT_ECHO);
		}
		queue_string("\r\n");
	}
	else if(t_lflow)
	{
		send_telnet_option(DO, TELOPT_LFLOW);
		send_telnet_option(WILL, TELOPT_ECHO);
	}
}

void
descriptor_data::get_value_from_subnegotiation(unsigned char *buf, unsigned char option, int size)
{
	switch(option)
	{
		case TELOPT_NAWS:
			terminal.width=buf[0]*256 + buf[1];
			terminal.height=buf[2]*256 + buf[3];
#ifdef DEBUG_TELNET
			log_debug("Descriptor %d terminal dimensions:  %d x %d", get_descriptor(), terminal.width, terminal.height);
#endif
			if(terminal.width < 20 || terminal.height < 5)
			{
				log_bug("Descriptor %d gave silly terminal dimensions (%d x %d)", get_descriptor(), terminal.width, terminal.height);
				terminal.width=0;
				terminal.height=0;
			}
			send_telnet_option(WONT, TELOPT_NAWS);	/* To make sure we don't have to send it */
			break;

		case TELOPT_TTYPE:
			memcpy(scratch, buf+1, size);
			scratch[size]=0;
#ifdef DEBUG_TELNET
			log_debug("Descriptor %d terminal type is '%s'", get_descriptor(), scratch);
#endif
			set_terminal_type(scratch);
			send_telnet_option(WONT, TELOPT_TTYPE);	/* To make sure we don't have to send it */
			break;
		
		case TELOPT_SNDLOC:
			if(ntohl(address) != LOGTHROUGH_HOST)
			{
				log_bug("Descriptor %d sent a SNDLOC, but isn't connected from LOGTHROUGH_HOST", get_descriptor());
				break;
			}
			memcpy(scratch, buf, size);
			scratch[size]=0;
#ifdef DEBUG_TELNET
			log_debug("Descriptor %d location is '%s'", get_descriptor(), scratch);
#endif
			service = "Redirect";
			hostname = scratch;
			send_telnet_option(WONT, TELOPT_SNDLOC);
			break;
	}
}

#if	HAS_CURSES
#ifdef USE_TERMINFO
/*
 * I've done a blatant cut and paste of this code for the other version so if
 * you update one you should check the other.  I was going to go through having
 * lots of ifdefs so it was more obviously how things tied up but it made the
 * totally unreadable.  -Abstract
 */
bool descriptor_data::set_terminal_type (const String& termtype)
{
	char *term;

	term = strdup(termtype.c_str());

	for(int i = 0; term[i]; i++)
		term[i] = tolower(term[i]);

	int setupterm_error;
	if (setupterm (term, get_descriptor(), &setupterm_error) != OK)
	{
		if (setupterm_error == -1)
			log_bug("Terminfo database could not be found.");
#ifdef DEBUG_TELNET
		else if (setupterm_error == 0)
			log_bug("Terminal type '%s' from descriptor %d not in the terminfo database", term, get_descriptor());
#endif
		FREE(term);
		return false;
	}

	terminal.type = term;
	FREE(term);

	if (terminal.width == 0)
	{
		terminal.width = tigetnum(const_cast<char *>("cols"));
#ifdef DEBUG_TELNET
		log_debug("Terminal width (from terminfo):  %d", terminal.width);
#endif
	}
	if (terminal.height == 0)
	{
		terminal.height = tigetnum(const_cast<char *>("lines"));
#ifdef DEBUG_TELNET
		log_debug("Terminal height (from terminfo):  %d", terminal.height);
#endif
	}

	const char *const bold = tigetstr(const_cast<char *>("bold"));
	if (bold != (char *)-1)
	{
		termcap.bold_on = bold;
	}

	const char *const bold_off = tigetstr(const_cast<char *>("sgr0"));
	if (bold_off != (char *)-1)
	{
		termcap.bold_off = bold_off;
	}

	const char *const underline = tigetstr(const_cast<char *>("smul"));
	if (underline != (char *)-1)
	{
		termcap.underscore_on = underline;
	}

	const char *const underscore_off = tigetstr(const_cast<char *>("rmul"));
	if (underscore_off != (char *)-1)
	{
		termcap.underscore_off = scratch;
	}

	return true;
}

#else /* !USE_TERMINFO */
/*
 * If you update this you should also check the version above!
 */

bool
descriptor_data::set_terminal_type(const String& termtype)
{
	static char ltermcap[1024];
	char *terminal, *area;
	int i;

	terminal=strdup(termtype);

	for(i=0; terminal[i]; i++)
		terminal[i]=tolower(terminal[i]);

	if(tgetent(ltermcap, terminal)!=1)
	{
#ifdef DEBUG_TELNET
		log_bug("Terminal type '%s' from descriptor %d not in /etc/termcap", terminal, get_descriptor());
#endif
		FREE(terminal);
		return false;
	}

	terminal.type=terminal;
	FREE(terminal);

	if(terminal.width == 0)
	{
		terminal.width=tgetnum("co");
#ifdef DEBUG_TELNET
	log_debug("Terminal width (from termcap):  %d", terminal.width);
#endif
	}
	if(terminal.height == 0)
	{
		terminal.height=tgetnum("li");
#ifdef DEBUG_TELNET
	log_debug("Terminal height (from termcap):  %d", terminal.height);
#endif
	}

	area=scratch;
	if(tgetstr("md", &area))
	{
		termcap.bold_on=scratch;
	}

	area=scratch;
	if(tgetstr("me", &area))
	{
		termcap.bold_off=scratch;
	}

	area=scratch;
	if(tgetstr("us", &area))
	{
		termcap.underscore_on=scratch;
	}

	area=scratch;
	if(tgetstr("ue", &area))
	{
		termcap.underscore_off=scratch;
	}

	return true;
}

#endif /* USE_TERMINFO */
#else	/* !HAS_CURSES */
/*
 * The version for those who don't have curses or termcap available.
 */

bool
descriptor_data::set_terminal_type(const String& termtype)
{
	// It really doesn't get much simpler.
	return true;
}

#endif	/* !HAS_CURSES */

#define CACHE_ADDR_SIZE 4096

struct cached_addr
{
	cached_addr() : addr(0), name(), count(0), lastused(0) {}
	u_long	addr;
	String	name;
	u_long	count;
	time_t	lastused;
};
cached_addr cached_addresses[CACHE_ADDR_SIZE];

bool
get_cached_addr(u_long addr, char* name)
{
	int i;
	for(i = 0; i < CACHE_ADDR_SIZE; i++)
	{
		if(cached_addresses[i].addr == addr)
		{
			strcpy(name, cached_addresses[i].name.c_str());
			cached_addresses[i].count++;
			time(&(cached_addresses[i].lastused));
			return true;
		}
	}
	return false;
}

void
set_cached_addr(u_long addr, const String& name)
{
	int i;
	time_t mintime = 0x7fffffff;
	u_long mincount = 0xffffffff;
	u_long bestindex = 0;
	for(i = 0; i < CACHE_ADDR_SIZE; i++)
	{
		if(cached_addresses[i].addr == 0)
		{
			cached_addresses[i].addr = addr;
			cached_addresses[i].name = name;
			cached_addresses[i].count = 1;
			time(&(cached_addresses[i].lastused));
			return;
		}
		if((cached_addresses[i].count < mincount)
			|| ((cached_addresses[i].count == mincount)
				&& (cached_addresses[i].lastused < mintime)))
		{
			mincount = cached_addresses[i].count;
			mintime = cached_addresses[i].lastused;
			bestindex = i;
		}
	}
	cached_addresses[bestindex].addr = addr;
	cached_addresses[bestindex].name = name;
	cached_addresses[bestindex].count = 1;
	time(&(cached_addresses[bestindex].lastused));
}

const char *
convert_addr (
unsigned long addr)

{
	struct	hostent	*he;
	static	char	buffer [MAX(20, MAXHOSTNAMELEN)];
	char		compare_buffer [MAXHOSTNAMELEN];

	if(get_cached_addr(addr, buffer))
	{
		return buffer;
	}
	if (smd_dnslookup (addr))
	{
		if((he = gethostbyaddr ((char *) &addr, sizeof(addr), AF_INET)) != NULL)
		{
			char	*pos;

			strcpy (buffer, he->h_name);
			strcpy (compare_buffer, DOMAIN_STRING);
			while (strchr (compare_buffer, '.') != NULL)
			{
				if ((pos = strstr (buffer, compare_buffer)) != NULL)
				{
					*pos = '\0';
					break;
				}
				else if ((pos = strchr (compare_buffer + 1, '.')) == NULL)
					break;
				else
					strcpy (compare_buffer, pos);
			}
		}
		else
		{
			long bsaddr = ntohl(addr);
			snprintf (buffer, sizeof(buffer), "%ld.%ld.%ld.%ld", (bsaddr >> 24) & 0xff, (bsaddr >> 16) & 0xff, (bsaddr >> 8) & 0xff, bsaddr & 0xff);
		}
		set_cached_addr(addr, buffer);
	}
	else
	{
		/* Not local, or not found in db */
		long bsaddr = ntohl(addr);
		snprintf (buffer, sizeof(buffer), "%ld.%ld.%ld.%ld", (bsaddr >> 24) & 0xff, (bsaddr >> 16) & 0xff, (bsaddr >> 8) & 0xff, bsaddr & 0xff);
	}
	return (buffer);
}

void
descriptor_data::shutdownsock()
{
	time_t stamp;
	struct tm *now;
/* For NPC connections, ignore most of this */
	if(!IS_FAKED() && (get_connect_state() != DESCRIPTOR_LIMBO_RECONNECTED) && !IS_HALFQUIT())
	{
		if (get_player() != 0)
		{
			mud_disconnect_player (get_player());
			time (&stamp);
			now = localtime (&stamp);
			log_disconnect(get_player(), getname(get_player()), CHANNEL(), channel, NULLSTRING, true);
		}
		else
		{
			log_disconnect(0, NULLSTRING, CHANNEL(), 0, NULLSTRING, false);
		}
	}
	process_output ();
	if(get_descriptor())	/* If it's not a concentrator connection */
	{
		shutdown (get_descriptor(), 2);
		closesocket (get_descriptor());
		_descriptor = 0; // Clear out the descriptor
	}
#ifdef CONCENTRATOR
	else if(!IS_FAKED())
	{
		struct conc_message msg;

		msg.type=CONC_DISCONNECT;
		msg.len=0;
		msg.channel=channel;
		write(conc_sock, &msg, sizeof(msg));
	}
#endif

	freeqs ();

	if(!IS_HALFQUIT())
	{
		delete this;
	}
	else
	{
		log_halfquit(get_player(), getname(get_player()), CHANNEL(), channel);
	}
}


descriptor_data::~descriptor_data()
{
	*prev = next;
	if (next)
		next->prev = prev;
	ndescriptors--;
}

/* Call with (socket, address) for normal connections, or (0, NULL, channel) for concentrator connections. */


descriptor_data::descriptor_data (
SOCKET			s,
struct	sockaddr_in	*a,
int			chanl)
:
	_descriptor(s),
	_player(0),
	_player_name(),
	_password(),
	_connect_state(DESCRIPTOR_UNCONNECTED),
	_output_prefix(),
	_output_suffix(),
	connect_attempts(3),
	output(),
	input(),
	raw_input(NULL),
	raw_input_at(NULL), // Gets reset in the constructor.
	start_time(time(0)),
	last_time(start_time),
	time_since_idle(start_time),
	warning_level(0),
	quota(COMMAND_BURST_SIZE),
	backslash_pending(false),
	cr_pending(0),
	hostname(),
	address(0),
	service(),
	next(descriptor_list),
	prev(&descriptor_list),
	termcap(),
	terminal(),
	channel(chanl),
	myoutput(false),
	t_echo(false),
	t_lflow(0),
	t_linemode(0),
	t_iac(IAC_OUTSIDE_IAC),
	t_iacbuf(0),
	t_piacbuf(0),
	t_liacbuf(0),
	t_iac_command(0),
	t_iac_option(0),
	_got_an_iac(false)
{
	ndescriptors++;
	if(s) make_nonblocking (s);

// Do processing depending on if this is an NPC or a real connection
	if(s == 0 && a == NULL & channel == 0)
	{
		set_connect_state(DESCRIPTOR_FAKED);
		MALLOC(raw_input, unsigned char, MAX_COMMAND_LEN + 1);
	}
	else
	{
		set_connect_state(DESCRIPTOR_UNCONNECTED);
	}
	raw_input_at		= raw_input;

	if(a)
	{
	struct sockaddr_in	tmpname;
	socklen_t		tmpnamelen;
	u_short                 local_port;
	char			tmpstring[7]; /* max num=65535=>5 digits+1 */


		tmpnamelen=sizeof(tmpname);
		getsockname(s,(struct sockaddr *)(&tmpname),&tmpnamelen);
		local_port = tmpname.sin_port;
		switch (local_port)
		{
			case 2323:
				service="TelnetGW";
				break;
			case 1394:
				service="Robot";
				break;
			case TINYPORT:
				service="Main";
				break;
			case 8080:
				service="WebGW";
				break;
			case 8181:
				service="JavaWeb";
				terminal.colour_terminal=false;
				break;
			default:
				sprintf(tmpstring,"%d",htons(local_port));
				service=tmpstring;
				break;
		}
		address = a->sin_addr.s_addr;
		hostname = convert_addr(address);
        }
        else
		hostname = "Concentrator";

        if (descriptor_list)
                descriptor_list->prev = &next;

	descriptor_list		= this;

	set_echo(true);
}


SOCKET make_socket(int port, unsigned long inaddr)
{
	SOCKET s;
	struct sockaddr_in server;

	s = socket (AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		perror ("creating stream socket");
		exit (3);
	}
	int one = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, SETSOCKOPT_OPTCAST &one, sizeof(one));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inaddr;
	server.sin_port = htons (port);
	if (bind (s, (struct sockaddr *) & server, sizeof (server)))
	{
		perror ("binding stream socket");
		closesocket (s);
		exit (4);
	}
	listen (s, 5);
	return s;
}

int
text_buffer::flush(unsigned int min_to_drop)
{
	min_to_drop += strlen (flushed_message);
	if(min_to_drop > m_data.length())
	{
		min_to_drop = m_data.length();
	}
	String tmp(flushed_message);
	m_data = tmp + (m_data.c_str() + min_to_drop);

	return min_to_drop;
}

int
text_buffer::write(int fd)
{
int written;
int towrite = m_data.length();
const char* data = m_data.c_str();
	do
	{
		written = ::write(fd, (const void*)data, towrite);
		if(written < 0)
		{
			String tmp = m_data;
			m_data = String(data, towrite);
			return written;
		}
		towrite -= written;
		data += written;
	}
	while(towrite > 0);

	m_data = NULLSTRING;
	return 1;
}

int
descriptor_data::queue_write(const char *b, int n)
{
	int space;
	const char *buf = b;

	if(IS_FAKED())
		return n;

	if(!terminal.noflush)
	{
		space = MAX_OUTPUT - output.size() - n;
		if (space < 0)
			output.flush(-space);
	}
	if(terminal.sevenbit)
	{
		for (int i=0; i<n; i++)
			scratch_buffer[i] = ((unsigned char) b[i] >= 0x7f) ? terminal.sevenbit : b[i];
		buf = scratch_buffer;
	}
	output.add(buf, n);

	if (!get_descriptor())
		outgoing_conc_data_waiting = 1;

	return n;
}

/* Note to stop this from substituting colours
   (eg who strings), call queue_string(s,1).
   To get colour, just use as normal, queue_string(s)
	- Dunk 3/2/96
*/
/* Note. Abandon all hope. Reaps is on the coding trail.
   Have added extra layer below queue_string(const char *s, int show_literally)
   so that the @recall command can happily pump information
   straight out from the recall buffer without it being
   put back into the recall buffer on the way through. */
/* Note. Grimthorpe is to the rescue.
   queue_string2 is no more, because Reaps doesn't know you can have more
   than one default value... */

int
descriptor_data::queue_string(const char *s, int show_literally, int store_in_recall_buffer)
{
static char b1[2*BUFFER_LEN];
static char b2[2*BUFFER_LEN];
static char OUTPUT_COMMAND[] = ".output ";
static char MYOUTPUT_COMMAND[] = ".myoutput ";
char *a,*a1,*b;

	if(get_connect_state()==DESCRIPTOR_LIMBO)
	{
		log_bug("WARNING: Attempt to queue_string to a limbo'd descriptor");
		return 0;
	}
	else if(IS_FAKED()) // Output text to a NPC
	{
		while(*s != '\0')
		{
			switch(*s)
			{
				case '\n':
					s++;
					*raw_input_at = 0;
					if(raw_input_at == raw_input)
						break;
					raw_input_at = raw_input;
					if(myoutput)
						strcpy(b1, MYOUTPUT_COMMAND);
					else
						strcpy(b1, OUTPUT_COMMAND);
					strcat(b1, (char *)raw_input);
					time(&last_time);
					save_command(b1);
					*raw_input_at = 0;
					break;
				case '\0':
					return 1;
				case '$':
				case '{':
				case '\\':
					*(raw_input_at++) = '\\';
					// Fallthrough
				default:
					*(raw_input_at++) = *(s++);
					break;
			}
		}
		return 1;
	}

        if(get_player())
        {
		if(store_in_recall_buffer)
		{
			if(_last_command_count != LastCommandCount)
			{
				_last_command_count = LastCommandCount;
				char temp[1024];
				snprintf(temp, 1024, "\001(%s)\002(#%d)\003", LastCommandName.c_str(), LastCommandCaller);
				queue_string(temp, show_literally, store_in_recall_buffer);
			}
			if(terminal.recall)
			{
				db[get_player()].add_recall_line(s);
			}
		}
        }

	if(IS_HALFQUIT())
	{
		return 1;
	}
	strcpy(b2, s);
	b=b2;

// Do word-wrap.
// We want to wrap at the last <space> before the end-of-line. If there isn't
// one, don't try to wrap.
	if((terminal.width > 0) && (terminal.wrap))
	{
		int percent_primed= 0; /* Take acct of %colour *not* affecting wrap */
		char* last_space = 0; /* The last place we saw a space. */
		a = b;
		while (*a)
		{
			switch (*a)
			{
			case 0:
				break;

			case 1:	// Start of command info
				while((*a) && (*a != 3))
					a++;
				if(*a)
					a++;
				continue;
				break;

			case '\n':
				terminal.xpos = 0; // We increment the position
				last_space = 0;
				a++;
				continue;
				break;

			case '\033': /* ANSI code. But why would we get one here? */
				while ((*a) && (*a!='m'))
					a++;
				if(*a)
				{
					a++;
				}
				continue;
				break;

			case '%':
				terminal.xpos--;
				percent_primed = !percent_primed;
				break;

			case ' ':
				if(!percent_primed)
					last_space = a;
				// FALLTHROUGH

			default:
				if (percent_primed)
				{
					percent_primed= 0;
					a++;
					continue;
				}
				break;
			}

			if(!*a)
			{
				break;
			}
			if(terminal.xpos+2 >= terminal.width)
			{
				if(last_space != 0)
				{
					a = last_space;
					last_space = 0;
					*a++ = '\n';
				}
				terminal.xpos = 0;
				a++;
			}
			else
			{
				terminal.xpos++;
				a++;
			}
		}
	}
	if(terminal.xpos < 0)
		terminal.xpos = 0;
	if(terminal.lftocr)
	{
		/* I apologise for the messyness of the next bit, but it is to implement */
		/* LF to LF/CR conversion (for Jimbo) */
		strcpy(b1,b);
		a=b;
		a1=b1;
		while((*a++ = *a1))
		{
			if(*a1++ == '\n')
				*a++='\r';
		}
	}

	String output;

	int colour = 0;
	if(get_player())
		colour = Colour(get_player());  /* Stores whether we need to substitute in the codes */
	int percent_loaded = 0;		/* Indicates whether we have already hit a % sign */

	while(*b)
	{
		char chr = *(b++);
		switch(chr)
		{
		case '%':
			if((percent_loaded) || (show_literally))
			{
				/* Deal with %% which displays a % sign */
				output += '%';
				percent_loaded = 0;
			}
			else
			{
				percent_loaded = 1;
			}
			break;

		case '\001':
			if(!terminal.emit_lastcommand)
			{
				do
				{
					b++;
				}
				while((*b) && (*b != 3));
				if(*b)
					b++;
				continue;
			}

		default:
			if(percent_loaded)
			{
				percent_loaded = 0;
				if((colour || terminal.effects) && terminal.colour_terminal)
				{
					/* They want the colour codes transfered*/

					if((chr >= 'A') && (chr <= '~'))
					{
						/* Find the entry in the colour table by
						   directly referencing it. 'A' is 0 */
						colour_table_type& entry = colour_table[(chr - 65)];
						if(colour || entry.is_effect)
						{
							output += entry.ansi;
						}
					}
				}
			}
			else
			{
				/* Just a character, copy it */
				output += chr;
			}
		}
	}
	return queue_write (output.c_str(), output.length());
}

int
descriptor_data::process_output()
{
	int cnt;

	if(IS_FAKED() || IS_HALFQUIT())
	{
		cnt = output.size();
	}
	else if (get_descriptor())
	{
		if(output.write(get_descriptor()) < 0)
		{
			if (errno == EWOULDBLOCK)
				return 1;

			return 0;
		}
	}
#ifdef CONCENTRATOR
*** BUG ME *** (Grimthorpe) Disabled at the moment; nobody uses it though.
	else
	{
		if ((cnt = send_concentrator_data (channel, cur->start, cur->nchars))<1)
		{
			log_bug("Concentrator disconnect in write() (process_output)");
			return 0;
		}
	}
#endif

	return 1;
}

void make_nonblocking(int s)
{
#if	USE_BSD_SOCKETS
#	ifdef FNDELAY
		if (fcntl (s, F_SETFL, FNDELAY) == -1)
#	else
		if (fcntl (s, F_SETFL, O_NDELAY) == -1)
#	endif
		{
			perror ("make_nonblocking: fcntl");
			panic ("FNDELAY fcntl failed");
		}
#endif	/* USE_BSD_SOCKETS */

#if	USE_WINSOCK2
	u_long one = 1;
	if (ioctlsocket (s, FIONBIO, &one))
		fprintf (stderr, "ioctlsocket error: %d.\n", WSAGetLastError ());	// TODO: Bug log.
#endif	/* USE_WINSOCK2 */
}

void
descriptor_data::freeqs()
{
	output.clear();

	while(!input.empty())
	{
		input.pop_front();
	}

	if (raw_input)
		FREE (raw_input);
	raw_input = NULL;
	raw_input_at = NULL;
}

void
descriptor_data::welcome_user()
{
	splat_motd();
	queue_string (name_prompt);
}

void
descriptor_data::splat_motd()
{
	FILE	*f;

	if((f = fopen(WELCOME_FILE, "r")) == NULL)
	{
		log_bug("file not found: %s", WELCOME_FILE);
		queue_string (WELCOME_MESSAGE);
	}
	else
	{
		while(fgets(scratch, BUFFER_LEN, f))
			queue_string (scratch);
		fclose(f);
	}
}


char *boldify(dbref player, const char *str)
{
	static char buf[BUFFER_LEN];
	struct descriptor_data *d;

	for (d=descriptor_list; d; d=d->next)
		if(d->get_player()==player)
			break;

	if(!d)
		return NULL;

	if(d->termcap.bold_on && d->termcap.bold_off && d->terminal.type)
		snprintf(buf, sizeof(buf), "%s%s%s", d->termcap.bold_on.c_str(), str, d->termcap.bold_off.c_str());
	else
		strcpy(buf, str);

	return buf;
}


char *underscorify(dbref player, const char *str)
{
	static char buf[BUFFER_LEN];
	struct descriptor_data *d;

	for (d=descriptor_list; d; d=d->next)
		if(d->get_player()==player)
			break;

	if(!d)
		return NULL;

//	if(d->termcap.underscore_on && d->termcap.underscore_off && d->terminal.type)
//		snprintf(buf, sizeof(buf), "%s%s%s", d->termcap.underscore_on.c_str(), str, d->termcap.underscore_off.c_str());
//	else
		strcpy(buf, str);

	return buf;
}


void
descriptor_data::announce_player (announce_states state)
{
	struct descriptor_data *d;

	char	wizard_string[1024];
	const char*	app_string;
	const char*	mortal_string;

	bool deruded=false;
	const char *censored=NULL;

	if(db[get_player()].get_flag(FLAG_DONTANNOUNCE))
	{
		// If the wizards don't want anyone to hear this...
		return;
	}

	/*
	 * Note: In order to make the colour stuff easier to look at, the '[<playername>' bit is done further down.
	 */
	switch (state)
	{
		case ANNOUNCE_CONNECTED :
			mortal_string = " has connected]\n";
			app_string = mortal_string;
			snprintf (wizard_string, sizeof(wizard_string), " has connected from %s]\n", hostname.c_str());
			break;
		case ANNOUNCE_CREATED :
			mortal_string = " has connected]\n";
			app_string = " has been created]\n";
			snprintf (wizard_string, sizeof(wizard_string), " has been created from %s]\n", hostname.c_str());
			break;
		case ANNOUNCE_BOOTED :
			snprintf (scratch, sizeof(scratch), " has been booted by %s%s%s]\n", player_booting, blank (boot_reason)?"":" ", boot_reason);
			mortal_string = scratch;
			app_string = mortal_string;
			snprintf (wizard_string, sizeof(wizard_string), " has been booted from %s by %s%s%s]\n", hostname.c_str(), player_booting, blank (boot_reason)?"":" ", boot_reason);
			break;
		case ANNOUNCE_DISCONNECTED :
			mortal_string = " has disconnected]\n";
			app_string = mortal_string;
			snprintf (wizard_string, sizeof(wizard_string), " has disconnected from %s]\n", hostname.c_str());
			break;
		case ANNOUNCE_SMD :
			mortal_string = " has disconnected]\n";
			app_string = mortal_string;
			snprintf (wizard_string, sizeof(wizard_string), " has disconnected from %s due to an SMD read]\n", hostname.c_str());
			break;
		case ANNOUNCE_TIMEOUT :
			mortal_string = " has disconnected]\n";
			app_string = " has timed out]\n";
			snprintf (wizard_string, sizeof(wizard_string), " has timed out from %s]\n", hostname.c_str());
			break;
		case ANNOUNCE_PURGE :
			mortal_string = " has purged an idle connection]\n";
			app_string = mortal_string;
			snprintf (wizard_string, sizeof(wizard_string), " has purged an idle connection from %s]\n", hostname.c_str());
			break;
		case ANNOUNCE_RECONNECT:
			mortal_string = " has reconnected]\n";
			app_string = mortal_string;
			snprintf (wizard_string, sizeof(wizard_string), " has reconnected from %s]\n", hostname.c_str());
			break;
		default :
			log_bug("Unknown state (%d) encounted in announce_player()", state);
			break;
	}

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->IS_CONNECTED () && Listen (d->get_player()))
		{
			d->queue_string("[");
			d->queue_string(player_colour(d->get_player(), get_player(), rank_colour(get_player())));
			d->queue_string(db[get_player()].get_name().c_str());
			d->queue_string(COLOUR_REVERT);
			if (Wizard (d->get_player()))
			{
				d->queue_string (underscorify (d->get_player(), wizard_string));
			}
			else if ( Apprentice(d->get_player()) || Welcomer (d->get_player()))
			{
				d->queue_string (underscorify (d->get_player(), app_string));
			}
			else if (Censorall(d->get_player()) || Censorpublic(d->get_player()))
			{
				if (deruded==false)
				{
					deruded=true;
					censored=censor(mortal_string);
				}
				d->queue_string (underscorify (d->get_player(), censored));
			}
			else
			{
				d->queue_string (underscorify (d->get_player(), mortal_string));
			}
		}
	}
}


int
descriptor_data::process_input (int len)
{
	int	got;
	unsigned char	*p, *pend, *q, *qend;
	static	unsigned char	read_buffer[8192];

	/* See what's waiting */
	if (get_descriptor())
		got = read (get_descriptor(), read_buffer, BUFFER_LEN);
#ifdef CONCENTRATOR
	else
		got = read (conc_sock, read_buffer, len);
#else
	len = len;	// Get rid of a warning that len isn't used.
#endif

	/* See if we got anything */
	if (got <= 0)
		return 0;

	/* Process the input for telnet options */
	got=process_telnet_options(read_buffer, got);

// We've given the telnet client a chance to send terminal stuff, etc.
// So now we'll welcome them if they're not already connected.
	if(get_connect_state() == DESCRIPTOR_UNCONNECTED)
	{
		set_echo(true);
		welcome_user();
		set_connect_state(DESCRIPTOR_NAME);
	}

	/* If we don't have any unprocessed input buffer, allocate one */
	if (!raw_input)
	{
		MALLOC(raw_input,unsigned char,MAX_COMMAND_LEN);
		raw_input_at = raw_input;
	}

	/* Remember where we are (p) and where the end of the buffer is (pend) */
	p = raw_input_at;
	pend = raw_input + MAX_COMMAND_LEN - 1;

	unsigned char *x = 0;

	int charcount = 0; // Count the chars as they come out
	int gotline = 0; // See if we've got and end-of-line marker

	/* Look through for ends of commands. Zaps backslash-newline pairs. */
	for (q=read_buffer, qend = read_buffer + got; q < qend; q++)
	{
		switch (*q)
		{
			case '\\':
				charcount++;
				do_write("\\", 1);
				if (backslash_pending)
				{
					if (p + 1 < pend)
					{
						*p++ = '\\';
						*p++ = *q;
					}
					backslash_pending = false;
				}
				else
					backslash_pending = true;
				break;
			case '\n':
				gotline = 1;
				if(cr_pending)
				{
					cr_pending = 0;
					break;
				}
			case '\r':
				gotline = 1;
				if((t_lflow == 2) && (charcount > 0))
				{
					t_lflow = 1;
					t_echo = false;
				}
				if(*q == '\r')
				{
					if(p > raw_input)
						do_write("\r\n", 2);
					cr_pending = 2;
				}

				if (backslash_pending)
				{
					if (p < pend)
						*p++ = '\n';
					backslash_pending = false;
				}
				else
				{
					*p = '\0';
					if (p > raw_input)
						save_command ((char *) raw_input);
					p = raw_input;
				}
				break;
			case '\010':	// Backspace
			case '\177':	// Delete
				cr_pending = 0;
				if(p == raw_input)
					break;
				if(*(p-1) == '\n')
					break;
				*p-- = '\0';
// Make sure the character is erased from the screen
				do_write(termcap.backspace.c_str(), termcap.backspace.length());
				x = raw_input;
				while(*x && (x < p))
				{
					if((*(x++) == '\\')
						 && (!backslash_pending))
						backslash_pending = true;
					else
						backslash_pending = false;
				}
				break;
			case '\022':	// ^R
				do_write("^R", 2);
				do_write(termcap.clearline.c_str(), termcap.clearline.length());
				x = p;
				*p = '\0';
				while(x > raw_input && (*x != '\n'))
					x--;
				if(*x == '\n')
					x++;
				do_write((const char *)x, strlen((char*)x));
				if(backslash_pending)
				{
					do_write("\\", 1);
				}
				break;
			case '\027':	// ^W
			{
				if(p == raw_input)
					break;
				x = p-1;
				*p = '\0';
				if(isspace(*x))
				{
					while(x > raw_input && isspace(*x)
						&& (*x != '\n'))
						x--;
				}
				while(x > raw_input && !isspace(*x))
					x--;
				if(isspace(*x))
					x++;

				int l = (p - x);
				char *bs = 0;
				MALLOC(bs, char, (l * termcap.backspace.length() + 5));
				*bs = '\0';
				for(; l > 0; l--)
					strcat(bs, termcap.backspace.c_str());
				do_write(bs, strlen(bs));
				FREE(bs);
				p = x;
				*p = '\0';
				break;
			}
			case '\025':	// ^U
			{
				do_write("^U", 2);
				do_write(termcap.clearline.c_str(), termcap.clearline.length());
				p = raw_input;
				*p = 0;
				backslash_pending = false;
			}
			default:
				if ((p < pend)&& is_printable (*q))
				{
					charcount++;
					if (backslash_pending)
					{
						if (p + 1 < pend)
							*p++ = '\\';
						backslash_pending = false;
					}
					do_write((const char *)q, 1);
					*p++ = *q;
				}
		}
		if(cr_pending > 0)
			cr_pending--;
	}
// If we're not sure about the connection being in char-at-a-time mode
// If we've got some data, but not an end-of-line marker then we've
// either hit NETBUFFERSIZE (unlikely at 8k), or this is char-at-a-time mode.
	if((t_lflow == 2) && (charcount > 0) && (gotline == 0))
	{
		t_lflow = 0;
		t_echo = true;
		*p = '\0';
// Output the chars used to influence the decision.
		do_write((const char *)raw_input_at, strlen((char*)raw_input_at));
	}

	/* If there's still some input pending, remember what it is; otherwise, FREE the pending buffer */
	if (p > raw_input)
	{
		raw_input_at = p;
	}
	else
	{
		FREE (raw_input);
		raw_input = NULL;
		raw_input_at = NULL;
	}

	/* We got some */
	return 1;
}


void
process_commands()
{
	int			nprocessed;
	struct	descriptor_data	*d, *dnext;

	do
	{
		nprocessed = 0;
		for (d = descriptor_list; d; d = dnext)
		{
			dnext = d->next;
			if (d->get_connect_state() != DESCRIPTOR_LIMBO && d -> quota > 0 && (!d->input.empty()))
			{
				d -> quota--;
				nprocessed++;
				String command = d->input.front();
				d->input.pop_front();
				if (!d->do_command (command))
				{
					if (d->get_connect_state() == DESCRIPTOR_CONNECTED)
						d->announce_player (ANNOUNCE_DISCONNECTED);
					d->NUKE_DESCRIPTOR ();
				}
			}
		}
	}
	while (nprocessed > 0);
}

void
boot_player (
dbref	player,
dbref	booter)

{
	time_t			now;
	(void)			time (&now);
	struct descriptor_data	*d;

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->IS_CONNECTED () && (d->get_player() == player))
		{
			if (player == booter && now - d->last_time > SELF_BOOT_TIME)
			{
				d->announce_player (ANNOUNCE_PURGE);
				d->NUKE_DESCRIPTOR ();
			}
			if (player != booter)
			{
				d->announce_player (ANNOUNCE_BOOTED);
				d->NUKE_DESCRIPTOR ();
			}
		}
	}
}


void
context::do_quit (
const	String& ,
const	String& )

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, 'quit' as a command is not implemented. (Type 'QUIT' in capitals to leave the game).");
	return;
}

	
int
descriptor_data::do_command (String command)
{
	FILE		*fp;

	if(strcmp (command.c_str(), QUIT_COMMAND) == 0)
	{
		if (IS_FAKED())
			return 1; // NPC's can't QUIT!

		if (get_descriptor())
			write (get_descriptor(), LEAVE_MESSAGE, strlen (LEAVE_MESSAGE));
#ifdef CONCENTRATOR
		else
			send_concentrator_data (channel, LEAVE_MESSAGE, strlen (LEAVE_MESSAGE));
#endif
		return 0;
	}
	else if(strcmp (command.c_str(), HALFQUIT_COMMAND) == 0)
	{
		if(IS_FAKED())
			return 1; // NPC's can't HALFQUIT!

		if (get_descriptor() && get_player())
		{
			write (get_descriptor(), HALFQUIT_MESSAGE, strlen(HALFQUIT_MESSAGE));
			HALFQUIT();
			return 1;
		}
		write(get_descriptor(), HALFQUIT_FAIL_MESSAGE, strlen(HALFQUIT_FAIL_MESSAGE));
	}
	else if(strcmp (command.c_str(), WHO_COMMAND) == 0)
	{
		output_prefix();
		dump_users (NULL, (IS_CONNECTED() && (db[get_player()].get_flag(FLAG_HOST))) ? DUMP_WIZARD : 0);
		output_suffix();
	}
	else if(strcmp (command.c_str(), IDLE_COMMAND) == 0)
	{
		queue_string ("Idle time increased to 3 minutes.\n");
		last_time = (time_t) time(NULL) - IDLE_TIME_INCREASE;
	}
	else if(strcmp (command.c_str(), INFO_COMMAND) == 0)
	{
		if ((fp=fopen (HELP_FILE, "r"))==NULL)
			log_bug("%s: %s",HELP_FILE, strerror (errno));
		else
		{
			while (fgets(scratch, BUFFER_LEN, fp)!=NULL)
				queue_string (scratch);
		}
		fclose(fp);
	}
	else if (strncmp (command.c_str(), PREFIX_COMMAND, strlen (PREFIX_COMMAND)) == 0)
		set_output_prefix(command.c_str()+strlen(PREFIX_COMMAND));
	else if (strncmp (command.c_str(), SUFFIX_COMMAND, strlen (SUFFIX_COMMAND)) == 0)
		set_output_suffix(command.c_str()+strlen(SUFFIX_COMMAND));
	else
	{
		if (IS_CONNECTED())
		{
			output_prefix();
// Make a log of the last MAX_LAST_COMMANDS commands run.
// This might help us track down bugs.
			if(LastCommands[LastCommandsPtr] != 0)
			{
				delete LastCommands[LastCommandsPtr];
			}
			LastCommands[LastCommandsPtr] = new LogCommand(get_player(), command);
			LastCommandsPtr = (LastCommandsPtr + 1) % MAX_LAST_COMMANDS;
			mud_command (get_player(), command);
			output_suffix();
		}
		else
		{
			return check_connect (command.c_str());
		}
	}
	return 1;
}

void
descriptor_data::connect_a_player (
	dbref player,
	announce_states announce_type)
{
	descriptor_data * d;
	descriptor_data * last_one;
	descriptor_data * this_one = this;
	descriptor_data * temp_next;
	descriptor_data ** temp_prev;
	time_t now;
	struct tm *the_time;

	int already_here = 0;

	(void) time (&now);
	the_time = localtime (&now);

	for (d = descriptor_list; d; d = d->next)
		if ((d->IS_CONNECTED() || d->IS_HALFQUIT()) && (player == d->get_player()))
		{
			//Put this descriptor in at this point
			//Shutdown current descriptor
			d->set_connect_state(DESCRIPTOR_LIMBO_RECONNECTED);
			d->NUKE_DESCRIPTOR();
			already_here=1;
			last_one = d;
		}


	set_connect_state(DESCRIPTOR_CONNECTED);
	set_player( player );

	if(!already_here)
	{
		if (announce_type == ANNOUNCE_CREATED) {
			log_created(CHANNEL(), player, db[player].get_name().c_str());
		}
		else {
			log_connect(true, CHANNEL(), player, db[player].get_name().c_str());
		}
		mud_connect_player (get_player());
		announce_player (announce_type);
	}
	else
	{
		log_reconnect(player, db[player].get_name().c_str(), CHANNEL());

		//transfer connect times

		start_time = last_one->start_time;

		notify_colour(player, player, COLOUR_MESSAGES, "You have been reconnected. Your previous connection has been closed and you occupy your previous place in the WHO list. To view any output from the game that you may have missed please use '@recall <number of lines to view>'");
		announce_player(ANNOUNCE_RECONNECT);
		mud_run_dotcommand(get_player(), ".reconnect");

		//Swap current position and the last descriptor
		//Swapping - last_one and this
		// This has to be done after running .reconnect so that any @terminal commnds go to the right descriptor.

		if(next)
		{
			next->prev = &(last_one->next);
		}

		if(last_one->next)
		{
			last_one->next->prev = &(this_one->next);
		}

		temp_next = this_one->next;
		this_one->next = last_one->next;
		last_one->next = temp_next;

		*prev = last_one;
		*last_one->prev = this_one;

		temp_prev = prev;
		prev = last_one->prev;
		last_one->prev = temp_prev;
	}
}

int
descriptor_data::check_connect (const char *input)
{
	char   command[MAX_COMMAND_LEN];
	char   luser[MAX_COMMAND_LEN];
	char   lpassword[MAX_COMMAND_LEN];
	char	msg[MAX_COMMAND_LEN];
	dbref  player;

	// Strip out any trailing spaces.
	while(*input && isspace(*input)) input++;

	strcpy(msg, input);
	int i = strlen(msg);
	while((i > 0) && isspace(msg[i-1])) i--;
	msg[i] = 0;

	parse_connect (msg, command, luser, lpassword);

	if ((strlen(command) >=2) && (string_prefix("connect", command)))
	{
		player = connect_player (luser, lpassword);
		if (player == NOTHING || player == 0)
		{
			queue_string (connect_fail);
			log_connect(false, CHANNEL(), -1, luser);
		}
		else
		{
			if(smd_cantuseguests(ntohl(address)) && (!string_compare(luser, "guest")))
			{
				queue_string(guest_create_banned);
				log_message("BANNED GUEST %s on descriptor %d", luser, CHANNEL());
			}
			else
			{
				connect_a_player(player, ANNOUNCE_CONNECTED);
			}
		}
	}
        else if ((strlen(command) >=2) && (string_prefix("create", command)))
        {
                if(smd_cantcreate(ntohl(address)))
                {
                        queue_string(create_banned);
			log_message("BANNED CREATE %s on descriptor %d", luser, CHANNEL());
                }
                else

                {
                        player = create_player (luser, lpassword);
                        if (player == NOTHING)
                        {
                                queue_string (create_fail);
							log_message("FAILED CREATE %s on descriptor %d", luser, CHANNEL());
                        }
                        else
                        {
				connect_a_player(player, ANNOUNCE_CREATED);
                        }
                }
        }
	else if (*command!='\0')
	{
		switch(get_connect_state())
		{
			case DESCRIPTOR_NAME:
				if(smd_cantuseguests(ntohl(address)) && (!string_compare(command, "guest")))
				{
					queue_string(guest_create_banned);
					log_message("BANNED GUEST %s on descriptor %d", luser, CHANNEL());
					welcome_user();
				}
				else
				{
					set_player_name(msg);
					dbref player = lookup_player(NOTHING, msg);
					if(NOTHING == player)
					{
						/* New player? If so, check if they want to create */
						if (ok_player_name(get_player_name()))
						{
							queue_string (character_not_found);
							queue_string (new_character_prompt);
							set_connect_state(DESCRIPTOR_NEW_CHARACTER_PROMPT);
						}
						else
						{
							queue_string (connect_fail);
							set_connect_state(DESCRIPTOR_NAME);
							if(--connect_attempts==0)
							{
								queue_string (too_many_attempts);
								set_connect_state(DESCRIPTOR_LIMBO);
								return 0;
							}
							else
								queue_string (name_prompt);
						}
					}
					else if(db[player].get_password())
					{
						set_echo(false);
						queue_string (password_prompt);
						set_connect_state(DESCRIPTOR_PASSWORD);
					}
					else // Player has no password, connect them immediately.
					{
						connect_a_player(player, ANNOUNCE_CONNECTED);
					}
				}
				break;

			case DESCRIPTOR_NEW_CHARACTER_PROMPT:
				if(strncasecmp(command, "yes", strlen(command))==0 || strncasecmp(command, "no", strlen(command))==0)
				{
					*command=tolower(*command);
					if(*command=='y')
					{
						if(smd_cantcreate(ntohl(address)))
						{
							queue_string(create_banned);
							log_message("BANNED CREATE %s on descriptor %d", luser, CHANNEL());

							queue_string (name_prompt);
							set_connect_state(DESCRIPTOR_NAME);
						}
						else
						{
							queue_string (new_password_prompt);
							set_echo (false);
							set_connect_state(DESCRIPTOR_PASSWORD);
						}
					}
					else
					{
						queue_string (cancel_create);
						set_connect_state(DESCRIPTOR_NAME);
						if(--connect_attempts==0)
						{
							queue_string (too_many_attempts);
							set_connect_state(DESCRIPTOR_LIMBO);
							return 0;
						}
						else
							queue_string (name_prompt);
					}
				}
				else
				{
					queue_string (player_is_thick);
					queue_string (new_character_prompt);
				}

				break;

			case DESCRIPTOR_PASSWORD:
				set_echo(true);
				queue_string ("\n");
				if(ok_password(msg))
				{
					player = connect_player (get_player_name(), msg);
					if (player == NOTHING)
					{
						/* New player, password is ok too */
						set_password(msg);	/* For confirmation */
						queue_string (confirm_password);
						set_echo(false);
						set_connect_state(DESCRIPTOR_CONFIRM_PASSWORD);
					}
					else if (player == 0)
					{
						/* Invalid password */
						queue_string (connect_fail);
						log_connect(false, CHANNEL(), -1, get_player_name().c_str());
						if(--connect_attempts==0)
						{
							queue_string (too_many_attempts);
							set_connect_state(DESCRIPTOR_LIMBO);
							return 0;
						}
						else
						{
							queue_string (name_prompt);
							set_connect_state(DESCRIPTOR_NAME);
						}
					}
					else
					{
						connect_a_player(player, ANNOUNCE_CONNECTED);
					}
				}
					
				break;


			case DESCRIPTOR_CONFIRM_PASSWORD:
				set_echo (true);
				// Make sure that the password is IDENTICAL.
				if(strcmp(msg, get_password().c_str())==0)
				{
					player = create_player (get_player_name(), get_password());
					if (player == NOTHING)
					{
						queue_string (create_fail);
						log_bug("FAILED CREATE: %s on descriptor %d. THIS SHOULD NOT HAPPEN!", luser, CHANNEL());
						queue_string (name_prompt);
						set_connect_state(DESCRIPTOR_NAME);
					}
					else
					{
						connect_a_player(player, ANNOUNCE_CREATED);
					}
				}
				else
				{
					queue_string (passwords_dont_match);
					queue_string (name_prompt);
					set_connect_state(DESCRIPTOR_NAME);
				}
				break;

			default:
				log_bug("check_connect called with connect_state==DESCRIPTOR_CONNECTED or DESCRIPTOR_LIMBO");
				break;
		}
	}
	else
	{
		welcome_user ();
	}

	return 1;
}

void parse_connect (const char *msg, char *command, char *user, char *pass)
{
	char *p;

	while (*msg && is_printable(*msg) && isspace (*msg))
		msg++;

	p = command;
	while (*msg && is_printable(*msg) && !isspace (*msg))
		*p++ = *msg++;

	*p = '\0';
	while (*msg && is_printable(*msg) && isspace (*msg))
		msg++;

	p = user;
	while (*msg && is_printable(*msg) && !isspace (*msg))
		*p++ = *msg++;

	*p = '\0';
	while (*msg && is_printable(*msg) && isspace (*msg))
		msg++;

	p = pass;
	while (*msg && is_printable(*msg) && !isspace (*msg))
		*p++ = *msg++;

	*p = '\0';
}


void close_sockets()
{
	struct	descriptor_data	*d;
	char			message [BUFFER_LEN];

	if (shutdown_flag != 2)
		strcpy (message, emergency_shutdown_message);
	else
	{
		strcpy (message, first_shutdown_message);
		strcat (message, getname(shutdown_player));
		strcat (message, second_shutdown_message);
		if (!blank(shutdown_reason))
		{
			strcat (message, " - ");
			strcat (message, shutdown_reason);
		}
		strcat (message, ".\n\n");
	}

	for (d = descriptor_list; d != NULL; d = d->next)
	{
		d->queue_string (message);
		d->process_output ();
		if (d->get_descriptor())
		{
			if (shutdown (d->get_descriptor(), 2) < 0)
				perror ("shutdown");
			closesocket (d->get_descriptor());
		}
	}
#ifdef CONCENTRATOR
	if (conc_connected)
		shutdown (conc_sock, 2);
#endif

	for(int i = 0; i < NumberListenPorts; i++)
	{
		closesocket (ListenPorts[i]);
	}

#if	USE_WINSOCK2
	WSACleanup ();
#endif	/* USE_WINSOCK2 */
}


void emergency_shutdown()
{
	close_sockets();
}


void
bailout (
int	sig)

{
	char message[1024];

	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, strsignal(sig));

// Dump the last MAX_LAST_COMMANDS out.
	for(int i = 0; i < MAX_LAST_COMMANDS; i++)
	{
		int j = (i + LastCommandsPtr) % MAX_LAST_COMMANDS;
		LogCommand* l = LastCommands[j];
		if(l)
		{
			log_message("LASTCOMMAND:%d Player|%d|:%s", MAX_LAST_COMMANDS - i, l->get_player(), l->get_command().c_str());
		}
	}
	panic(message);
	_exit (7);
}


dbref
match_connected_player (
const	char	*given)

{
	struct  descriptor_data *d;
	dbref			matched_player = NOTHING;

#ifdef ALIASES
	int			matched_priority = NOTHING_YET;
	int			got_one_this_time;

	for (d = descriptor_list; d; d = d->next)
	{
		if(d->IS_FAKED())
			continue; // Ignore NPCs

		got_one_this_time = NOTHING_YET;
		if (db[d->get_player()].has_partial_alias(given))
			got_one_this_time = PARTIAL_ALIAS;
		if (string_prefix(getname(d->get_player()), given))
			got_one_this_time = PARTIAL_NAME;
		if (db[d->get_player()].has_alias(given))
			got_one_this_time = COMPLETE_ALIAS;
		if (!(string_compare(getname(d->get_player()), given)))
			got_one_this_time = COMPLETE_NAME;

		if (got_one_this_time != NOTHING_YET)
		{
			/*
			 * There are three possibilities here:
			 *	Higher priority - use this one.
			 * 	Same priority   - clash! Return ambiguous.
			 *      Lower priority  - ignore the new one.
			 */

			if ((got_one_this_time == matched_priority) &&
					(d->get_player() != matched_player))
				return(AMBIGUOUS);
			if (got_one_this_time < matched_priority)
			{
				matched_priority = got_one_this_time;
				matched_player = d->get_player();
			}
		}
	}
#else
	int			number_of_matches = 0;

	for (d = descriptor_list; d; d = d->next)
	{
		if(d->IS_FAKED())
			continue;
		if (string_prefix(getname(d -> player), given))
		{
			if ((d->get_player() != matched_player) && (++number_of_matches > 1))
				return(NOTHING);
			else
				matched_player = d->get_player();
		}
	}
#endif
	return (matched_player);
}


struct WhoToShow
{
	const String		wanted;
	dbref			player;
	bool			want_everyone;
	bool			want_unconnected;
	bool			want_me;
	bool			want_wizards;
	bool			want_apprentices;
	bool			want_natter;
	bool			want_guests;
	bool			want_officers;
	bool			want_builders;
	bool			want_xbuilders;
	bool			want_fighters;
	bool			want_welcomers;
	bool			want_npcs;
	bool			want_area_who;
	bool			want_here;
	dbref 			wanted_location;

	WhoToShow(const String& who, dbref p, bool unconnectedtoo) :
		wanted			(who),
		player			(p),
		want_everyone		(!who),
		want_unconnected	(want_everyone && unconnectedtoo),
		want_me			(string_compare("me", who) == 0),
		want_wizards		(string_compare("wizards", who) == 0),
		want_apprentices	(string_compare("apprentices", who) == 0),
		want_natter		(string_compare("natter", who) == 0),
		want_guests		(string_compare("guests", who) == 0),
		want_officers		(string_compare("officers", who) == 0),
		want_builders		(string_compare("builders", who) == 0),
		want_xbuilders		(string_compare("xbuilders", who) == 0),
		want_fighters		(string_compare("fighters", who) == 0),
		want_welcomers		((string_compare("welcomers", who) == 0)
					 || (string_compare("welcomer", who) == 0)),
		want_npcs		((string_compare("npc", who) == 0)
					 || (string_compare("npcs", who) == 0)),
		want_area_who		('#' == (who.c_str()[0])),
		want_here		(string_compare("here", who) == 0),
		wanted_location		(0)
	{
		if(want_area_who)
		{
			wanted_location = atoi(who.c_str() + 1);
		}
		else if(want_here)
		{
			wanted_location = db[player].get_location();
		}
		else if((want_natter) || (string_compare("admin", who) == 0))
		{
			want_wizards = want_apprentices = true;
		}
	}

	bool Show(descriptor_data* d)
	{
		if(d->IS_FAKED())
			return want_npcs;

		if(!d->IS_CONNECTED())
			return want_unconnected;

		if(want_everyone)
			return true;

		if(!d->IS_CONNECTED())	// At this point the target has to be connected.
			return false;

		dbref target = d->get_player();
		if(!Connected(target))	// Something screwy has happened...
			return false;
		if(want_me && (player == target))				return true;
		if(want_wizards && Wizard(target))				return true;
		if(want_apprentices && Apprentice(target))			return true;
		if(want_natter && Natter(target))				return true;
		if(want_guests && is_guest(target))				return true;
		if(want_builders && Builder(target))				return true;
		if(want_xbuilders && XBuilder(target))				return true;
		if(want_fighters && Fighting(target))				return true;
		if(want_welcomers && Welcomer(target))				return true;
		if(want_area_who && in_area(target, wanted_location))		return true;
		if(want_here && in_area(target, wanted_location))		return true;

		// We've run out of standard tests, go for the names...
#ifdef ALIASES
		if(db[target].has_alias(wanted))				return true;
#endif
		if(string_prefix(db[target].get_name(), wanted))		return true;

		// If we get here, we definately don't want it.

		return false;
	}
private:
	WhoToShow(const WhoToShow&); // DUMMY
	WhoToShow& operator=(const WhoToShow&); // DUMMY
};

/* Warning: don't forget when doing colour that e->player might be zero (ie. WHO before connecting) */

void
descriptor_data::dump_users (
const	char		*victim,
int			flags)

{
	struct	descriptor_data	*d;
	time_t			now;
	char			buf[1024];
	char			flag_buf [256];
	int			length;
	int			users = 0;
	int			local = 0, inet = 0, logthrough = 0;
	int			builder = 0, wizard = 0, haven = 0;
	int			uptime;
	int			gotonealready = 0;

	int			firstchar;

	(void) time (&now);

	WhoToShow who(victim, get_player(), true);
	
	//Find size of current descriptor list
	//Make new array
	//Copy in
	//Sort

	for (d = descriptor_list; d; d = d->next)
	{
		if(who.Show(d))
		{
			if (!gotonealready)
			{
				if (get_player())
					queue_string (player_colour (get_player(), get_player(), COLOUR_MESSAGES));
				if (victim == NULL)
					queue_string ("Current Players:                                                          Idle\n");
				else if (who.want_npcs)
					queue_string ("Currently connected NPCs:                                                 Idle\n");
				else
					queue_string ("Specific player details (as requested):                                   Idle\n");

				if (get_player())
					queue_string (COLOUR_REVERT);
				gotonealready = 1;
			}
			/* ... start time... */
			if (d->start_time)
			{
				length = now - d->start_time;
				if (get_player())
					queue_string (player_colour (get_player(), get_player(), COLOUR_WHOSTRINGS));
				if (length < 60)
					snprintf (buf, sizeof(buf), "    %02ds", length);
				else if (length < 60 * 60)
					snprintf (buf, sizeof(buf), "%2dm %02ds", length / 60, length % 60);
				else if (length < 8 * 60 * 60)
					snprintf (buf, sizeof(buf), "%2dh %02dm", length / (60 * 60), (length / 60) % 60);
				else if (length < 9 * 60 * 60)
					snprintf (buf, sizeof(buf), "Muddict");
				else if (length < 24 * 60 * 60)
					snprintf (buf, sizeof(buf), "%2dh %02dm", length / (60 * 60), (length / 60) % 60);
				else if (length < 7 * 24 * 60 * 60)
					snprintf (buf, sizeof(buf), "%2dd %02dh", length / (24 * 60 * 60), (length / (60 * 60)) % 24);
				else
					snprintf (buf, sizeof(buf), "%2dw %02dd", length / (7 * 24 * 60 * 60), (length / (24 * 60 * 60) % 7));

			}
			else
				strcpy (buf, " Broken");

			strncat (buf, " ", sizeof(buf));
			int twidth= 80;
			int remaining= twidth;

			/* Format output: Name... */
			if (d->IS_CONNECTED())
			{
				if (who.want_npcs)
				{
					snprintf(scratch, sizeof(scratch), "#%-5d ", d->get_player());
					remaining-=7;
					strcat(buf, scratch);
				}

				if (get_player())
				{
					int thing= d->get_player();
					int colour=rank_colour(thing);
					if (!who.want_npcs)
						strcat(buf, player_colour (get_player(), get_player(), colour));
				}
				if (who.want_npcs)
				{
					snprintf(flag_buf, sizeof(flag_buf), "(%s)", db[db[d->get_player()].get_owner()].get_name().c_str());
					snprintf(scratch, sizeof(scratch), "%-20s %-22s", db[d->get_player()].get_name().c_str(), flag_buf);
				}
				else
					snprintf(scratch, sizeof(scratch), "%s", db[d->get_player()].get_name().c_str());

				strcat(buf, scratch);

				flag_buf[0]='\0';

				if (db[d->get_player()].get_who_string ())
				{
					firstchar = db[d->get_player()].get_who_string().c_str()[0];
					if (!(firstchar == ' '
					|| firstchar == ','
					|| firstchar == ';'
					|| firstchar == ':'
					|| firstchar == '.'
					|| firstchar == '\''))
					{
						strcat (buf, " ");
						remaining--;
					}
				}

				strcat (flag_buf, "(");
				bool wantbuilder = true;
				if (Retired(d->get_player()))
				{
					strcat (flag_buf, "R");
					wantbuilder = false;
				}
				else if (Wizard(d->get_player()))
				{
					strcat (flag_buf, "W");
					wantbuilder = false;
					wizard++;
				}
				else if (Apprentice(d->get_player()))
				{
					strcat (flag_buf, "A");
					wantbuilder = false;
				}
				if (XBuilder(d->get_player()))
				{
					strcat (flag_buf, "X");
				}
				else if (wantbuilder && (Builder(d->get_player())))
				{
					strcat (flag_buf, "B");
					builder++;
				}
				if (Welcomer(d->get_player()))
				{
					strcat (flag_buf, "w");
				}
				if (Fighting(d->get_player()))
				{
					strcat (flag_buf, "F");
				}
				if (Haven(d->get_player()))
				{
					strcat (flag_buf, "H");
					haven++;
				}
				if (strlen (flag_buf) == 1)
					*flag_buf = '\0';
				else
					strcat (flag_buf, ")");

				if (who.want_npcs)
					remaining-=18 + strlen(flag_buf) + 43;
				else
					remaining-=18 + strlen(flag_buf) + strlen(db[d->get_player()].get_name().c_str());
				if (get_player())
					strcat(buf, player_colour (get_player(), get_player(), COLOUR_WHOSTRINGS));
				if (db[d->get_player()].get_who_string())
					strcat(buf, chop_string(db[d->get_player()].get_who_string ().c_str(), remaining));
				else
					strcat(buf, chop_string("", remaining)); // Lazy


				strcat(buf, " ");
				if (get_player())
					strcat(buf,player_colour (get_player(), get_player(), COLOUR_WHOSTRINGS));
				/* count the users. */
				users++;

				if (!who.want_npcs)
				{
					/* ... and machine name */
					if (d->address == LOGTHROUGH_HOST)
						logthrough++;
					else if ((d->address & 0xffff0000) == LOCAL_ADDRESS_MASK || d->address == INADDR_LOOPBACK)
						local++;
					else
						inet++;
				}

			}
			else
			{

				strcat (buf, chop_string("*** UNCONNECTED ***", 63));
				*flag_buf = '\0';
				/* ... and machine name */
			}
			
			strcat (buf, flag_buf);


			/* ... idle time... - may as well re-use flag_buf*/
			if (d->last_time)
			{
				length = now - d->last_time;
				if (length < 60)
					snprintf (flag_buf, sizeof(flag_buf), "    %02ds", length);
				else if (length < 60 * 60)
					snprintf (flag_buf, sizeof(flag_buf), "%2dm %02ds", length / 60, length % 60);
				else if (length < 24 * 60 * 60)
					snprintf (flag_buf, sizeof(flag_buf), "%2dh %02dm", length / (60 * 60), (length / 60)  % 60);
				else if (length < 7 * 24 * 60 * 60)
					snprintf (flag_buf, sizeof(flag_buf), "%2dd %02dh", length / (24 * 60 * 60), (length / (60 * 60) % 24));
				else
					snprintf (flag_buf, sizeof(flag_buf), "%2dw %02dd", length / (7 * 24 * 60 * 60), (length / (24 * 60 * 60) % 7));
			}
			else
				snprintf (flag_buf, sizeof(flag_buf), "TooLong");

			strcat(buf, flag_buf);
			if (flags & DUMP_WIZARD)
			{
				snprintf (flag_buf, sizeof(flag_buf), " [%s:%s]", d->hostname.c_str(),d->service.c_str());
				strcat(buf, flag_buf);
			}

			/* Add a newline and queue it */
			strcat (buf, "\n");
			queue_string (buf);

		}
		if (get_player())
			queue_string (COLOUR_REVERT);
			
	}
	if (gotonealready)
	{
		if (!(flags & DUMP_QUIET))
		{
			if(who.want_npcs)
			{
				snprintf (buf, sizeof(buf), "NPCs: %d\n", users);
				queue_string (buf);
				if (get_player())
					queue_string (COLOUR_REVERT);
			}
			else
			{
				if(users > peak_users)
					peak_users = users;

				uptime = time(NULL) - game_start_time;

				snprintf (scratch, sizeof(scratch), " Up: %s", small_time_string(uptime));
				if (get_player())
					queue_string (player_colour (get_player(), get_player(), COLOUR_MESSAGES));
				if (flags & DUMP_WIZARD)
					snprintf (buf, sizeof(buf), "Users: %d (Peak %d)  (%d local, %d remote, %d logthrough) %s\n", users, peak_users, local, inet, logthrough, scratch);
				else
					snprintf (buf, sizeof(buf), "Users: %d (Peak %d)  %s\n", users, peak_users, scratch);
				queue_string (buf);
				if (get_player())
					queue_string (COLOUR_REVERT);
			}
		}
	}
	else
	{
		if (!(flags & DUMP_QUIET))
			queue_string("No match found.\n");
	}

}

void
context::do_at_connect(
const	String& what,
const	String& )
{
struct  descriptor_data *d;
        dbref   victim;

        return_status = COMMAND_FAIL;
        set_return_string (error_return_string);

        /* get victim */
        Matcher matcher (player, what, TYPE_PUPPET, get_effective_id());
        matcher.match_everything ();
        if (((victim = matcher.noisy_match_result ()) == NOTHING) ||
                (Typeof(victim) != TYPE_PUPPET))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That NPC does not exist.");
                return;
        }


        /* Check player/victim dependencies */
        if ((!controls_for_write (victim))
                || (Wizard (victim) && !Wizard (player)))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
                return;
        }
        if (Connected(victim))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That NPC is already connected.");
                return;
        }
        d = new descriptor_data(0, 0, 0); // No fd, No sockaddr, No channel
        d->set_player( victim );
        db[victim].set_flag(FLAG_CONNECTED);
	Accessed (victim);
        notify(player, "Connected");
        return_status = COMMAND_SUCC;
        set_return_string (ok_return_string);
}

void
context::do_at_disconnect(
const   String&what,
const   String&)

{
struct  descriptor_data *d;
        dbref   victim;

        return_status = COMMAND_FAIL;
        set_return_string (error_return_string);

        /* get victim */
        Matcher matcher (player, what, TYPE_PUPPET, get_effective_id());
        matcher.match_everything ();
        if (((victim = matcher.noisy_match_result ()) == NOTHING) ||
                (Typeof(victim) != TYPE_PUPPET))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That NPC does not exist.");
                return;
        }
        /* Check player/victim dependencies */
        if ((!controls_for_write (victim))
                || (Wizard (victim) && !Wizard (player)))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
                return;
        }
        if (!Connected(victim))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That NPC is not connected.");
                return;
        }
        for (d = descriptor_list; d; d = d->next)
                if (d->IS_CONNECTED() && (d->get_player() == victim))
                {
			if(d->raw_input) // Make sure we don't have a memory leak
			{
				FREE(d->raw_input);
				d->raw_input = 0;
			}
                        d->shutdownsock();
                        break;
                }
        db[victim].clear_flag(FLAG_CONNECTED); // NPC disconnected
	Accessed (victim);
        notify(player, "Disconnected");

        return_status = COMMAND_SUCC;
        set_return_string (ok_return_string);
}
void
context::do_at_queue (
const   String& what,
const   String& command)

{
struct  descriptor_data *d;
        dbref   victim;

        return_status = COMMAND_FAIL;
        set_return_string (error_return_string);

        /* get victim */
        if((victim = lookup_player(player, what)) == NOTHING)
        {
                Matcher matcher (player, what, TYPE_PUPPET, get_effective_id());
                matcher.match_everything ();
                if (((victim = matcher.noisy_match_result ()) == NOTHING) ||
                        (Typeof(victim) != TYPE_PUPPET))
                {
                        notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That player does not exist.");
                        return;
                }
        }

        /* Check player/victim dependencies */
        if ((!controls_for_write (victim))
                || (Wizard (victim) && !Wizard (player)))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
                return;
        }

	if(!command)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "What do you want to @queue?");
		return;
	}

	/* JPK hack to match softcode change */
	if (Typeof(victim) != TYPE_PUPPET)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you can only queue commands for puppets.");
		return;
	}

        for (d = descriptor_list; d; d = d->next)
                if (d->IS_CONNECTED() && (d->get_player() == victim))
                {
                        d->save_command(command.c_str());
			Accessed (victim);
                        break;
                }
        return_status = COMMAND_SUCC;
        set_return_string (ok_return_string);
}

void
context::do_who (
const	String& name,
const	String& )

{
	int matched_location;
struct	descriptor_data	*d;
	const	char		*victim;

	if (!name)
	{
/* If no name dump users as usual. */
		victim = NULL;
		for (d = descriptor_list; d; d = d->next)
			if (d->IS_CONNECTED() && (d->get_player() == player))
				d->dump_users (victim, ((in_command()) ? DUMP_QUIET : 0) | (db[player].get_flag(FLAG_HOST) ? DUMP_WIZARD : 0)); 
	}
	else
	{
/* Otherwise find if we are dealing with a localised who */
		victim = name.c_str();
		if (victim[0] == '#')
		{
/* If we are then dump users with real id rather than effective */
			Matcher loc_matcher (player, victim, TYPE_NO_TYPE, get_effective_id());
			loc_matcher.match_everything();
			if ((matched_location = loc_matcher.noisy_match_result()) == NOTHING)
				return;
			if (!((Typeof (matched_location) == TYPE_PLAYER) || controls_for_read(matched_location)))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Permission Denied.");
				return;
			}
		}
		for (d = descriptor_list; d; d = d->next)
			if (d->IS_CONNECTED() && (d->get_player() == player))
				d->dump_users (victim, ((in_command()) ? DUMP_QUIET : 0) | (db[player].get_flag(FLAG_HOST) ? DUMP_WIZARD : 0));
	}
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_swho (
const	String& victim,
const	String& )

{
	static	char		linebuf [4096];
	String			line;
	struct	descriptor_data	*d;
	int			num = 0;

	int			width = 0;
	for(d = descriptor_list; d; d = d->next)
	{
		if(d->get_player() == player)
		{
			width = d->terminal.width;
			break;
		}
	}
	int			numperline = (width-1) / 25;

	if(numperline <= 0) numperline = 3;

	WhoToShow who(victim, get_player(), false);

	notify_colour(player, player, COLOUR_MESSAGES, "Current players:");
	for (d = descriptor_list; d; d = d->next)
		if(who.Show(d))
		{
			int thing= d->get_player();
			int colour=rank_colour(thing);
			char rankchar= Retired(d->get_player())?'-':
					(Wizard(d->get_player())?'*':
						(Apprentice(d->get_player())?'~':' '));
			snprintf(linebuf, sizeof(linebuf), " %c%s%-23s%s", 
				rankchar,
				player_colour (get_player(), get_player(), colour),
				db[d->get_player()].get_name().c_str(),
				COLOUR_REVERT);
			line += linebuf;
			if((++num % numperline) == 0)
			{
				notify(player, "%s", line.c_str());
				line = "";
			}
		}

	if(line)
		notify(player, "%s", line.c_str());
	notify_colour(player, player, COLOUR_MESSAGES, "Users: %d", num);

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_query_connectedplayers (const String& victim, const String& name)
{
struct	descriptor_data *d;
bool found_one = false;
String buf;

	WhoToShow who(victim, get_player(), false);

	// The remaining players separated with semi-colons.
	for (d = descriptor_list; d; d = d->next)
	{
		if(who.Show(d))
		{
			if (found_one)
			{
				buf+=";";
			}
			if(d->IS_FAKED())  	// NPC, so give number
			{
				snprintf(scratch, sizeof(scratch), "#%d",d->get_player());
				buf+=scratch;
			}
			else 			// Normal player, so give name
			{
				buf+=db[d->get_player()].get_name();
			}
			found_one = true;
		}
	}
	return_status = COMMAND_SUCC;
	set_return_string (buf);
}


time_t get_idle_time (dbref player)
{
	struct descriptor_data *d;
	time_t now;

	for(d=descriptor_list; d!=NULL; d=d->next)
	{
		if(d->get_player() == player)
			break;
	}

	if(d==NULL)
		return -1;

	time(&now);
	return (now - d->last_time);
}

String
descriptor_data::terminal_get_noflush()
{
	if(terminal.noflush)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_noflush(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on") == 0)
		{
			terminal.noflush = true;
		}
		else if(string_compare(toggle, "off") == 0)
		{
			terminal.noflush = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal noflush=on' or '@terminal noflush=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"noflush is %s", terminal.noflush?"on":"off");
	}
	return COMMAND_SUCC;
}


String
descriptor_data::terminal_get_colour_terminal()
{
	if(terminal.colour_terminal)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_colour_terminal(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on") == 0)
		{
			terminal.colour_terminal = true;
		}
		else if(string_compare(toggle, "off") == 0)
		{
			terminal.colour_terminal = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal colour_terminal=on' or '@terminal colour_terminal=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"colour_terminal is %s", terminal.colour_terminal?"on":"off");
	}
	return COMMAND_SUCC;
}


String
descriptor_data::terminal_get_emit_lastcommand()
{
	if(terminal.emit_lastcommand)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_emit_lastcommand(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on") == 0)
		{
			terminal.emit_lastcommand = true;
		}
		else if(string_compare(toggle, "off") == 0)
		{
			terminal.emit_lastcommand = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal commandinfo=on' or '@terminal commandinfo=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"commandinfo is %s", terminal.emit_lastcommand?"on":"off");
	}
	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_sevenbit()
{
	if(terminal.sevenbit)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_sevenbit(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(toggle.c_str()[1]!='\0')
		{
			if(string_compare(toggle, "off")==0)
				terminal.sevenbit='\0';
			else
			{
				notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal sevenbit=<character>' or '@terminal sevenbit=off'.");
				return COMMAND_FAIL;
			}
		}
		else if(isprint(toggle.c_str()[0]))
			terminal.sevenbit=toggle.c_str()[0];
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "That isn't a valid character to use with sevenbit.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		if(terminal.sevenbit)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "sevenbit is '%c'", terminal.sevenbit);
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "sevenbit is off");
	}
	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_halfquit()
{
	if(terminal.halfquit)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_halfquit(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on") == 0)
		{
			terminal.halfquit = true;
		}
		else if(string_compare(toggle, "off") == 0)
		{
			terminal.halfquit = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal halfquit=on' or '@terminal halfquit=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"Auto-HALFQUIT is %s", terminal.halfquit?"on":"off");
	}
	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_effects()
{
	if(terminal.effects)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_effects(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on") == 0)
		{
			terminal.effects = true;
		}
		else if(string_compare(toggle, "off") == 0)
		{
			terminal.effects = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal effects=on' or '@terminal effects=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"Effects are %s", terminal.effects?"on":"off");
	}
	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_recall()
{
	if(terminal.recall)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_recall(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on") == 0)
		{
			terminal.recall = true;
		}
		else if(string_compare(toggle, "off") == 0)
		{
			terminal.recall = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal recall=on' or '@terminal recall=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"Recall is %s", terminal.recall?"on":"off");
	}
	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_echo()
{
	if(t_echo)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_echo(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on")==0)
			t_echo = true;
		else if(string_compare(toggle, "off")==0)
			t_echo = false;
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal echo=on' or '@terminal echo=off'.");
			return COMMAND_FAIL;
		}
	}

	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"Echo is %s", t_echo?"on":"off");
	}

	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_pagebell()
{
	if(terminal.pagebell)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_pagebell(const String& toggle, bool gagged)
{
	if(toggle)
	{
		if(string_compare(toggle, "on")==0)
			terminal.pagebell=true;
		else if(string_compare(toggle, "off")==0)
			terminal.pagebell=false;
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal pagebell=on' or '@terminal pagebell=off'.");
			return COMMAND_FAIL;
		}
	}

	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"Pagebell is %s", terminal.pagebell?"on":"off");
	}

	return COMMAND_SUCC;
}



String
descriptor_data::terminal_get_termtype()
{
	return terminal.type;
}

Command_status
descriptor_data::terminal_set_termtype (const String& termtype, bool gagged)
{
	if(termtype)
	{
		if(string_compare(termtype, "none")==0)
		{
			terminal.type = (const char*)0;
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Your terminal type is no longer set.");
		}
		else
		{
			if(set_terminal_type(termtype))
				notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal type is now '%s'.", termtype.c_str());
			else
			{
				notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Unknown terminal '%s'.", termtype.c_str());
				return COMMAND_FAIL;
			}
		}
	}
	else
	{
		if(terminal.type)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal type is '%s'.", terminal.type.c_str());
		else
			notify_colour(get_player(),get_player(), COLOUR_MESSAGES, "Your terminal type is not set.");
	}

	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_height()
{
char ret[40];
	sprintf(ret, "%d", terminal.height);

	return ret;
}


Command_status
descriptor_data::terminal_set_height(const String& height, bool gagged)
{
	int			i;

	if(height)
	{
		i = atoi (height.c_str());
		if (i<0)
		{
			notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Can't have a negative word terminal height");
			return COMMAND_FAIL;
		}
		terminal.height = i;
	}
	if(!gagged)
	{
		if(terminal.height == 0)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal height is unset");
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal height is %d", terminal.height);
	}

	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_width()
{
char ret[40];
	sprintf(ret, "%d", terminal.width);

	return ret;
}


Command_status
descriptor_data::terminal_set_width(const String& width, bool gagged)
{
	int			i;

	if(width)
	{
		i = atoi (width.c_str());
		if (i<0)
		{
			notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Can't have a negative word terminal width");
			return COMMAND_FAIL;
		}
		terminal.width = i;
	}
	if(!gagged)
	{
		if(terminal.width == 0)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal width is unset");
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal width is %d", terminal.width);
	}

	return COMMAND_SUCC;
}

String
descriptor_data::terminal_get_wrap()
{
	if(terminal.wrap)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_wrap(const String& width, bool gagged)
{
	int			i;

	if(width)
	{
		if(string_compare(width, "on") == 0)
		{
			terminal.wrap = true;
		}
		else if(string_compare(width, "off") == 0)
		{
			terminal.wrap = false;
		}
		else
		{
			i = atoi (width.c_str());
			if(isdigit(width.c_str()[0]) || (i != 0))
			{
				notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Deprecated usage: @terminal wrap=<number>. Please use @terminal width instead");

			}
			else
			{
				notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal wrap=on' or '@terminal wrap=off'.");
			}
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Word wrap is %s", terminal.wrap?"on":"off");
	}
	return COMMAND_SUCC;
}


String
descriptor_data::terminal_get_lftocr()
{
	if(terminal.lftocr)
		return str_on;
	else
		return str_off;
}

Command_status
descriptor_data::terminal_set_lftocr(const String& z, bool gagged)
{
	if(z)
	{
		if(string_compare(z, "on") == 0)
		{
			terminal.lftocr = true;
		}
		else if(string_compare(z, "off") == 0)
		{
			terminal.lftocr = false;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  '@terminal lftocr=on' or '@terminal lftocr=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!gagged)
	{
		notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"lftocr is %s", terminal.lftocr?"on":"off");
	}
	return COMMAND_SUCC;
}


void
context::do_at_motd (
const	String& ,
const	String& )

{
	struct descriptor_data *d;

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (d->get_player() == player))
			d->splat_motd();
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_beep (
const	String& ,
const	String& )

{
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	beep (player);
}


void
context::do_at_truncate(
const String& plist,
const String& string)
{
	int error_count=0,
	    ok_count=0;

	if (!plist || !string)
	{
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @truncate <playerlist> = <underline string>");
		return;
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	char *mystring=safe_strdup(string.c_str()); // Discarding the const

	Player_list targets(player);
	if (targets.build_from_text(player, plist) == 0)
		return;

	dbref target=targets.get_first();
	while (target!=NOTHING)
	{
		if ((controls_for_read(db[target].get_location())) || (target==player) || (target==get_effective_id()))
		{
			ok_count++;
			terminal_underline(target, mystring);
		}
		else	
			error_count++;
		target=targets.get_next();
	}
	free (mystring);

	if (!in_command())
	{
		if (error_count==0)
			notify_colour(player, player, COLOUR_MESSAGES, "Underlined.");
		else if (ok_count == 0)
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only @truncate to yourself or to people in rooms that you control.");
		else
			notify_colour(player, player, COLOUR_MESSAGES, "(Warning): %d player%s that you do not control so did not get underlined to.", error_count, (error_count==1) ? " is in a room" : "s are in rooms");
	}
}
void context::do_at_terminal(const String& command, const String& arg)
{
	struct descriptor_data *d;

	for(d=descriptor_list; d; d=d->next)
		if(d->get_player()==player)
			break;

	if(!d)
	{
		RETURN_FAIL;
	}

	int		i;
	bool		gag = arg?gagged_command():false;

	for(i=0; terminal_set_command_table[i].name; i++)
	{
		if(command)
		{
			if(string_compare(command, terminal_set_command_table[i].name)==0)
			{
				return_status=(d->*(terminal_set_command_table[i].set_function))(arg, gag);
				
				break;
			}
		}
		// If @terminal is run with no parameter then ignore the in_command bit
		else if(!terminal_set_command_table[i].deprecated)
			(d->*(terminal_set_command_table[i].set_function))(NULLSTRING, false);
	}

	if(command && !terminal_set_command_table[i].name)
	{
		if(!gagged_command())
			notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Unknown terminal set command '%s'.", command.c_str());
		RETURN_FAIL;
	}
	else if(!command)
	{
		RETURN_SUCC;
	}

	set_return_string ((return_status==COMMAND_SUCC) ? ok_return_string : error_return_string);
}

void
context::do_query_terminal(const String& command, const String& arg)
{
	struct descriptor_data *d;

	for(d=descriptor_list; d; d=d->next)
		if(d->get_player()==player)
			break;

	if(!d)
		return;

	if(string_compare(command, "commands") == 0)
	{
		static char retbuf[4096];
		retbuf[0] = 0;
		for(int i=0; terminal_set_command_table[i].name; i++)
		{
			if((i != 0) && (!terminal_set_command_table[i].deprecated))
				strcat(retbuf, ";");
			strcat(retbuf, terminal_set_command_table[i].name);
		}
		set_return_string(retbuf);
		return_status = COMMAND_SUCC;
		return;
	}
	int i;
	for(i=0; terminal_set_command_table[i].name; i++)
	{
		if(string_compare(command, terminal_set_command_table[i].name) == 0)
			break;
	}
	if(terminal_set_command_table[i].name)
	{
		if(terminal_set_command_table[i].query_function)
		{
			set_return_string((d->*(terminal_set_command_table[i].query_function))());
			return_status = COMMAND_SUCC;
			return;
		}
		notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "@?terminal %s not implemented yet", command.c_str());
		RETURN_FAIL;
	}
	if(!gagged_command())
		notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Unknown @?terminal option: %s. Use @?terminal commands to list available commands.", command.c_str());
	RETURN_FAIL;
}

void
smd_updated()
{
struct descriptor_data *d;
u_long	host_addr;

	for(d=descriptor_list; d!=NULL; d=d->next)
	{
		host_addr = ntohl (d->address);

		if (is_banned (host_addr))
		{
			d->queue_string(reject_updatedsmd);
			d->announce_player (ANNOUNCE_SMD);
			d->NUKE_DESCRIPTOR();
		}
		else
		{
			if(((d->hostname.c_str()[0]) >= '0') && ((d->hostname.c_str()[0]) <= '9'))
			{
				d->hostname=convert_addr (d->address);
			}
		}
		
	}
}

#ifdef CONCENTRATOR
static struct descriptor_data *which_descriptor(int channel)
{
	struct	descriptor_data	*d;

	for(d=descriptor_list; d; d=d->next)
		if(d->channel == channel)
			break;

	if(!d)
		log_bug("CONCENTRATOR: couldn't find channel %d in descriptor list!", channel);

	return d;
}


int process_concentrator_input(int sock)
{
	struct	conc_message		msg;
	struct	descriptor_data		*d;
	int				i;

	if ((i=read(sock, &msg, sizeof(msg)))<sizeof(msg))
	{
		log_bug("CONCENTRATOR: corrupt message received - wanted %d bytes, got %d", sizeof(msg), i);
		return 0;
	}

	switch(msg.type)
	{
		case CONC_CONNECT:

			/* This is the equivalent of new_connection() for concentrator connections */
			log_message("ACCEPT from concentrator(%d) on channel %d", sock, msg.channel);
			new descriptor_data (0, NULL, msg.channel);
			break;

		case CONC_DISCONNECT:
			if((d=which_descriptor(msg.channel)))
			{
				d->announce_player (ANNOUNCE_DISCONNECTED);
				d->NUKE_DESCRIPTOR ();
			}
			break;

		case CONC_DATA:
			if((d=which_descriptor(msg.channel)))
				d->process_input (msg.len);
			else
				log_message("CONCENTRATOR: got data on unknown channel %d (discarded)", msg.channel);
			break;

		default:
			log_message("CONCENTRATOR: unknown message type %d received (discarded)", msg.type);
			break;
	}

	return 1;
}

int send_concentrator_data(int channel, char *data, int length)
{
	struct conc_message msg;

	msg.channel=channel;
	msg.type=CONC_DATA;
	msg.len=length;


	write(conc_sock, &msg, sizeof(msg));
	return write(conc_sock, data, length);
}


void concentrator_disconnect(void)
{
	struct descriptor_data *d;

	log_message("CONCENTRATOR: dropped connection (players booted)");
	for (d=descriptor_list; d; d=d->next)
	{
		if (d->channel)
		{
			d->announce_player (ANNOUNCE_DISCONNECTED);
			d->NUKE_DESCRIPTOR();
		}
	}

	shutdown(conc_sock, 2);
	conc_connected=0;
}
#endif

/*----------------------------------------------------------------------------
 * CLASS descriptor_data
 *
 * Member functions
 */

void
descriptor_data::output_prefix()
{
	myoutput = true;
	if(IS_FAKED())
		return;
	if(_output_prefix)
	{
		queue_string(_output_prefix.c_str());
		queue_string("\n");
	}
}

void
descriptor_data::output_suffix()
{
	myoutput = false;
	if(IS_FAKED())
		return;
	if(_output_suffix)
	{
		queue_string(_output_suffix.c_str());
		queue_string("\n");
	}
}

void
set_last_command(const String& command, dbref player)
{
	if(!command)
	{
		descriptor_data::ClearLastCommand();
	}
	else
	{
		descriptor_data::SetLastCommand(command, player);
	}
}

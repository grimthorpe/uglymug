/* static char SCCSid[] = "@(#)interface.c 1.155 00/06/29@(#)"; */
/*
 * Waaaah, someone took our comment out!
 *
 * -- Flup, ReaperMan, and Bursar; 2-MAY-94
 */

/* "To fight a surperior battle for a rightous cause" */

/* KEITH: Buy a surperior spellchecker too. - Luggs */

#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
/* JPK #include <malloc.h> */
#include <stdarg.h>

#include "db.h"
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

#ifdef USE_TERMINFO
#if defined (sun) || (linux)
	#include <curses.h>
#else
	/* #include <ncurses.h> */
	#include "local_curses.h"
#endif
	#include <term.h>
#endif	/* USE_TERMINFO */

#ifdef DEBUG_TELNET
#define	TELOPTS		/* make telnet.h define telopts[] */
#define	TELCMDS
#endif /* DEBUG_TELNET */
#ifdef ABORT
#undef ABORT
#endif
#ifdef linux
#include <arpa/telnet.h>
#else
#include "telnet.h"
#endif

#define safe_strdup(x) ((x==0)?strdup(""):strdup(x))
#ifdef	SYSV
#define	getdtablesize()		MAX_USERS + 4
#endif	/* SYSV */

#define	HELP_FILE		"help.txt"
#define	SIGNAL_STACK_SIZE	200000
#define	LOGTHROUGH_HOST		0xc0544e92	/* 192.84.78.146 (delginis) */
#define	LOCAL_ADDRESS_MASK	0x82580000	/* 130.88.0.0 */
//#define	DOMAIN_STRING		".man.ac.uk"
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
static	int	check_descriptors;
int			peak_users;

static const char *connect_fail =
	"Either that player does not exist, or has a different password.\n";
static const char *create_fail =
	"Either there is already a player with that name, or that name is illegal.\n";
static const char *flushed_message =
	"<Output Flushed>\n";
static const char *emergency_shutdown_message =
	"Emergency shutdown. We're numb, we're so numb it hurts.\n\n";
static const char *first_shutdown_message =
	"All hands brace for real life.\n";
static const char *second_shutdown_message =
	" has shut down the game";
static const char *reject_message =
	"Connections to UglyMUG from your site have been banned. Sorry.\n";
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

struct text_block
{
	int			nchars;
	struct	text_block	*nxt;
	char			*start;
	char			*buf;
};


struct text_queue
{
	struct	text_block	*head;
	struct	text_block	**tail;
};

enum connect_states
{
	DESCRIPTOR_UNCONNECTED,			/* Just initialised */
	DESCRIPTOR_NAME,			/* Await player name */
	DESCRIPTOR_PASSWORD,			/* Await password */
	DESCRIPTOR_NEW_CHARACTER_PROMPT,	/* New Character? */
	DESCRIPTOR_CONFIRM_PASSWORD,		/* Re-enter password */
	DESCRIPTOR_CONNECTED,			/* Player is connected */
	DESCRIPTOR_LIMBO,			/* Awaiting disconnection */
	DESCRIPTOR_FAKED			/* Faked connection; NPC */
};

enum announce_states
{
	ANNOUNCE_CREATED,			/* player just been created */
	ANNOUNCE_CONNECTED,			/* player just connected */
	ANNOUNCE_BOOTED,			/* player just booted */
	ANNOUNCE_DISCONNECTED,			/* player just QUITed */
	ANNOUNCE_SMD,				/* player fallen foul of an SMD read */
	ANNOUNCE_TIMEOUT,			/* mortal been idle too long */
	ANNOUNCE_PURGE				/* player booting his idle connections */
};

void add_to_queue(struct text_queue *q, const char *b, int n);

class descriptor_data
{
public:
	descriptor_data(int sock, sockaddr_in *a, int conc = 0);
	void clearstrings();
private:
	int			_descriptor;
	dbref			_player;	/* Initilized to zero; hence (d->get_player() == 0) implies never connected */
	char			*_player_name;	/* Used at connect time - not necessarily correct outside check_connect */
	char			*_password;	/* Used at connect time - not necessarily correct outside check_connect */
	enum connect_states	_connect_state;
	char			*_output_prefix;
	char			*_output_suffix;
// Remove this when ready:
public:
	int			connect_attempts;
	int			output_size;
	struct	text_queue	output;
	struct	text_queue	input;
	unsigned char		*raw_input;
	unsigned char		*raw_input_at;
	long			start_time;
	long			last_time;
        long                    time_since_idle;
	int			warning_level;
	int			quota;
	int			backslash_pending;
	int			cr_pending;
	int			indirect_connection;	/* got hostname through a SNDLOC */
	char			hostname [MAXHOSTNAMELEN];
	struct	sockaddr_in	address;
	class	descriptor_data	*next;
	class	descriptor_data	**prev; // Pointer to previous->next!
	int                     terminal_width;
	int                     terminal_wrap;
	int			terminal_height;
	char			*terminal_type;
	struct		{
				char *bold_on;
				char *bold_off;
				char *underscore_on;
				char *underscore_off;
				char *backspace;
				char *clearline;
			}	termcap;
	int			terminal_xpos;
	int			terminal_lftocr;
	int			terminal_pagebell;
	int			channel;

	int			myoutput;

	int			t_echo;
	int			t_lflow;
	int			t_linemode;

	enum IAC_STATUS	{	IAC_OUTSIDE_IAC,	IAC_GOT_FIRST_IAC,
				IAC_GOT_COMMAND,	IAC_INSIDE_IAC,
				IAC_GOT_ANOTHER_IAC }
				t_iac;
	unsigned char*		t_iacbuf;
	int			t_piacbuf;
	int			t_liacbuf;
	unsigned char		t_iac_command;
	unsigned char		t_iac_option;

	int			_got_an_iac; // Set to non-zero if we've ever received an iac
// Functions:
public:
	int	IS_CONNECTED()
	{
		return ((_connect_state == DESCRIPTOR_CONNECTED)
			|| (_connect_state == DESCRIPTOR_FAKED));
	}
	int	IS_FAKED()
	{
		return (_connect_state == DESCRIPTOR_FAKED);
	}
	int	CHANNEL()
	{
		return ((get_descriptor()==0)? -channel:get_descriptor());
	}
	void	NUKE_DESCRIPTOR()
	{
		_connect_state = DESCRIPTOR_LIMBO;
		check_descriptors = 1;
	}
	void	output_prefix();
	void	output_suffix();
	void	set_output_prefix(const char *s);
	void	set_output_suffix(const char *s);

	enum connect_states
		get_connect_state()	{ return _connect_state; }
	void	set_connect_state(enum connect_states cs)
					{ _connect_state = cs; }
	int	get_descriptor()	{ return _descriptor; }
	int	get_player()		{ return _player; }
	char *	get_player_name()	{ return _player_name;}
	char *	get_password()		{ return _password;}
	void	set_player(int p)	{ _player = p; }
	void	set_player_name(char *p);
	void	set_password(char *p);

	int	queue_string(const char *, int show_literally = 0);
	int	queue_string(const CString& s, int show_literally = 0)
	{
		return queue_string(s.c_str(), show_literally);
	}
	int	queue_write(const char *, int len);

	void	send_telnet_option(unsigned char command, unsigned char option);
	void	initial_telnet_options();
	int	process_telnet_options(unsigned char *, int);
	int	__process_telnet_options(unsigned char *&, unsigned char*&, unsigned char*);
	void	get_value_from_subnegotiation(unsigned char *, unsigned char, int);

	void	set_echo(int echo);
	int	set_terminal_type(const char* terminal);
	Command_status	terminal_set_termtype(const char *, int);
	Command_status	terminal_set_lftocr(const char *, int);
	Command_status	terminal_set_pagebell(const char *, int);
	Command_status	terminal_set_width(const char *, int);
	Command_status	terminal_set_wrap(const char *, int);
	Command_status	terminal_set_echo(const char *, int);
	Command_status	really_do_terminal_set(const char *, const char *, int);
	void	do_write(const char * c, int i)
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

	int	process_output();
	int	process_input(int len = 0);
	void	shutdownsock();
	void	freeqs();

	void	welcome_user();
	void	splat_motd();
	int	check_connect(const char *msg);
	void	announce_player(announce_states state);

	void save_command (const char *command)
	{
		add_to_queue (&input, command, strlen(command)+1);
	}
	int	do_command(const char *c);

	void	dump_users(const char *victim, int flags);
	void	dump_swho();
private:
	descriptor_data();
};




struct	descriptor_data		*descriptor_list = NULL;

static	int			sock;
#ifdef CONCENTRATOR
static	int			conc_listensock, conc_sock, conc_connected;
#endif
static	int			ndescriptors = 0;
static	int			outgoing_conc_data_waiting = 0;

void				process_commands	(void);
void				make_nonblocking	(int s);
const	char			*convert_addr 		(struct in_addr *);
struct	descriptor_data		*new_connection		(int sock);
void				parse_connect 		(const char *msg, char *command, char *user, char *pass);
void				set_userstring 		(char **userstring, const char *command);
char				*strsave 		(const char *s);
int				make_socket		(int, unsigned long);
void				bailout			(int);
u_long				addr_numtoint		(char *, int);
u_long				addr_strtoint		(char *, int);
//void				get_value_from_subnegotiation (struct descriptor_data *d, unsigned char *, unsigned char, int);
#ifdef CONCENTRATOR
int				connect_concentrator	(int);
int				send_concentrator_data	(int channel, char *data, int length);
int				process_concentrator_input(int sock);
void				concentrator_disconnect	(void);
#endif

struct terminal_set_command
{
	const char	*name;
	Command_status	(descriptor_data::*set_function) (const char *, int);
} terminal_set_command_table[] =
{
	{ "termtype",	&descriptor_data::terminal_set_termtype },
	{ "width",	&descriptor_data::terminal_set_width },
	{ "wrap",	&descriptor_data::terminal_set_wrap },
	{ "lftocr",	&descriptor_data::terminal_set_lftocr },
	{ "pagebell",	&descriptor_data::terminal_set_pagebell },
	{ "echo",	&descriptor_data::terminal_set_echo },

	{ NULL, NULL }
};

#define MALLOC(result, type, number) do {			\
	if (!((result) = (type *) malloc ((number) * sizeof (type))))	\
		panic("Out of memory");				\
	} while (0)

#ifdef DEBUG
#define FREE(x) { if((x)==NULL) \
			Trace( "WARNING:  attempt to free NULL pointer (%s, line %d)\n", __FILE__, __LINE__); \
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
	char*	command;
	int	player;
public:
	LogCommand(int _player, const char*_command)
	{
		player = _player;
		if(_command)
			command = strdup(_command);
		else
			command = strdup("<null>");
	}
	~LogCommand()
	{
		FREE(command);
	}
	int get_player() { return player; }
	const char* get_command() { return command; }
};

LogCommand* LastCommands[MAX_LAST_COMMANDS];
int LastCommandsPtr = 0;

void set_signals()
{
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
			if(player == GOD_ID)
			{
				god++;
				blob = 0;
			}
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

/*******************************************************
 * I am removing the following two functions for now, as they cause
 * compiler warnings, and Peter asked me to remove as many warnings
 * as possible.
 *
 * Grimthorpe 6-June-1997
 *
static void
colour_queue_string(
struct descriptor_data *d,
const char *s,
int colour)
{
	d->queue_string(db[d->get_player()].get_colour_at()[colour]);
	d->queue_string(s);
	d->queue_string(COLOUR_REVERT);
}

static void
colour_queue_player_string(
struct descriptor_data	*d,
const char		*s,
dbref			talker)
{
	d->queue_string(player_colour(d->get_player(), talker, NO_COLOUR));
	d->queue_string(s);
	d->queue_string(COLOUR_REVERT);
}
 *
 *******************************************************/

void notify_area (dbref area, dbref originator, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;
	Boolean deruded = False,
		censorall= False;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result, sizeof(vsnprintf_result),fmt, vl);
	va_end (vl);

	if (Censored(originator))
	{
		censorall= True;
		censored=censor(vsnprintf_result);
		deruded=True;
	}

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->IS_CONNECTED() && in_area (d->get_player(), area))
		{
			if (censorall || Censorall(d->get_player()) ||
			    (Censorpublic(d->get_player()) && Public(db[d->get_player()].get_location())))
			{
				if (deruded==False)
				{
					deruded=True;
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
		if (d->IS_CONNECTED() && (Wizard (d->get_player()) || Apprentice (d->get_player())))
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
		if (d->IS_CONNECTED() && (Wizard (d->get_player())) || (Apprentice (d->get_player())))
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
			d->queue_string(db[d->get_player()].get_colour_at()[COLOUR_NATTER_TITLES]);
			d->queue_string("[WELCOMER]");
			d->queue_string(db[d->get_player()].get_colour_at()[COLOUR_NATTERS]);
			d->queue_string(vsnprintf_result);
			d->queue_string(COLOUR_REVERT);
			d->queue_string("\n");
		}
}


void colour_player_notify(dbref player, dbref victim, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			d->queue_string (player_colour(player, victim, NO_COLOUR));
			d->queue_string (vsnprintf_result);
			d->queue_string (COLOUR_REVERT);
			d->queue_string ("\n");
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
	Boolean deruded=False;
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
				if (deruded==False)
				{
					deruded=True;
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

	Boolean deruded=False;
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
				if (deruded==False)
				{
					deruded=True;
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
			twidth= d->terminal_width;
			if (twidth<=0)
				twidth = 80; // A reasonable default
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

/* notify_censor used when we want to take account of the censorship
   flags 'censored' and 'censorall' */

void notify_censor(dbref player, dbref originator, const char *fmt, ...)
{
	struct descriptor_data *d;
	va_list vl;
	Boolean deruded=False;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			if (Censorall(player) || Censored(originator))
			{
				if (deruded==False)
				{
					deruded=True;
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

	Boolean deruded=False;
	const char *censored=NULL;

	va_start (vl, fmt);
	vsnprintf (vsnprintf_result,sizeof(vsnprintf_result), fmt, vl);
	va_end (vl);

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && d->get_player() == player)
		{
			if (Censorall(player) || Censored(originator) || (Censorpublic(player) && Public(db[player].get_location())))
			{
				if (deruded==False)
				{
					deruded=True;
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
		if (d->IS_CONNECTED() && d->get_player() == player && d->terminal_pagebell)
		{
			d->queue_write ("\007", 1);
			d->process_output ();
		}
	}
}

void beep (struct descriptor_data *d)
{
	if (d->IS_CONNECTED() && d->terminal_pagebell)
	{
		d->queue_write("\007", 1);
		d->process_output();
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

void mud_main_loop(int port)
{
	fd_set			input_set, output_set;
	long			now;
	long			diff;
	struct	timeval		last_slice, current_time;
	struct	timeval		next_slice;
	struct	timeval		timeout, slice_timeout;
	int			maxd;
	int			select_value;
	struct	descriptor_data	*d, *dnext;
	struct	descriptor_data	*newd;
	int			avail_descriptors;
#ifdef CONCENTRATOR
	int			conc_data_sent;
#endif

#ifdef CONCENTRATOR
	conc_listensock = make_socket (CONC_PORT, INADDR_ANY);
#endif
	sock = make_socket (port, INADDR_ANY);
	maxd = sock+1;
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

		if (check_descriptors)
		{
			check_descriptors = 0;
			for (d = descriptor_list; d; d = dnext)
			{
				dnext = d->next;
				if (d->get_connect_state() == DESCRIPTOR_LIMBO)
					d->shutdownsock ();
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
			FD_SET (sock, &input_set);
		}
		(void) time (&now);
		for (d = descriptor_list; d; d=d->next)
		{
			if ((d->get_descriptor())	/* Don't want to watch concentrator descriptors - if you don't know why, go learn C, Keith */
			  || (!d->IS_FAKED())) /* Don't forget to exclude NPCs either */
			{
				if (d->input.head)
					timeout = slice_timeout;
				else
					FD_SET (d->get_descriptor(), &input_set);

				if (d->output.head)
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
					d->set_echo(1);
					d->welcome_user();
				}
			}
		}
		else
		{
			/* Check for new direct connection */
			if (FD_ISSET (sock, &input_set))
			{
				if (!(newd = new_connection (sock)))
				{
					if (errno != EINTR && errno != EMFILE && errno != ECONNREFUSED && errno != EWOULDBLOCK && errno != ENFILE)
					{
Trace( "new_connection returned %d, errno=%d\nThe old code would have ABORTED here, but we're continuing.", newd, errno);
						//perror ("new_connection");
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
							if(d->get_player())
								d->announce_player(ANNOUNCE_DISCONNECTED);
							d->NUKE_DESCRIPTOR ();
							continue;
						}
					}
					if (FD_ISSET (d->get_descriptor(), &output_set))
					if (d->output.head)
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
							Trace( "Wank dick, channel %d.\n", d->channel);
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
						else
							d->warning_level ++;
					}
					break;
			}
		}
	}
}

#ifdef CONCETRATOR
int connect_concentrator(int sock)
{
	struct	sockaddr_in	addr;
	int			addr_len;
	int			newsock;

	Trace( "CONCENTRATOR:  connected\n");

	addr_len=sizeof(addr);
	newsock=accept(sock, (struct sockaddr *) &addr, &addr_len);

	return newsock;
}
#endif

struct descriptor_data *new_connection(int sock)
{
	int			newsock;
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
		write (newsock, reject_message, sizeof (reject_message) - 1);
		shutdown(newsock, 2);
		close (newsock);
		errno = ECONNREFUSED;
		return (0);
	}

	int one = 1;
	if(setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) != 0)
	{
		Trace("BUG: KeepAlive option not set, errno=%d", errno);
	}

	Trace( "ACCEPT from |%s|%d| on descriptor |%d\n", convert_addr (&(addr.sin_addr)), ntohs (addr.sin_port), newsock);
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
	Trace( "Sent option '%s %s'\n", TELCMD(command), TELOPT(option));
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
		_got_an_iac = 1;	// We got an iac, we can now sort out echo handling properly.
		t_iac_command = *(buf++);
		t_iac = IAC_GOT_COMMAND;
		if(buf >= end)
		{
			return 0;
		}
	case IAC_GOT_COMMAND:
		t_iac_option = *(buf++);
#ifdef DEBUG_TELNET
		Trace( "Got option '%s %s'\n", TELCMD(t_iac_command), TELOPT(t_iac_option));
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
				t_echo = 0;
				break;
			}
			return 0; // We've handled a telnet option

		case WONT:
// Client wont do LINEMODE! We're out of luck, so provide standard support
// but nothing fancy. If the client end can't cope with this, then tough,
// as we don't want the hastle of providing full line editing functions.
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
				t_echo = 0;
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
				Trace("Telnet option warning, descriptor %d:  %x isn't IAC SE\n", get_descriptor(), c);
			t_iacbuf[t_piacbuf]=0;
#ifdef DEBUG_TELNET
			Trace( "Got command '%s' for '%s'\n", TELCMD(t_iac_command), TELOPT(t_iac_option));
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
descriptor_data::set_echo(int echo)
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
			terminal_width=buf[0]*256 + buf[1];
			terminal_height=buf[2]*256 + buf[3];
#ifdef DEBUG_TELNET
			Trace( "Descriptor %d terminal dimensions:  %d x %d\n", get_descriptor(), terminal_width, terminal_height);
#endif
			if(terminal_width < 20 || terminal_height < 5)
			{
				Trace( "Descriptor %d gave silly terminal dimensions (%d x %d)\n", get_descriptor(), terminal_width, terminal_height);
				terminal_width=0;
				terminal_height=0;
			}
			send_telnet_option(WONT, TELOPT_NAWS);	/* To make sure we don't have to send it */
			break;

		case TELOPT_TTYPE:
			memcpy(scratch, buf+1, size);
			scratch[size]=0;
#ifdef DEBUG_TELNET
			Trace( "Descriptor %d terminal type is '%s'\n", get_descriptor(), scratch);
#endif
			set_terminal_type(scratch);
			send_telnet_option(WONT, TELOPT_TTYPE);	/* To make sure we don't have to send it */
			break;
		
		case TELOPT_SNDLOC:
			if(address.sin_addr.s_addr != LOGTHROUGH_HOST)
			{
				Trace( "Descriptor %d sent a SNDLOC, but isn't connected from LOGTHROUGH_HOST\n", get_descriptor());
				break;
			}
			memcpy(scratch, buf, size);
			scratch[size-1]=0;
#ifdef DEBUG_TELNET
			Trace( "Descriptor %d location is '%s'\n", get_descriptor(), scratch);
#endif
			indirect_connection=1;
			strcpy(hostname, scratch);
			send_telnet_option(WONT, TELOPT_SNDLOC);
			break;
	}
}

#ifdef USE_TERMINFO
/*
 * I've done a blatent cut and paste of this code for the other version so if
 * you update one you should check the other.  I was going to go through having
 * lots of ifdefs so it was more obviously how things tied up but it made the
 * totally unreadable.  -Abstract
 */
int descriptor_data::set_terminal_type (const char *const termtype)
{
	char *terminal;

	terminal = safe_strdup(termtype);

	for(int i = 0; terminal[i]; i++)
		terminal[i] = tolower(terminal[i]);

	int setupterm_error;
	if (setupterm (terminal, get_descriptor(), &setupterm_error) != OK)
	{
		if (setupterm_error == -1)
			Trace( "BUG:  Terminfo database could not be found.\n");
#ifdef DEBUG_TELNET
		else if (setupterm_error == 0)
			Trace( "BUG: Terminal type '%s' from descriptor %d not in the terminfo database\n", terminal, get_descriptor());
#endif
		FREE(terminal);
		return 0;
	}

	if (terminal_type)
		FREE(terminal_type);

	terminal_type = safe_strdup(terminal);
	FREE(terminal);

	if (terminal_width == 0)
	{
		terminal_width = tigetnum("cols");
#ifdef DEBUG_TELNET
		Trace( "Terminal width (from terminfo):  %d\n", terminal_width);
#endif
	}
	if (terminal_height == 0)
	{
		terminal_height = tigetnum("lines");
#ifdef DEBUG_TELNET
		Trace( "Terminal height (from terminfo):  %d\n", terminal_height);
#endif
	}

/*	const char *const clearline = tigetstr("dl1");
 *	if (clearline != (char *)-1)
 *	{
 *		if (termcap.clearline)
 *			FREE(termcap.clearline);
 *		termcap.clearline = safe_strdup(clearline);
 *	}
 *
 *	const char *const backspace = tigetstr("dch1");
 *	if (backspace != (char *)-1)
 *	{
 *		if (termcap.backspace)
 *			FREE (termcap.backspace);
 *		termcap.backspace = safe_strdup(backspace);
 *	}
 */

	const char *const bold = tigetstr("bold");
	if (bold != (char *)-1)
	{
		if (termcap.bold_on)
			FREE (termcap.bold_on);
		termcap.bold_on = safe_strdup(bold);
	}

	const char *const bold_off = tigetstr("sgr0");
	if (bold_off != (char *)-1)
	{
		if (termcap.bold_off)
			FREE(termcap.bold_off);
		termcap.bold_off = safe_strdup(bold_off);
	}

	const char *const underline = tigetstr("smul");
	if (underline != (char *)-1)
	{
		if (termcap.underscore_on)
			FREE(termcap.underscore_on);
		termcap.underscore_on = safe_strdup(underline);
	}

	const char *const underscore_off = tigetstr("rmul");
	if (underscore_off != (char *)-1)
	{
		if (termcap.underscore_off)
			FREE(termcap.underscore_off);
		termcap.underscore_off = safe_strdup(scratch);
	}

	return 1;
}

#else /* !USE_TERMINFO */
/*
 * If you update this you should also check the version above!
 */
int
descriptor_data::set_terminal_type(const char *termtype)
{
	static char ltermcap[1024];
	char *terminal, *area;
	int i;

	terminal=safe_strdup(termtype);

	for(i=0; terminal[i]; i++)
		terminal[i]=tolower(terminal[i]);

	if(tgetent(ltermcap, terminal)!=1)
	{
#ifdef DEBUG_TELNET
		Trace( "BUG: Terminal type '%s' from descriptor %d not in /etc/termcap\n", terminal, get_descriptor());
#endif
		FREE(terminal);
		return 0;
	}

	if(terminal_type)
		FREE(terminal_type);

	terminal_type=safe_strdup(terminal);
	FREE(terminal);

	if(terminal_width == 0)
	{
		terminal_width=tgetnum("co");
#ifdef DEBUG_TELNET
	Trace( "Terminal width (from termcap):  %d\n", terminal_width);
#endif
	}
	if(terminal_height == 0)
	{
		terminal_height=tgetnum("li");
#ifdef DEBUG_TELNET
	Trace( "Terminal height (from termcap):  %d\n", terminal_height);
#endif
	}

/*	area=scratch;
	if(tgetstr("dl", &area))
	{
		if(termcap.clearline)
			FREE(termcap.clearline);
		termcap.clearline=safe_strdup(scratch);
	}

	area=scratch;
	if(tgetstr("dc", &area))
	{
		if(termcap.backspace)
			FREE(termcap.backspace);
		termcap.backspace=safe_strdup(scratch);
	}*/

	area=scratch;
	if(tgetstr("md", &area))
	{
		if(termcap.bold_on)
			FREE(termcap.bold_on);
		termcap.bold_on=safe_strdup(scratch);
	}

	area=scratch;
	if(tgetstr("me", &area))
	{
		if(termcap.bold_off)
			FREE(termcap.bold_off);
		termcap.bold_off=safe_strdup(scratch);
	}

	area=scratch;
	if(tgetstr("us", &area))
	{
		if(termcap.underscore_on)
			FREE(termcap.underscore_on);
		termcap.underscore_on=safe_strdup(scratch);
	}

	area=scratch;
	if(tgetstr("ue", &area))
	{
		if(termcap.underscore_off)
			FREE(termcap.underscore_off);
		termcap.underscore_off=safe_strdup(scratch);
	}


	return 1;
}

#endif /* USE_TERMINFO */

const char *
convert_addr (
struct	in_addr	*a)

{
	struct	hostent	*he;
	static	char	buffer [MAX(20, MAXHOSTNAMELEN)];
	char		compare_buffer [MAXHOSTNAMELEN];
	u_long		addr = ntohl (a->s_addr);

	if (smd_dnslookup (addr) && ((he = gethostbyaddr ((char *) a, sizeof(*a), AF_INET)) != NULL))
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
		/* Not local, or not found in db */
		snprintf (buffer, sizeof(buffer), "%ld.%ld.%ld.%ld", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
	}
	return (buffer);
}

void
descriptor_data::clearstrings()
{
	if (_output_prefix)
		FREE (_output_prefix);

	if (_output_suffix)
		FREE (_output_suffix);

	if (terminal_type)
		FREE (terminal_type);

	if (termcap.bold_on)
		FREE (termcap.bold_on);

	if (termcap.bold_off)
		FREE (termcap.bold_off);

	if (termcap.backspace)
		FREE (termcap.backspace);

	if (termcap.clearline)
		FREE (termcap.clearline);

	if (_player_name)
		FREE (_player_name);

	if (_password)
		FREE (_password);
}

void
descriptor_data::shutdownsock()
{
	time_t stamp;
	struct tm *now;
/* For NPC connections, ignore most of this */
	if(!IS_FAKED())
	{
		if (get_player() != 0)
		{
			mud_disconnect_player (get_player());
			time (&stamp);
			now = localtime (&stamp);
			Trace( "DISCONNECT descriptor |%d|%d| player |%s|%d| at |%02d/%02d/%02d %02d:%02d\n",
			CHANNEL(),
			channel,
			getname (get_player()),
			get_player(),
			now->tm_year,
			now->tm_mon+1,
			now->tm_mday,
			now->tm_hour,
			now->tm_min);
		}
		else
		{
			Trace( "DISCONNECT descriptor |%d| never connected\n",
			CHANNEL());
		}
	}
	process_output ();
	if(get_descriptor())	/* If it's not a concentrator connection */
	{
		shutdown (get_descriptor(), 2);
		close (get_descriptor());
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
	*prev = next;
	if (next)
		next->prev = prev;

	clearstrings ();
	delete (this);
	ndescriptors--;
}


/* Call with (socket, address) for normal connections, or (0, NULL, channel) for concentrator connections. */


descriptor_data::descriptor_data (
int			s,
struct	sockaddr_in	*a,
int			channel)

{
	long now;

	(void) time (&now);

	ndescriptors++;
	_descriptor		= s;
	channel			= channel;
	_player			= 0;
	_player_name		= NULL;
	_password		= NULL;
	connect_attempts	= 3;
	if(s) make_nonblocking (s);
	_output_prefix		= NULL;
	_output_suffix		= NULL;
	output_size		= 0;
	output.head		= NULL;
	output.tail		= &output.head;
	input.head		= NULL;
	input.tail		= &input.head;
	raw_input		= NULL;
	quota			= COMMAND_BURST_SIZE;
	start_time		= now;
	last_time		= now;
	warning_level		= 0;
/*
 * The following threee variables have never been initialised until now.
 * For some reason the code worked before. I assume that was luck.
 * They have been fixed mainly because Purify shouts loudly at them
 * being used unitialised.
 * If things break, then just comment out these lines!
 *
 * Grimthorpe, 29-June-2000
 */
	t_echo			= 0; // We've never set these before, and it
	t_linemode		= 0; // worked for some reason.
	t_lflow			= 0; // If it breaks now, then please analyse

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

	indirect_connection     = 0;
	if(a)
	{
		address = *a;
		strncpy (hostname,
			convert_addr (&(a->sin_addr)),
			sizeof (hostname) - 1);
		hostname [sizeof (hostname) - 1] = '\0';
        }
        else
		strcpy (hostname, "Concentrator");

        if (descriptor_list)
                descriptor_list->prev = &next;

        next			= descriptor_list;
	prev			= &descriptor_list;
	terminal_xpos		= 0;
	terminal_width		= 0;
	terminal_height		= 0;
	terminal_type		= NULL;
	terminal_xpos		= 0;
	terminal_lftocr		= 1;
	terminal_pagebell	= 1;
	terminal_wrap		= 1;
	termcap.bold_on		= NULL;
	termcap.bold_off	= NULL;
	termcap.underscore_on	= NULL;
	termcap.underscore_off	= NULL;
// Naive assumption of terminal capabilities, which will probably work.
	termcap.backspace	= safe_strdup("\010 \010");
	termcap.clearline	= safe_strdup("^R\r\n");

	backslash_pending	= 0;
	cr_pending		= 0;

	last_time		= time(NULL);

	descriptor_list		= this;

	t_iac			= IAC_OUTSIDE_IAC;

	_got_an_iac		= 0;

	set_echo(1);
}


int make_socket(int port, unsigned long inaddr)
{
	int s;
	struct sockaddr_in server;
	int opt;

	s = socket (AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		perror ("creating stream socket");
		exit (3);
	}

	opt = 1;

	if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof (opt)) < 0)
	{
		perror ("setsockopt");
		exit (1);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inaddr;
	server.sin_port = htons (port);
	if (bind (s, (struct sockaddr *) & server, sizeof (server)))
	{
		perror ("binding stream socket");
		close (s);
		exit (4);
	}
	listen (s, 5);
	return s;
}

struct text_block *make_text_block(const char *s, int n)
{
	struct text_block *p;

	MALLOC(p, struct text_block, 1);
	MALLOC(p->buf, char, n);
	memcpy (p->buf, s, n);
	p->nchars = n;
	p->start = p->buf;
	p->nxt = NULL;
	return p;
}

void free_text_block (struct text_block *t)
{
	FREE (t->buf);
	FREE ((char *) t);
	t = NULL;
}

void add_to_queue(struct text_queue *q, const char *b, int n)
{
	struct text_block *p;

	if (n == 0)
		return;

	p = make_text_block (b, n);
	p->nxt = NULL;
	*q->tail = p;
	q->tail = &p->nxt;
}

int flush_queue(struct text_queue *q, int n)
{
	struct text_block *p;
	int really_flushed = 0;
	
	n += strlen(flushed_message);

	while (n > 0 && (p = q->head))
	{
		n -= p->nchars;
		really_flushed += p->nchars;
		q->head = p->nxt;
		free_text_block (p);
	}
	p = make_text_block(flushed_message, strlen(flushed_message));
	p->nxt = q->head;
	q->head = p;
	if (!p->nxt)
	    q->tail = &p->nxt;

	really_flushed -= p->nchars;
	return really_flushed;
}

int
descriptor_data::queue_write(const char *b, int n)
{
	int space;

	if(IS_FAKED())
		return n;

	space = MAX_OUTPUT - output_size - n;
	if (space < 0)
		output_size -= flush_queue(&output, -space);
	add_to_queue (&output, b, n);
	output_size += n;

	if (!get_descriptor())
		outgoing_conc_data_waiting = 1;

	return n;
}

/* Note to stop this from substituting colours
   (eg who strings), call queue_string(s,1).
   To get colour, just use as normal, queue_string(s)
	- Dunk 3/2/96
*/
int
descriptor_data::queue_string(const char *s, int show_literally)
{
static char b1[2*BUFFER_LEN];
static char b2[2*BUFFER_LEN];
static char OUTPUT_COMMAND[] = ".output ";
static char MYOUTPUT_COMMAND[] = ".myoutput ";
char *a,*a1,*b;

	if(get_connect_state()==DESCRIPTOR_LIMBO)
	{
		Trace( "WARNING:  Attempt to queue_string to a limbo'd descriptor\n");
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
				default:
					*(raw_input_at++) = *(s++);
					break;
			}
		}
		return 1;
	}

	strcpy(b2, s);
	b=b2;

// Do word-wrap.
// We want to wrap at the last <space> before the end-of-line. If there isn't
// one, don't try to wrap.
	if((terminal_width > 0) && (terminal_wrap))
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

			case '\n':
				terminal_xpos = 0; // We increment the position
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
				terminal_xpos--;
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
			if(terminal_xpos+1 >= terminal_width)
			{
				if(last_space != 0)
				{
					a = last_space;
					last_space = 0;
					*a++ = '\n';
				}
				terminal_xpos = 0;
				a++;
			}
			else
			{
				terminal_xpos++;
				a++;
			}
		}
	}
	if(terminal_xpos < 0)
		terminal_xpos = 0;
	if(terminal_lftocr)
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

	int queue_count = 0;		/* Collates all the results from the queue_writes */
	int colour = 0;
	if(get_player())
		colour = Colour(get_player());  /* Stores whether we need to substitute in the codes */
	int percent_loaded = 0;		/* Indicates whether we have already hit a % sign */
	int bcount = 0;			/* This steps along string copying from */
	int cocount = 0;		/* The counts along output buffer */
	char cobuf[BUFFER_LEN];		/* This is the output buffer, once full, output and
					   start again */


	while(b[bcount])	
	{
		/* Step through the string until the buffer gets full, minus a safety margin */
		while(b[bcount] && (bcount % BUFFER_LEN) < (BUFFER_LEN - 64))
		{
			if(b[bcount] == '%')
			{
				if((percent_loaded) || (show_literally))
				{
					/* Deal with %% which displays a % sign */
					cobuf[cocount++] = '%';
					percent_loaded = 0;
				}
				else
				{
					percent_loaded = 1;
				}
			}
			else
			{
				if(percent_loaded)
				{
					percent_loaded = 0;
					if(colour)
					{
						/* They want the colour codes transfered*/

						if((b[bcount] >= 'A') && (b[bcount] <= 'z'))
						{
							/* Find the entry in the colour table by
							   directly referencing it. 'A' is 0 */
							strcpy(&cobuf[cocount], colour_table[(b[bcount] - 65)].ansi);
							cocount += strlen(colour_table[(b[bcount] - 65)].ansi);
						}
					}
				}
				else
				{
					/* Just a character, copy it */
					cobuf[cocount++] = b[bcount];
				}
			}
			bcount++;
		}

		cobuf[cocount] = '\0';
		queue_count += queue_write (cobuf, strlen (cobuf));
		cocount = 0;
	}
	return queue_count;
}

int
descriptor_data::process_output()
{
	struct text_block **qp, *cur;
	int cnt;

	for (qp = &output.head; (cur = *qp);)
	{
		if(IS_FAKED())
		{
			cnt = cur->nchars;
		}
		else if (get_descriptor())
		{
			cnt = write (get_descriptor(), cur -> start, cur -> nchars);
			if (cnt < 0)
			{
				if (errno == EWOULDBLOCK)
					return 1;

				return 0;
			}
		}
#ifdef CONCENTRATOR
		else
		{
			if ((cnt = send_concentrator_data (channel, cur->start, cur->nchars))<1)
			{
				Trace("Concentrator disconnect in write() (process_output)\n");
				return 0;
			}
		}
#endif

		output_size -= cnt;
		if (cnt == cur -> nchars)
		{
			if (!cur -> nxt)
			output.tail = qp;
			*qp = cur -> nxt;
			free_text_block (cur);
			continue;		/* do not adv ptr */
		}
		cur -> nchars -= cnt;
		cur -> start += cnt;
		break;
	}
	return 1;
}

void make_nonblocking(int s)
{
#ifdef FNDELAY
	if (fcntl (s, F_SETFL, FNDELAY) == -1)
#else
	if (fcntl (s, F_SETFL, O_NDELAY) == -1)
#endif
	{
		perror ("make_nonblocking: fcntl");
		panic ("FNDELAY fcntl failed");
	}
}

void
descriptor_data::freeqs()
{
	struct text_block *cur, *next;

	cur = output.head;
	while (cur)
	{
		next = cur -> nxt;
		free_text_block (cur);
		cur = next;
	}
	output.head = NULL;
	output.tail = &output.head;

	cur = input.head;
	while (cur)
	{
		next = cur -> nxt;
		free_text_block (cur);
		cur = next;
	}
	input.head = NULL;
	input.tail = &input.head;

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
		fputs("BUG:", stderr);
		perror(WELCOME_FILE);
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

	if(d->termcap.bold_on && d->termcap.bold_off && d->terminal_type)
		snprintf(buf, sizeof(buf), "%s%s%s", d->termcap.bold_on, str, d->termcap.bold_off);
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

//	if(d->termcap.underscore_on && d->termcap.underscore_off && d->terminal_type)
//		snprintf(buf, sizeof(buf), "%s%s%s", d->termcap.underscore_on, str, d->termcap.underscore_off);
//	else
		strcpy(buf, str);

	return buf;
}


void
descriptor_data::announce_player (announce_states state)
{
	struct descriptor_data *d;
	char	admin_buffer[1024];
// This is the original line. Trying to get rid of unix time of connect time
// being displayed for admin listening. Its suspect that its something to do
// with scratch_return_string
//	char	scratch_return_string [BUFFER_LEN];
	char	scratch_return_string [1024];
	Boolean deruded=False;
	const char *censored=NULL;

	switch (state)
	{
		case ANNOUNCE_CONNECTED :
			snprintf (scratch, sizeof(scratch), "[%s has connected]\n", db[get_player()].get_name ().c_str());
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has connected from %s]\n", db[get_player()].get_name ().c_str(), hostname);
			break;
		case ANNOUNCE_CREATED :
			snprintf (scratch, sizeof(scratch), "[%s has connected]\n", db[get_player()].get_name().c_str());
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has been created from %s]\n", db[get_player()].get_name ().c_str(), hostname);
			snprintf (admin_buffer, sizeof(admin_buffer), "[%s has been created]\n", db[get_player()].get_name().c_str());
			break;
		case ANNOUNCE_BOOTED :
			snprintf (scratch, sizeof(scratch), "[%s has been booted by %s%s%s]\n", db[get_player()].get_name ().c_str(), player_booting, blank (boot_reason)?"":" ", boot_reason);
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has been booted from %s by %s%s%s]\n", db[get_player()].get_name ().c_str(), hostname, player_booting, blank (boot_reason)?"":" ", boot_reason);
			break;
		case ANNOUNCE_DISCONNECTED :
			snprintf (scratch, sizeof(scratch), "[%s has disconnected]\n", db[get_player()].get_name ().c_str());
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has disconnected from %s]\n", db[get_player()].get_name ().c_str(), hostname);
			break;
		case ANNOUNCE_SMD :
			snprintf (scratch, sizeof(scratch), "[%s has disconnected]\n", db[get_player()].get_name ().c_str());
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has disconnected from %s due to an SMD read]\n", db[get_player()].get_name ().c_str(), hostname);
			break;
		case ANNOUNCE_TIMEOUT :
			snprintf (scratch, sizeof(scratch), "[%s has disconnected]\n", db[get_player()].get_name ().c_str());
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has timed out from %s]\n", db[get_player()].get_name ().c_str(), hostname);
			break;
		case ANNOUNCE_PURGE :
			snprintf (scratch, sizeof(scratch), "[%s has purged an idle connection]\n", db[get_player()].get_name ().c_str());
			snprintf (scratch_return_string, sizeof(scratch_return_string), "[%s has purged an idle connection from %s]\n", db[get_player()].get_name ().c_str(), hostname);
			break;
		default :
			Trace( "BUG: Unknown state (%d) encounted in announce_player()\n", state);
			break;
	}

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->IS_CONNECTED () && Listen (d->get_player()))
		{
			if (Wizard (d->get_player()))
				d->queue_string (underscorify (d->get_player(), scratch_return_string));
			else
			{
				if (( Apprentice(d->get_player()) || Welcomer (d->get_player())) && ( state == ANNOUNCE_CREATED))
					d->queue_string (underscorify (d->get_player(), admin_buffer));
				else
				{
					if (Censorall(d->get_player()) || Censorpublic(d->get_player()))
					{
						if (deruded==False)
						{
							deruded=True;
							censored=censor(scratch);
						}
						d->queue_string (underscorify (d->get_player(), censored));
					}
					else
						d->queue_string (underscorify (d->get_player(), scratch));
				}
			}
		}
	}
}


char *strsave (const char *s)
{
	char *p;

	MALLOC (p, char, strlen(s) + 1);
	if (p)
		strcpy (p, s);

	return p;
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
		set_echo(1);
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
					backslash_pending = 0;
				}
				else
					backslash_pending = 1;
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
					t_echo = 0;
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
					backslash_pending = 0;
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
do_write(termcap.backspace, strlen(termcap.backspace));
				x = raw_input;
				while(*x && (x < p))
				{
					if((*(x++) == '\\')
						 && (backslash_pending == 0))
						backslash_pending = 1;
					else
						backslash_pending = 0;
				}
				break;
			case '\022':	// ^R
				do_write((const char *)termcap.clearline, strlen(termcap.clearline));
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
				MALLOC(bs, char, (l * strlen(termcap.backspace) + 5));
				*bs = '\0';
				for(; l > 0; l--)
					strcat(bs, termcap.backspace);
				do_write(bs, strlen(bs));
				FREE(bs);
				p = x;
				*p = '\0';
				break;
			}
			default:
				if (p < pend && isascii (*q) && isprint (*q))
				{
charcount++;
					if (backslash_pending)
					{
						if (p + 1 < pend)
							*p++ = '\\';
						backslash_pending = 0;
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
		t_echo = 1;
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


void set_userstring (char **userstring, const char *command)
{
	if (*userstring)
	{
		FREE (*userstring);
		*userstring = NULL;
	}
	while (*command && isascii (*command) && isspace (*command))
		command++;

	if (*command)
		*userstring = strsave (command);
}

void
process_commands()
{
	int			nprocessed;
	struct	descriptor_data	*d, *dnext;
	struct	text_block	*t;

	do
	{
		nprocessed = 0;
		for (d = descriptor_list; d; d = dnext)
		{
			dnext = d->next;
			if (d->get_connect_state() != DESCRIPTOR_LIMBO && d -> quota > 0 && (t = d -> input.head))
			{
				d -> quota--;
				nprocessed++;
				if (!d->do_command (t -> start))
				{
					if (d->get_connect_state() == DESCRIPTOR_CONNECTED)
						d->announce_player (ANNOUNCE_DISCONNECTED);
					d->NUKE_DESCRIPTOR ();
				}
//				else
//				{
					d -> input.head = t -> nxt;
					if (!d -> input.head)
						d -> input.tail = &d -> input.head;
					free_text_block (t);
//				}
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
	long			now;
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
const	char	*,
const	char	*)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Sorry, 'quit' as a command is not implemented. (Type 'QUIT' in capitals to leave the game).");
	return;
}

	
int
descriptor_data::do_command (const char *command)
{
	FILE		*fp;
#ifdef sun
	extern	char	*sys_errlist[];
#endif

	if (!strcmp (command, QUIT_COMMAND))
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
	else if (!strcmp (command, WHO_COMMAND))
	{
		output_prefix();
		dump_users (NULL, (IS_CONNECTED() && (db[get_player()].get_flag(FLAG_HOST))) ? DUMP_WIZARD : 0);
		output_suffix();
	}
	else if (!strcmp (command, IDLE_COMMAND))
	{
		queue_string ("Idle time increased to 3 minutes.\n");
		last_time = (time_t) time(NULL) - IDLE_TIME_INCREASE;
	}
	else if (!strcmp (command, INFO_COMMAND))
	{
		if ((fp=fopen (HELP_FILE, "r"))==NULL)
			Trace("BUG:  %s: %s\n",HELP_FILE,sys_errlist[errno]);
		else
		{
			while (fgets(scratch, BUFFER_LEN, fp)!=NULL)
				queue_string (scratch);
		}
		fclose(fp);
	}
	else if(!strcmp (command, SET_COMMAND))
	{
		/* Ensure we don't trash a constant string - PJC 23/12/96 */
		char	*copied_command = safe_strdup (command);
		char	*a1;
		char	*a2;
		char	*q;
		char	*endarg;

		endarg=copied_command+strlen(SET_COMMAND);

		/* This is nicked from game.c */

                /* move over command word */
                for(a1 = copied_command; *a1 && !isspace(*a1); a1++)
                        ;

                /* NUL-terminate command word */
                if(*a1)
                        *a1++ = '\0';

                /* Make sure arg2 points to NULL if there isn't a 2nd argument */
                if(a1 <= endarg)
                {
                        /* move over spaces */
                        while(*a1 && isspace(*a1))
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

		really_do_terminal_set(a1, a2, 0);
		free (copied_command);
	}
	else if (!strncmp (command, PREFIX_COMMAND, strlen (PREFIX_COMMAND)))
		set_output_prefix(command+strlen(PREFIX_COMMAND));
	else if (!strncmp (command, SUFFIX_COMMAND, strlen (SUFFIX_COMMAND)))
		set_output_suffix(command+strlen(SUFFIX_COMMAND));
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
			return check_connect (command);
		}
	}
	return 1;
}

int
descriptor_data::check_connect (const char *msg)
{
	char command[MAX_COMMAND_LEN];
	char luser[MAX_COMMAND_LEN];
	char lpassword[MAX_COMMAND_LEN];
	long now;
	struct tm *the_time;
	dbref player;

	parse_connect (msg, command, luser, lpassword);

	(void) time (&now);
	the_time = localtime (&now);
	if (!strncmp (command, "co", 2))
	{
		player = connect_player (luser, lpassword);
		if (player == NOTHING || player == 0)
		{
			queue_string (connect_fail);
			Trace( "FAILED CONNECT |%s| on descriptor |%d\n", luser, CHANNEL());
		}
		else
		{
			if(smd_cantuseguests(ntohl(address.sin_addr.s_addr)) && (!strcasecmp(luser, "guest")))
			{
				queue_string(guest_create_banned);
				Trace( "BANNED GUEST |%s| on descriptor |%d\n", luser, CHANNEL());
			}
			else
			{
				Trace( "CONNECTED |%s|%d| on descriptor |%d|at |%02d/%02d/%02d %02d:%02d\n",
				 db[player].get_name().c_str(), player, CHANNEL(), the_time->tm_year, the_time->tm_mon + 1, the_time->tm_mday, the_time->tm_hour, the_time->tm_min);
				set_connect_state(DESCRIPTOR_CONNECTED);
				set_player( player );
				mud_connect_player (get_player());
				announce_player (ANNOUNCE_CONNECTED);
			}
		}
	}
	#ifndef NEXUS
	else if (!strncmp (command, "cr", 2))
	{
		if(smd_cantcreate(ntohl(address.sin_addr.s_addr)))
		{
			queue_string(create_banned);
			Trace( "BANNED CREATE |%s| on descriptor |%d\n", luser, CHANNEL());
		}
		else

		{
			player = create_player (luser, lpassword);
			if (player == NOTHING)
			{
				queue_string (create_fail);
				Trace( "FAILED CREATE |%s| on descriptor |%d\n",
				luser, CHANNEL());
			}
			else
			{
				Trace( "CREATED |%s|%d| on descriptor |%d| at |%02d/%02d/%02d %02d:%02d\n",
				db[player].get_name().c_str(), player, CHANNEL(), the_time->tm_year, the_time->tm_mon + 1, the_time->tm_mday, the_time->tm_hour, the_time->tm_min);
				set_connect_state(DESCRIPTOR_CONNECTED);
				set_player( player );
				mud_connect_player (player);
				announce_player (ANNOUNCE_CREATED);
			}
		}
	}
	#endif /* NEXUS */
	else if (*command!='\0' && *luser=='\0' && *lpassword=='\0')
	{
		switch(get_connect_state())
		{
			case DESCRIPTOR_NAME:
				if(smd_cantuseguests(ntohl(address.sin_addr.s_addr)) && (!strcasecmp(command, "guest")))
				{
					queue_string(guest_create_banned);
					Trace( "BANNED GUEST |%s| on descriptor |%d\n", luser, CHANNEL());
					welcome_user();
				}
				else
				{
					set_player_name(safe_strdup(command));
					if(lookup_player(NOTHING, command) == NOTHING)
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
					else
					{
						set_echo(0);
						queue_string (password_prompt);
						set_connect_state(DESCRIPTOR_PASSWORD);
					}

				}
				break;

			case DESCRIPTOR_NEW_CHARACTER_PROMPT:
				if(strncasecmp(command, "yes", strlen(command))==0 || strncasecmp(command, "no", strlen(command))==0)
				{
					*command=tolower(*command);
					if(*command=='y')
					{
						if(smd_cantcreate(ntohl(address.sin_addr.s_addr)))
						{
							queue_string(create_banned);
							Trace( "BANNED CREATE |%s| on descriptor |%d\n", luser, CHANNEL());
							queue_string (name_prompt);
							set_connect_state(DESCRIPTOR_NAME);
						}
						else
						{
							queue_string (new_password_prompt);
							set_echo (0);
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
				set_echo(1);
				queue_string ("\n");
				if(ok_password(command))
				{
					player = connect_player (get_player_name(), command);
					if (player == NOTHING)
					{
						/* New player, password is ok too */
						set_password(safe_strdup(command));	/* For confirmation */
						queue_string (confirm_password);
						set_echo(0);
						set_connect_state(DESCRIPTOR_CONFIRM_PASSWORD);
					}
					else if (player == 0)
					{
						/* Invalid password */
						queue_string (connect_fail);
						Trace( "FAILED CONNECT |%s| on descriptor |%d\n",
						get_player_name(), CHANNEL());
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
						Trace( "CONNECTED |%s|%d| on descriptor |%d| at |%02d/%02d/%02d %02d:%02d\n",
						db[player].get_name().c_str(), player, CHANNEL(), the_time->tm_year, the_time->tm_mon + 1, the_time->tm_mday, the_time->tm_hour, the_time->tm_min);
						set_connect_state(DESCRIPTOR_CONNECTED);
						set_player( player );
						mud_connect_player (player);
						announce_player (ANNOUNCE_CONNECTED);
					}
				}
					
				break;


			case DESCRIPTOR_CONFIRM_PASSWORD:
				set_echo (1);
				if(strcmp(command, get_password())==0)
				{
					player = create_player (get_player_name(), get_password());
					if (player == NOTHING)
					{
						queue_string (create_fail);
						Trace( "BUG: FAILED CREATE %s on descriptor %d (THIS SHOULD NOT HAPPEN!)\n",
						luser, CHANNEL());
						queue_string (name_prompt);
						set_connect_state(DESCRIPTOR_NAME);
					}
					else
					{
						Trace( "CREATED |%s|%d| on descriptor |%d| at |%02d/%02d/%02d %02d:%02d\n",
						db[player].get_name().c_str(), player, CHANNEL(), the_time->tm_year, the_time->tm_mon + 1, the_time->tm_mday, the_time->tm_hour, the_time->tm_min);
						set_connect_state(DESCRIPTOR_CONNECTED);
						set_player ( player );
						mud_connect_player (player);
						announce_player (ANNOUNCE_CREATED);
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
				Trace( "BUG: check_connect called with connect_state==DESCRIPTOR_CONNECTED or DESCRIPTOR_LIMBO\n");
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

	while (*msg && isascii(*msg) && isspace (*msg))
		msg++;

	p = command;
	while (*msg && isascii(*msg) && !isspace (*msg))
		*p++ = *msg++;

	*p = '\0';
	while (*msg && isascii(*msg) && isspace (*msg))
		msg++;

	p = user;
	while (*msg && isascii(*msg) && !isspace (*msg))
		*p++ = *msg++;

	*p = '\0';
	while (*msg && isascii(*msg) && isspace (*msg))
		msg++;

	p = pass;
	while (*msg && isascii(*msg) && !isspace (*msg))
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
//		d->process_output ();
//		d->clearstrings ();
		if (d->get_descriptor())
		{
			if (shutdown (d->get_descriptor(), 2) < 0)
				perror ("shutdown");
			close (d->get_descriptor());
		}
	}
#ifdef CONCENTRATOR
	if (conc_connected)
		shutdown (conc_sock, 2);
#endif

	close (sock);
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
#ifdef linux
// Yuck - I feel ill JPK
	extern const char * const sys_siglist[];
	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, sys_siglist[sig]);
#else
	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, strsignal(sig));
#endif
//#if defined (sun) // Sun's different
//	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, strsignal(sig));
//#elif defined (SYSV)
//	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, sys_siglist[sig]);
//#else
//	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, strsignal(sig));
//#endif

// #if !defined (SYSV) // Fixing signal naming...
// 	extern char *sys_siglist[];
//
// 	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, sys_siglist[sig]);
// #else
// 	snprintf (message, sizeof(message), "BAILOUT: caught signal %d (%s)", sig, strsignal(sig));
// #endif

// Dump the last MAX_LAST_COMMANDS out.
	for(int i = 0; i < MAX_LAST_COMMANDS; i++)
	{
		int j = (i + LastCommandsPtr) % MAX_LAST_COMMANDS;
		LogCommand* l = LastCommands[j];
		if(l)
		{
			fprintf(stderr, "LASTCOMMAND:%d Player|%d|:%s\n", MAX_LAST_COMMANDS - i, l->get_player(), l->get_command());
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


/* Warning: don't forget when doing colour that e->player might be zero (ie. WHO before connecting) */

void
descriptor_data::dump_users (
const	char		*victim,
int			flags)

{
	struct	descriptor_data	*d;
	long			now;
	char			buf[1024];
	char			flag_buf [256];
	int			length;
	int			users = 0;
	int			local = 0, inet = 0, logthrough = 0;
	int			builder = 0, wizard = 0, haven = 0;
	int			uptime;
	int			gotonealready = 0;
	int			want_me = 0;
	int			want_wizards = 0;
	int			want_apprentices = 0;
	int			want_guests = 0;
	int			want_officers = 0;
	int			want_builders = 0;
	int			want_xbuilders = 0;
	int			want_fighters = 0;
	int			want_welcomers = 0;
	int			want_npcs = 0;
	int			want_area_who = 0;
	int 			wanted_location;

	int			firstchar;

	(void) time (&now);

	if (victim)
	{
		if (!(strcasecmp(victim, "me")))
			want_me = 1;
		if (!(strcasecmp(victim, "wizards")))
			want_wizards = 1;
		if (!(strcasecmp(victim, "apprentices")))
			want_apprentices = 1;
		if (!(strcasecmp(victim, "admin")))
			want_wizards = want_apprentices = 1;
		if (!(strcasecmp(victim, "guests")))
			want_guests = 1;
		if (!(strcasecmp(victim, "officers")))
			want_officers = 1;
		if (!(strcasecmp(victim, "builders")))
			want_builders = 1;
		if (!(strcasecmp(victim, "xbuilders")))
			want_xbuilders = 1;
		if (!(strcasecmp(victim, "fighters")))
			want_fighters = 1;
		if ((!strcasecmp(victim, "welcomers")) || (!strcasecmp(victim, "welcomer")))
			want_welcomers = 1;
		if (!(strcasecmp(victim, "npc")) || (!strcasecmp(victim, "npcs")))
			want_npcs=1;
		if (victim[0] == '#')
		{
			want_area_who = 1;
			victim++; /* Go past the # */
			wanted_location = atoi(victim); 
/* Set up the location for area who */
		}
	}
	//Find size of current descriptor list
	//Make new array
	//Copy in
	//Sort

	for (d = descriptor_list; d; d = d->next)
	{
		if(d->IS_FAKED() && (want_npcs==0))
			continue;
#ifdef ALIASES
		if ((victim == NULL) || ((want_npcs==1) && (d->IS_FAKED())) ||
		    (!want_me && !want_wizards && !want_apprentices &&
			  ((strncasecmp(victim, db[d->get_player()].get_name().c_str(), strlen(victim))==0) ||
				(db[d->get_player()].has_alias(victim)))) ||
		    (want_wizards && Connected(d->get_player()) && Wizard(d->get_player())) ||
		    (want_apprentices && Connected(d->get_player()) && Apprentice(d->get_player())) ||
		    (want_me && (strcasecmp(db[get_player()].get_name().c_str(), db[d->get_player()].get_name().c_str()) == 0)) ||
			(want_guests && is_guest(d->get_player())) ||
			(want_builders && Builder(d->get_player())) ||
			(want_xbuilders && XBuilder(d->get_player())) ||
			(want_fighters && Fighting(d->get_player())) ||
			(want_welcomers && Welcomer(d->get_player())) ||
			(want_area_who && in_area(d->get_player(), wanted_location)))
#else
		if ((victim == NULL) || ((want_npcs==1) && (d->IS_FAKED())) ||
		    (!want_me && !want_wizards && !want_apprentices &&
			(strncasecmp(victim, db[d->get_player()].get_name().c_str(), strlen(victim))==0)) ||
		    (want_wizards && Connected(d->get_player()) && Wizard(d->get_player())) ||
		    (want_apprentices && Connected(d->get_player()) && Apprentice(d->get_player())) ||
		    (want_me && (strcasecmp(db[get_player()].get_name().c_str(), db[d->get_player()].get_name().c_str()) == 0)) ||
			(want_guests && is_guest(d->get_player())) ||
			(want_builders && Builder(d->get_player())) ||
			(want_xbuilders && XBuilder(d->get_player())) ||
			(want_fighters && Fighting(d->get_player())) ||
			(want_welcomers && Welcomer(d->get_player())) ||
			(want_area_who && in_area(d->get_player(), wanted_location)))
#endif
		{
			if (!gotonealready)
			{
				if (get_player())
					queue_string (player_colour (get_player(), get_player(), COLOUR_MESSAGES));
				if (victim == NULL)
					queue_string ("Current Players:                                                          Idle\n");
				else if (want_npcs==1)
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
				if (want_npcs==1)
				{
					snprintf(scratch, sizeof(scratch), "#%-5d ", d->get_player());
					remaining-=7;
					strcat(buf, scratch);
				}

				if (get_player())
				{
					int thing= d->get_player();
					int colour=COLOUR_MORTALS;
					if (Builder(thing))
						colour= COLOUR_BUILDERS;
					if (Welcomer(thing))
						colour= COLOUR_WELCOMERS;
                                        if (XBuilder(thing))
						colour= COLOUR_XBUILDERS;
                                        if (Apprentice(thing))
						colour= COLOUR_APPRENTICES;
                                        if (Wizard(thing))
						colour= COLOUR_WIZARDS;
                                        if (thing==GOD_ID)
						colour= COLOUR_GOD;
					if (want_npcs==0)
						strcat(buf, player_colour (get_player(), get_player(), colour));
				}
				if (want_npcs)
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
				if (d->get_player() == GOD_ID)
				{
					strcat (flag_buf, "G");
					wizard++;
				}
				else if (Wizard(d->get_player()))
				{
					strcat (flag_buf, "W");
					wizard++;
				}
				else if (Apprentice(d->get_player()))
				{
					strcat (flag_buf, "A");
				}
				else if (XBuilder(d->get_player()))
				{
					strcat (flag_buf, "X");
				}
				else if (Builder(d->get_player()))
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

				if (want_npcs)
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

				if (want_npcs==0)
				{
					/* ... and machine name */
					if (d->address.sin_addr.s_addr == LOGTHROUGH_HOST)
						logthrough++;
					else if ((d->address.sin_addr.s_addr & 0xffff0000) == LOCAL_ADDRESS_MASK || d->address.sin_addr.s_addr == INADDR_LOOPBACK)
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
				if (d->indirect_connection)
					snprintf (flag_buf, sizeof(flag_buf), " <%s>", d->hostname);
				else
					snprintf (flag_buf, sizeof(flag_buf), " [%s]", d->hostname);
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
	else
	{
		if (!(flags & DUMP_QUIET))
			queue_string("No match found.\n");
	}

}

void
context::do_at_connect(
const	char	*what,
const	char	*)
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
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
        notify(player, "Connected");
        return_status = COMMAND_SUCC;
        set_return_string (ok_return_string);
}

void
context::do_at_disconnect(
const   char    *what,
const   char    *)

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
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
        notify(player, "Disconnected");

        return_status = COMMAND_SUCC;
        set_return_string (ok_return_string);
}
void
context::do_at_queue (
const   char    *what,
const   char    *command)

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
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
                return;
        }

	if(!command || !*command)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Queue what?");
		return;
	}

        for (d = descriptor_list; d; d = d->next)
                if (d->IS_CONNECTED() && (d->get_player() == victim))
                {
                        d->save_command(command);
                        break;
                }
        return_status = COMMAND_SUCC;
        set_return_string (ok_return_string);
}

void
context::do_who (
const	char	*name,
const	char	*)

{
	int matched_location;
struct	descriptor_data	*d;
	const	char		*victim;

	if (blank (name))
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
		victim = name;
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
descriptor_data::dump_swho()
{
	static	char		linebuf [4096];
	struct	descriptor_data	*d;
	int			num = 0;
	int			numperline = terminal_width / 25;

	if(numperline == 0) numperline = 3;

	for (d = descriptor_list; d; d = d->next)
	{
		if(d->IS_CONNECTED() && (Typeof(d->get_player()) != TYPE_PUPPET))
		{
			int thing= d->get_player();
			int colour=COLOUR_MORTALS;
			if (Builder(thing))
				colour= COLOUR_BUILDERS;
			if (Welcomer(thing))
				colour= COLOUR_WELCOMERS;
			if (XBuilder(thing))
				colour= COLOUR_XBUILDERS;
			if (Apprentice(thing))
				colour= COLOUR_APPRENTICES;
			if (Wizard(thing))
				colour= COLOUR_WIZARDS;
			if (thing==GOD_ID)
				colour= COLOUR_GOD;
			snprintf(linebuf, sizeof(linebuf), " %c%s%-23s%s", 
				(Wizard(d->get_player())?'*':(Apprentice(d->get_player())?'~':' ')),
				player_colour (get_player(), get_player(), colour),
				db[d->get_player()].get_name().c_str(),
				COLOUR_REVERT);
			queue_string(linebuf);
			if((++num % numperline) == 0)
				queue_string("\n");
		}
	}
	if(num % numperline)	/* Bit of a hack to get correct num of \n's */
		queue_string("\n");
	snprintf(linebuf, sizeof(linebuf), "%sUsers: %d%s\n",
		player_colour (get_player(), get_player(), COLOUR_MESSAGES),
		num, COLOUR_REVERT);
	queue_string(linebuf);
}


void
context::do_swho (
const	char	*,
const	char	*)

{
	struct descriptor_data *d;

	notify_colour(player, player, COLOUR_MESSAGES, "Current players:");
	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (d->get_player() == player))
			d->dump_swho ();
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
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


Command_status
descriptor_data::terminal_set_echo(const char *toggle, int)
{
	if(toggle && *toggle)
	{
		if(strcasecmp(toggle, "on")==0)
			t_echo = 1;
		else if(strcasecmp(toggle, "off")==0)
			t_echo = 0;
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  'set echo=on' or 'set echo=off'.");
	}

	notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Echo is %s.", t_echo? "on":"off");

	return COMMAND_SUCC;
}

Command_status
descriptor_data::terminal_set_pagebell(const char *toggle, int)
{
	if(toggle && *toggle)
	{
		if(strcasecmp(toggle, "on")==0)
			terminal_pagebell=1;
		else if(strcasecmp(toggle, "off")==0)
			terminal_pagebell=0;
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  'set pagebell=on' or 'set pagebell=off'.");
	}

	notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Pagebell is %s.", terminal_pagebell? "on":"off");

	return COMMAND_SUCC;
}




Command_status
descriptor_data::terminal_set_termtype (const char *termtype, int)
{
	if(termtype && *termtype)
	{
		if(strcasecmp(termtype, "none")==0)
		{
			if(terminal_type)
			{
				FREE(terminal_type);
				terminal_type=NULL;
			}
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Your terminal type is no longer set.");
		}
		else
		{
			if(set_terminal_type(termtype))
				notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal type is now '%s'.", termtype);
			else
				notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Unknown terminal '%s'.", termtype);
		}
	}
	else
	{
		if(terminal_type)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal type is '%s'.", terminal_type);
		else
			notify_colour(get_player(),get_player(), COLOUR_MESSAGES, "Your terminal type is not set.");
	}

	return COMMAND_SUCC;
}


Command_status
descriptor_data::terminal_set_width(const char *width, int commands_executed)
{
	int			i;

	if(width && *width)
	{
		i = atoi (width);
		if (i<0)
		{
			notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Can't have a negative word wrap width");
			return COMMAND_FAIL;
		}
		if (i)
			i = MAX(i,20); // Don't have an upper limit, cos someone might want it that wide.
		terminal_width = i;
	}
	if(!commands_executed)
	{
		if(terminal_width == 0)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal width is unset");
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Terminal width is %d", terminal_width);
	}

	return COMMAND_SUCC;
}

Command_status
descriptor_data::terminal_set_wrap(const char *width, int commands_executed)
{
	int			i;

	if(width && *width)
	{
		i = atoi (width);
		if (i<0)
		{
			notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Can't have a negative word wrap width");
			return COMMAND_FAIL;
		}
		if (i)
			i = MIN(MAX(i,20),256);
// If wrap is not zero, set terminal width to the value and set wrap on.
// If wrap is zero, leave terminal width, but set wrap off.
		if(i > 0)
		{
			terminal_width = i;
			terminal_wrap = 1;
		}
		else
		{
			terminal_wrap = 0;
		}
	}
	if(!commands_executed)
	{
		if(terminal_wrap == 0)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Word wrap is off");
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Word wrap width is %d", terminal_width);
	}

	return COMMAND_SUCC;
}


Command_status
descriptor_data::terminal_set_lftocr(const char *z, int commands_executed)
{
	if(z && *z)
	{
		if(!strcasecmp(z, "on"))
		{
			terminal_lftocr = 1;
		}
		else if(!strcasecmp(z, "off"))
		{
			terminal_lftocr = 0;
		}
		else
		{
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "Usage:  'set lftocr=on' or 'set lftocr=off'.");
			return COMMAND_FAIL;
		}
	}
	if(!commands_executed)
	{
		if(terminal_lftocr)
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES,"lftocr is on");
		else
			notify_colour(get_player(), get_player(), COLOUR_MESSAGES, "lftocr is off");
	}
	return COMMAND_SUCC;
}


void
context::do_at_motd (
const	char	*,
const	char	*)

{
	struct descriptor_data *d;

	for (d = descriptor_list; d; d = d->next)
		if (d->IS_CONNECTED() && (d->get_player() == player))
			d->splat_motd();
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_beep (
const	char	*,
const	char	*)

{
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	beep (player);
}


void
context::do_truncate(
const char *plist,
const char *string)
{
	int error_count=0,
	    ok_count=0;

	if (blank(plist) || blank(string))
	{
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @truncate <playerlist> = <underline string>");
		return;
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	char *mystring=safe_strdup(string); // Discarding the const

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
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only @underline to yourself or to people in rooms that you control.");
		else
			notify_colour(player, player, COLOUR_MESSAGES, "(Warning): %d player%s that you do not control so did not get underlined to.", error_count, (error_count==1) ? " is in a room" : "s are in rooms");
	}
}
void context::do_terminal_set(const char *command, const char *arg)
{
	struct descriptor_data *d;

	for(d=descriptor_list; d; d=d->next)
		if(d->get_player()==player)
			break;

	if(!d)
		return;

	return_status = d->really_do_terminal_set (command, arg, commands_executed);
	set_return_string ((return_status==COMMAND_SUCC) ? ok_return_string : error_return_string);
}


Command_status
descriptor_data::really_do_terminal_set(const char *command, const char *arg, int commands_executed)
{
	Command_status	return_status=COMMAND_SUCC;
	int		i;

	for(i=0; terminal_set_command_table[i].name; i++)
	{
		if(command && *command)
		{
			if(strcasecmp(command, terminal_set_command_table[i].name)==0)
			{
				return_status=(this->*(terminal_set_command_table[i].set_function))(arg, commands_executed);
				
				break;
			}
		}
		else
			(this->*(terminal_set_command_table[i].set_function))(NULL, commands_executed);
	}

	if(command && *command && !terminal_set_command_table[i].name)
	{
		notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Unknown terminal set command '%s'.", command);
		return_status=COMMAND_FAIL;
	}

	return return_status;
}


void
smd_updated()
{
struct descriptor_data *d;
u_long	host_addr;

	for(d=descriptor_list; d!=NULL; d=d->next)
	{
		host_addr = ntohl (d->address.sin_addr.s_addr);

		if (is_banned (host_addr))
		{
			d->queue_string(reject_updatedsmd);
			d->announce_player (ANNOUNCE_SMD);
			d->NUKE_DESCRIPTOR();
		}
		else
		{
			if((*(d->hostname) >= '0') && (*(d->hostname) <= '9'))
			{
				strncpy (d->hostname,
					convert_addr (&(d->address.sin_addr)),
					sizeof (d->hostname) - 1);
				d->hostname [sizeof (d->hostname) - 1] = '\0';
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
		Trace("CONCENTRATOR:  couldn't find channel %d in descriptor list!\n", channel);

	return d;
}


int process_concentrator_input(int sock)
{
	struct	conc_message		msg;
	struct	descriptor_data		*d;
	int				i;

	if ((i=read(sock, &msg, sizeof(msg)))<sizeof(msg))
	{
		Trace("CONCENTRATOR:  corrupt message received - wanted %d bytes, got %d\n", sizeof(msg), i);
		return 0;
	}

	switch(msg.type)
	{
		case CONC_CONNECT:

			/* This is the equivalent of new_connection() for concentrator connections */

			Trace( "ACCEPT from concentrator(%d) on channel %d\n", sock, msg.channel);
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
				Trace( "CONCENTRATOR:  got data on unknown channel %d (discarded)\n", msg.channel);
			break;

		default:
			Trace( "CONCENTRATOR:  unknown message type %d received (discarded)\n", msg.type);
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

	Trace( "CONCENTRATOR:  dropped connection (players booted)\n");
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

descriptor_data::descriptor_data()
{}

void
descriptor_data::set_output_prefix(const char *s)
{
	if(_output_prefix)
	{
		FREE(_output_prefix);
	}

	if(s && *s)
	{
		int l = strlen(s) + 3;
		MALLOC(_output_prefix, char, l);
		strcpy(_output_prefix, s);
		strcat(_output_prefix, "\n");
	}
	else
	{
		_output_prefix = NULL;
	}
}


void
descriptor_data::set_output_suffix(const char *s)
{
	if(_output_suffix)
	{
		FREE(_output_suffix);
	}

	if(s && *s)
	{
		int l = strlen(s) + 3;
		MALLOC(_output_suffix, char, l);
		strcpy(_output_suffix, s);
		strcat(_output_suffix, "\n");
	}
	else
	{
		_output_suffix = NULL;
	}
}

void
descriptor_data::output_prefix()
{
	myoutput = 1;
	if(IS_FAKED())
		return;
	if(_output_prefix)
	{
		queue_string(_output_prefix);
	}
}

void
descriptor_data::output_suffix()
{
	myoutput = 0;
	if(IS_FAKED())
		return;
	if(_output_suffix)
	{
		queue_string(_output_suffix);
	}
}

void
descriptor_data::set_player_name(char *p)
{
	if(_player_name)
	{
		free(_player_name);
		_player_name = NULL;
	}

	if(p && *p)
	{
		_player_name = safe_strdup(p);
	}
}

void
descriptor_data::set_password(char *p)
{
	if(_password)
	{
		free(_password);
		_password = NULL;
	}

	if(p && *p)
	{
		_password = safe_strdup(p);
	}
}



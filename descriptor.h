#ifndef _DESCRIPTOR_H_
#define _DESCRIPTOR_H_

#include "copyright.h"
#include "db.h"
#include "context.h"

enum connect_states
{
	DESCRIPTOR_UNCONNECTED,			/* Just initialised */
	DESCRIPTOR_NAME,			/* Await player name */
	DESCRIPTOR_PASSWORD,			/* Await password */
	DESCRIPTOR_NEW_CHARACTER_PROMPT,	/* New Character? */
	DESCRIPTOR_CONFIRM_PASSWORD,		/* Re-enter password */
	DESCRIPTOR_CONNECTED,			/* Player is connected */
	DESCRIPTOR_LIMBO_RECONNECTED,		/* Player is has been reconnected on another descriptor */
	DESCRIPTOR_LIMBO,			/* Awaiting disconnection */
	DESCRIPTOR_FAKED,			/* Faked connection; NPC */
	DESCRIPTOR_HALFQUIT			/* Player has 'HALFQUIT' */
};

enum announce_states
{
	ANNOUNCE_CREATED,			/* player just been created */
	ANNOUNCE_CONNECTED,			/* player just connected */
	ANNOUNCE_BOOTED,			/* player just booted */
	ANNOUNCE_DISCONNECTED,			/* player just QUITed */
	ANNOUNCE_SMD,				/* player fallen foul of an SMD read */
	ANNOUNCE_TIMEOUT,			/* mortal been idle too long */
	ANNOUNCE_PURGE,				/* player booting his idle connections */
	ANNOUNCE_RECONNECT,			/* player reconnects */
};


struct sockaddr_in;

struct text_block
{
	int		nchars;
	text_block	*nxt;
	char		*start;
	char		*buf;

	text_block() : nchars(0), nxt(NULL), start(NULL), buf(NULL) {}
private:
	text_block(const text_block&); // DUMMY
	text_block& operator=(const text_block&); // DUMMY
};

struct text_queue
{
	text_block	*head;
	text_block	**tail;

	text_queue() : head(NULL), tail(&head) {}
private:
	text_queue(const text_queue&); // DUMMY
	text_queue& operator=(const text_queue&); // DUMMY
};

class descriptor_data
{
public:
	descriptor_data(int sock, sockaddr_in *a, int conc = 0);
	~descriptor_data();
	static bool		check_descriptors;
private:

	int			_descriptor;
	dbref			_player;	/* Initilized to zero; hence (d->get_player() == 0) implies never connected */
	String			_player_name;	/* Used at connect time - not necessarily correct outside check_connect */
	String			_password;	/* Used at connect time - not necessarily correct outside check_connect */
	enum connect_states	_connect_state;
	String			_output_prefix;
	String			_output_suffix;

// Remove this when ready:
public:
	int			connect_attempts;
	int			output_size;
	struct	text_queue	output;
	struct	text_queue	input;
	unsigned char		*raw_input;
	unsigned char		*raw_input_at;
	time_t			start_time;
	time_t			last_time;
        time_t                  time_since_idle;
	int			warning_level;
	int			quota;
	bool			backslash_pending;
	int			cr_pending;
	int			indirect_connection;	/* got hostname through a SNDLOC */
	String			hostname;
	unsigned long		address;
	String			service; // What the port connected to is for
	descriptor_data		*next;
	descriptor_data		**prev; // Pointer to previous->next!
	struct Termcap	{
				String bold_on;
				String bold_off;
				String underscore_on;
				String underscore_off;
				String backspace;
				String clearline;
				Termcap() : bold_on(), bold_off(), underscore_on(), underscore_off(), backspace("\010 \010"), clearline("\r\n") {}
			}	termcap;
	struct	Terminal{
				int	width;
				int	height;
				String	type;
				int	xpos;
				bool	wrap;
				bool	lftocr;
				bool	pagebell;
				bool	recall;
				bool	effects;
				bool	halfquit;
				bool	noflush;
				Terminal() : width(0), height(0), type(), xpos(0), wrap(true), lftocr(true), pagebell(true), recall(true), effects(false), halfquit(false), noflush(false) {}
			}	terminal;
	int			channel;

	bool			myoutput;

	bool			t_echo;
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

	bool			_got_an_iac; // Set to non-zero if we've ever received an iac
// Functions:
public:
	bool	IS_HALFQUIT()
	{
		return (_connect_state == DESCRIPTOR_HALFQUIT);
	}
	bool	IS_CONNECTED()
	{
		return ((_connect_state == DESCRIPTOR_CONNECTED)
			|| (_connect_state == DESCRIPTOR_FAKED)
			|| (_connect_state == DESCRIPTOR_HALFQUIT));
	}
	bool	IS_FAKED()
	{
		return (_connect_state == DESCRIPTOR_FAKED);
	}
	bool	CHANNEL()
	{
		return ((get_descriptor()==0)? -channel:get_descriptor());
	}
	void	NUKE_DESCRIPTOR()
	{
		if(_connect_state != DESCRIPTOR_LIMBO_RECONNECTED)
		{
			_connect_state = DESCRIPTOR_LIMBO;
		}
		check_descriptors = true;
	}
	void	HALFQUIT()
	{
		NUKE_DESCRIPTOR();
		_connect_state = DESCRIPTOR_HALFQUIT;
	}
	void	output_prefix();
	void	output_suffix();
	void	set_output_prefix(const String& s) { _output_prefix = s; }
	void	set_output_suffix(const String& s) { _output_suffix = s; }

	enum connect_states
		get_connect_state()	{ return _connect_state; }
	void	set_connect_state(enum connect_states cs)
					{ _connect_state = cs; }
	void	connect_a_player	(dbref player, announce_states connect_state);
	int	get_descriptor()	{ return _descriptor; }
	int	get_player()		{ return _player; }
	const String& 	get_player_name()	{ return _player_name;}
	const String&	get_password()		{ return _password;}
	void	set_player(int p)	{ _player = p; }
	void	set_player_name(const String& p) { _player_name = p; }
	void	set_password(const String& p) { _password = p; }

	int	queue_string(const char *, int show_literally = 0, int store_in_recall_buffer = 1);
	int	queue_string(const String& s, int show_literally = 0, int store_in_recall_buffer = 1)
	{
		return queue_string(s.c_str(), show_literally, store_in_recall_buffer);
	}
	int	queue_write(const char *, int len);

	void	send_telnet_option(unsigned char command, unsigned char option);
	void	initial_telnet_options();
	int	process_telnet_options(unsigned char *, int);
	int	__process_telnet_options(unsigned char *&, unsigned char*&, unsigned char*);
	void	get_value_from_subnegotiation(unsigned char *, unsigned char, int);

	void	set_echo(bool echo);
	int	set_terminal_type(const String& terminal);
	Command_status	terminal_set_termtype(const String& , bool);
	String	terminal_get_termtype();
	Command_status	terminal_set_lftocr(const String& , bool);
	String	terminal_get_lftocr();
	Command_status	terminal_set_pagebell(const String& , bool);
	String	terminal_get_pagebell();
	Command_status	terminal_set_width(const String& , bool);
	String	terminal_get_width();
	Command_status	terminal_set_height(const String& , bool);
	String	terminal_get_height();
	Command_status	terminal_set_wrap(const String& , bool);
	String	terminal_get_wrap();
	Command_status	terminal_set_echo(const String& , bool);
	String	terminal_get_echo();
	Command_status	terminal_set_recall(const String& , bool);
	String	terminal_get_recall();
	Command_status	terminal_set_effects(const String& , bool);
	String	terminal_get_effects();
	Command_status	terminal_set_halfquit(const String& , bool);
	String	terminal_get_halfquit();
	Command_status	terminal_set_noflush(const String& , bool);
	String	terminal_get_noflush();
	void	do_write(const char * c, int i);

	int	process_output();
	int	process_input(int len = 0);
	void	shutdownsock();
	void	freeqs();

	void	welcome_user();
	void	splat_motd();
	int	check_connect(const char *msg);
	void	announce_player(announce_states state);

	void save_command (const char *command);
	int	do_command(const char *c);

	void	dump_users(const char *victim, int flags);
private:
	// Declared but not implemented.
	descriptor_data();
	descriptor_data(const descriptor_data&);
	descriptor_data& operator=(const descriptor_data&);
};

#endif

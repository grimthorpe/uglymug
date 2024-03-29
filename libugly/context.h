#ifndef	_CONTEXT_H
#define	_CONTEXT_H

#include <exception>
#include <stack>

#ifndef	_MATCH_H
#include "match.h"
#endif	/* !_MATCH_H */

#ifndef	_COMMAND_H
#include "command.h"
#endif	/* !_COMMAND_H */

#pragma interface

#define	RETURN_SUCC	{ return_status=COMMAND_SUCC; set_return_string (ok_return_string); return; }

#define	RETURN_FAIL	{ return_status=COMMAND_FAIL; set_return_string (error_return_string); return; }


enum	Command_status
{
	COMMAND_FAIL	= 0,
	COMMAND_SUCC	= 1,
	COMMAND_INIT	= 2,
	COMMAND_EXITHOME= 3,
	COMMAND_HALT	= 4
};

enum	Command_action
{
	ACTION_UNSET,
	ACTION_STOP,
	ACTION_CONTINUE,
	ACTION_RESTART,
	ACTION_HALT,
};


class	String_pair
{
    private:
		String			m_name;
		String			m_value;
    public:
					String_pair	(const String& n, const String& v);
					~String_pair	();
		void			set_value	(const String& v);
	const	String&			get_name	() 	const	{ return m_name; }
	const	String&			name	() 	const	{ return m_name; }
	const	String&			get_value	()	const	{ return m_value; }
	const	String&			value	()	const	{ return m_value; }
};


class context;

class	Variable_stack
{
    private:
		Variable_stack(const Variable_stack&); // DUMMY
		Variable_stack& operator=(const Variable_stack&); // DUMMY

		std::list <String_pair *> m_string_pair_stack;
    public:
				Variable_stack		()	{}
	virtual			~Variable_stack		();
		String_pair	*addarg			(const String& n, const String& v);
		String_pair	*check_and_add_arg	(const String& n, const String& v);
		bool		updatearg		(const String& n, const String& v);
		String_pair	*locatearg		(const String& name)	const;

};

/**
 * Scope holds information to do with lexical scopes inside compound commands.
 *	Any kind of loop, or the inside of an if...elseif...endif, starts a
 *	new lexical scope.  A new compound command execution is also a new
 *	scope; chasing a csucc/fail in a command chain is not, however.
 *
 * Variables defined inside a lexical scope are local to that scope.
 *
 * A Scope also knows what it is and what ends it; thus, a scope handles most
 *	of its own parsing and decision-making.  This eliminates the flow
 *	status that used to be held in the context, as the scopes themselves
 *	now hold flow information.
 *
 * PJC, 26/12/96.
 */

class	Scope
: public Variable_stack
{
private:
	Scope& operator=(const Scope&); // DUMMY
	Scope (const Scope &os);

    private:

		const	Scope		*outer_scope;
    public:
			int		current_line;
					Scope			(const Scope *os);
	virtual				~Scope () {}
	virtual	const	int		line_for_outer_scope	()	const	= 0;
	virtual	const	int		line_for_inner_scope	()	const;
			void		set_current_line	(int cl)	{ current_line = cl; }
		const	Scope		&get_outer_scope	()	const	{ return *outer_scope; }
	virtual	const	dbref		get_command		()	const	{ return outer_scope->get_command (); }
    public:
	virtual		Command_action	step_once		(context *)		= 0;
	virtual		void		do_at_elseif		(const bool ok);
	virtual		bool		do_at_break		();
	virtual		bool		do_at_continue		();
			String_pair	*locate_stack_arg	(const String& name)	const;
};

typedef	std::stack <Scope *>		Scope_stack;


#ifdef	HELL_HAS_FROZEN_OVER
/*
 * A Brace_scope holds information about nested commands.  Strictly it isn't
 *	a full scope, but the scope stack is a convenient place to stick it so
 *	that we have some permanent notion that a braced command is being
 *	executed.  PJC 8/2/97.
 *
 * Brace_scopes do not take part in the pre-parsing process.
 */

class	Brace_scope
: public Scope
{
    private:
		const	char		*cached_return_string;
			Command_status	cached_return_status;
		const	char		*source;
			char		smashed_base [MAX_COMMAND_LEN];
			char		*smashed_current;
    public:
					Brace_scope		(const Scope *os, const context &c);
	virtual				~Brace_scope		();
	virtual		Command_action	step_once		(context *);
};
#endif	/* HELL_HAS_FROZEN_OVER */


class	If_scope
: public Scope
{
			bool		if_ok;
			int		stop_line;
			int		endif_line;
    protected:
	virtual	const	int		line_for_outer_scope	()	const;
    public:
					If_scope	(const Scope *os, bool i);
	virtual				~If_scope	()			{}
			Command_action	step_once	(context *);
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
	virtual		void		do_at_elseif	(const bool ok);
};


/*
 * A Loop is a Scope that loops back to a start point under defined conditions.
 */

class	Loop
: public Scope
{
			int		start_line;
			int		end_line;
    protected:
					Loop		(const Scope *os);
	virtual	const	int		line_for_outer_scope	()	const;
	virtual		bool		loopagain	()			= 0;
	virtual		bool		shouldrun	()	const		= 0;
	virtual		bool		do_at_break	();
	virtual		bool		do_at_continue	();
    public:
	virtual				~Loop		()			{}
			Command_action	step_once	(context *);
};


class	For_loop
: public Loop
{
private:
	For_loop(const For_loop&); // DUMMY
	For_loop& operator=(const For_loop&); // DUMMY
    private:
			int		start;
			int		end;
			int		step;
			String_pair	*argument;
    protected:
	virtual		bool		shouldrun	()	const;
	virtual		bool		loopagain	();
	virtual		bool		do_at_break	();
    public:
					For_loop	(const Scope *os, int in_start, int in_end, int in_step, const String& name);
	virtual				~For_loop	()			{}
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
};


class	With_loop
: public Loop
{
    private:
	With_loop(const With_loop&); // DUMMY
	With_loop& operator=(const With_loop&); // DUMMY

	std::list<String_pair> elements;
	std::list<String_pair>::const_iterator pos;
			String_pair	*index;
			String_pair	*element;
    protected:
	virtual		bool		shouldrun	()	const;
	virtual		bool		loopagain	();
	virtual		bool		do_at_break	();
    public:
					With_loop	(const Scope *os, dbref d, const char *index_name, const char *element_name);
	virtual				~With_loop	()			{}
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
};


class	Command_and_arguments
{
    private:
	Command_and_arguments(const Command_and_arguments&);
	Command_and_arguments& operator=(const Command_and_arguments&);

			Matcher		*matcher;
			Pending_fuse	*sticky_fuses;
			String		simple_command;
			String		arg1;
			String		arg2;
			void		set_matcher(const Matcher *);
    protected:
			void		set_simple_command	(const String&);
			void		set_arg1		(const String&);
			void		set_arg2		(const String&);
    public:
					Command_and_arguments	(const String&sc, const String&a1, const String&a2, Matcher *m);
	virtual				~Command_and_arguments	();
			void		pend_fuse		(dbref fuse, bool success, const String&simple_command, const String&arg1, const String&arg2, const Matcher &matcher);
			void		fire_sticky_fuses	(context &c);
		const	String&		get_simple_command	()	const		{ return (simple_command); }
		const	String&		get_arg1		()	const		{ return (arg1); }
		const	String&		get_arg2		()	const		{ return (arg2); }
			Matcher		*get_matcher		()	const		{ return (matcher); }
};


class	Compound_command_and_arguments
: public Command_and_arguments
, public Scope
{
private:
	Compound_command_and_arguments(const Compound_command_and_arguments&); // DUMMY
	Compound_command_and_arguments& operator=(const Compound_command_and_arguments&); // DUMMY

    private:
			dbref		command;
			dbref		player;
			dbref		effective_id;
			dbref		unchpid_id;
			dbref		csucc_cache;
			dbref		cfail_cache;
			Scope_stack	scope_stack;
			bool		gagged; // Set true if this or previous compound commands 'Silent'
			bool		in_chpid; // Set true if this compound command has called @chpid
			void		empty_scope_stack	();
    protected:
	virtual	const	int		line_for_outer_scope	()	const	{ return current_line + 1; };
    public:
					Compound_command_and_arguments	(dbref c, context *, const String& sc, const String& a1, const String& a2, dbref eid, Matcher *m, bool silent);
	virtual				~Compound_command_and_arguments	();
			Command_action	step			(context *);
		const	char		*set_command		(dbref c);
		const	dbref		get_command		()	const		{ return (command); }
			void		set_effective_id	(dbref i)		{ effective_id = i; }
		const	dbref		get_effective_id	()	const		{ return (effective_id); }
			void		set_unchpid_id		(dbref i)		{ unchpid_id = i; }
		const	dbref		get_unchpid_id	()		const		{ return (unchpid_id); }
			void		chpid			()			{ effective_id = db [command].get_owner (); in_chpid = true; }
			void		unchpid			()			{ if(!in_chpid) effective_id = unchpid_id; }
		const	bool		inside_subscope		()	const		{ return !scope_stack.empty(); }
			void		push_scope		(Scope *s)		{ scope_stack.push (s); }
			Command_action	step_once	(context *);
			void		do_at_elseif		(const bool ok);
			void		do_at_end		();
			void		do_at_return		();
			void		do_at_returnchain	();
			bool		do_at_break		();
			bool		do_at_continue		();
		const	Scope		*innermost_scope	()	const;
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
		String_pair		*locate_innermost_arg	(const String& name)	const;

			bool		gagged_command		()	const	{ return gagged; }
};

class	Lua_command_and_arguments :
		public Compound_command_and_arguments
{
private:
	Lua_command_and_arguments(const Lua_command_and_arguments&); // DUMMY
	Lua_command_and_arguments& operator=(const Lua_command_and_arguments&); // DUMMY

    public:
					Lua_command_and_arguments	(dbref c, context *co, const String& sc, const String& a1, const String& a2, dbref eid, Matcher *m, bool silent);
	virtual				~Lua_command_and_arguments	();
			Command_action	step			(context *co);
	virtual		Command_action	step_once		(context *);
};

typedef	std::stack <Compound_command_and_arguments *>	Call_stack;


/* For later */
class	Dependency;
class	Scheduler;


class	context
: public Command_and_arguments
, public Variable_stack
{
	// No default constructor implemented.
	explicit context (const bool is_default);	///< Only here to allow the default context to be constructed.
	context(const context&);	///< No copy constructor implemented.
	context& operator=(const context&);	///< No operator= implemented.
public:
	static const context	DEFAULT_CONTEXT;
	static const context	UNPARSE_CONTEXT;
    friend struct Scheduler;
    friend struct Dependency;
    private:

		dbref			player;
		dbref			trace_command;
		Call_stack		call_stack;
		size_t			commands_executed;
		size_t			sneaky_executed_depth;
		size_t			step_limit;
		size_t			depth_limit;
		String			return_string;
		Command_status		return_status;
		bool			called_from_command;
		const context&		creator;
		bool			m_scheduled;	///< true (false) if this job is (not) scheduled.
		Dependency		*dependency;
    // friend Scheduler
		void			clear_dependency	();
    // friend Scheduler
	const	Dependency		*get_dependency		()	const	{ return dependency; }
    // friend Scheduler
	const	Command_action		step			();
    // friend Dependency
		void			execute			(void (context::*f) ())	{ (this->*f) (); }
    public:
					context			(dbref pl, const context& your_maker);
	virtual				~context		();
		void			execute_commands	();
		void			command_executed	()	{ commands_executed++; }
		void			set_step_limit		(size_t new_limit)	{ step_limit = new_limit; }
		void			set_depth_limit		(size_t new_limit)	{ depth_limit = new_limit; }
	const	bool			set_effective_id	(dbref i);
	const	bool			set_unchpid_id		(dbref i);
		void			scheduled		(bool s)	{ m_scheduled = s; }	///< Set whether this context is currently scheduled.
	const	bool			scheduled		()	const	{ return m_scheduled; }	///< Return whether this context is currently scheduled.
		void			set_return_string	(const String& rs) { return_string = rs; }
		void			calling_from_command	()		{ called_from_command = true; }
	const	size_t			get_commands_executed	()	const	{ return (commands_executed); }
	const	size_t			get_sneaky_executed_depth()	const	{ return (sneaky_executed_depth); }
		void			set_sneaky_executed_depth(size_t d)	{ sneaky_executed_depth = d; }
	const	size_t			get_depth_limit		()	const	{ return (depth_limit); }
	const	dbref			get_player		()	const	{ return (player); }
	const	dbref			get_unchpid_id		()	const;
	const	Command_status		get_return_status	()	const	{ return (return_status); }
	const	String&			get_innermost_arg1	()	const;
	const	String&			get_innermost_arg2	()	const;
	const	bool			really_in_command	()	const;
	const	bool			in_command		()	const;
	const	dbref			get_current_command	()	const;
	const	bool			gagged_command		()	const	{ return (call_stack.empty())?false:call_stack.top()->gagged_command(); }
	const	String&			get_return_string	()	const	{ return (return_string); }
	const	dbref			get_effective_id	()	const;
		void			copy_returns_from	(const context &source);
	const	bool			controls_for_read	(const dbref what) const;
	const	bool			controls_for_write	(const dbref what) const;
	const	bool			controls_for_private	(const dbref what) const;
	const	bool			controls_for_look	(const dbref what) const;
		dbref			find_object		(const String& name, bool need_control);
		String_pair		*locate_innermost_arg	(const String& name)	const;
#ifndef	NO_GAME_CODE
	bool				allow_another_step	();
	void				process_basic_command	(const String&);
	String				sneakily_process_basic_command	(const String&, Command_status &);
	bool				can_do_compound_command	(const String& command, const String& arg1, const String& arg2);
	bool				can_override_command (const String& command, const String& arg1, const String& arg2);
	Command_action			prepare_compound_command	(dbref command, const String& simple_command, const String& arg1, const String& arg2, dbref effective_id = NOTHING, Matcher &matcher = *(Matcher *) NULL);
	Command_action			do_compound_command	(dbref command, const String& simple_command, const String& arg1, const String& arg2, dbref effective_id = NOTHING, Matcher &matcher = *(Matcher *) NULL);
	Command_action			do_lua_command	(dbref command, const String& simple_command, const String& arg1, const String& arg2);
	/* Functions from move.c that needed to be inside a context */
	void				enter_room              (dbref loc);
	void				send_home		(dbref thing);
	void				maybe_dropto		(dbref loc, dbref dropto);
	void				send_contents		(dbref loc, dbref dropto);

	const	bool			variable_substitution	(const char *arg, char *result, size_t max_length);
	const	bool			nested_variable_substitution	(const char *&, char *, const int, size_t space_left);
	const	bool			dollar_substitute	(const char *&argp, char *&resp, const int depth, size_t space_left);
		void			brace_substitute	(const char *&, char *&, size_t space_left);

	void				do_at_alarm		(const String&, const String&);
	void				do_at_array		(const String&, const String&);
	void				do_at_censor		(const String&, const String&);
//	void				do_at_clone		(const String&, const String&);
//	dbref				really_do_at_clone	(const dbref original, const dbref location, bool copyDescription = true);
	void				do_at_censorinfo	(const String&, const String&);
	void				do_at_command		(const String&, const String&);
	void				do_at_connect		(const String&, const String&);
	void				do_at_disconnect	(const String&, const String&);
	void				do_at_drop		(const String&, const String&);
	void				do_at_else		(const String&, const String&);
	void				do_at_elseif		(const String&, const String&);
	void				do_at_endif		(const String&, const String&);
	void				do_at_end		(const String&, const String&);
	void				do_at_exclude		(const String&, const String&);
	void				do_at_false		(const String&, const String&);
	void				do_at_for		(const String&, const String&);
	void				do_at_global		(const String&, const String&);
	void				do_at_if		(const String&, const String&);
	void				do_at_insert		(const String&, const String&);
	void				do_at_key		(const String&, const String&);
	void				do_at_codelanguage	(const String&, const String&);
	void				do_at_list		(const String&, const String&);
	void				do_at_listcolours	(const String&, const String&);
	void				do_at_local		(const String&, const String&);
	void				do_at_lock		(const String&, const String&);
	void				do_at_motd		(const String&, const String&);
	void				do_at_note		(const String&, const String&);
	void				do_at_notify		(const String&, const String&);
	void				do_at_oecho		(const String&, const String&);
	void				do_at_oemote		(const String&, const String&);
	void				do_at_open		(const String&, const String&);
	void				do_at_queue		(const String&, const String&);
	void				do_at_recursionlimit	(const String&, const String&);
	void				do_at_score		(const String&, const String&);
	void				do_at_sort		(const String&, const String&);
	void				do_at_true		(const String&, const String&);
	void				do_at_uncensor		(const String&, const String&);
	void				do_at_unexclude		(const String&, const String&);
	void				do_at_unlock		(const String&, const String&);
	void				do_at_who		(const String&, const String&);
	void				do_at_with		(const String&, const String&);
	void				do_at_beep		(const String&, const String&);
	void				do_at_boot		(const String&, const String&);
	void				do_at_break		(const String&, const String&);
	void				do_at_cfailure		(const String&, const String&);
	void				do_at_channel		(const String&, const String&);
	void				do_channel_who		(const String&, const String&);
	void				do_chat			(const String&, const String&);
	void				do_at_chpid		(const String&, const String&);
	void				do_close		(const String&, const String&);
	void				do_at_colour		(const String&, const String&);
	void				do_container		(const String&, const String&);
	void				do_at_controller	(const String&, const String&);
	void				do_at_continue		(const String&, const String&);
	void				do_at_create		(const String&, const String&);
	void				do_at_credit		(const String&, const String&);
	void				do_at_cstring		(const String&, const String&);
	void				do_at_csuccess		(const String&, const String&);
	void				do_at_debit		(const String&, const String&);
	void				do_at_decompile		(const String&, const String&);
	void				do_at_describe		(const String&, const String&);
	void				do_at_destroy		(const String&, const String&);
	void				do_at_dictionary	(const String&, const String&);
	void				do_at_dig		(const String&, const String&);
	void				do_drop			(const String&, const String&);
	void				do_at_dump		(const String&, const String&);
	void				do_at_echo		(const String&, const String&);
	void				do_at_email		(const String&, const String&);
	void				dump_email_addresses		();
	void				do_at_empty		(const String&, const String&);
	void				do_enter		(const String&, const String&);
	void				do_at_event		(const String&, const String&);
	void				do_examine		(const String&, const String&);
	void				do_at_fail		(const String&, const String&);
	void				do_fchat		(const String&, const String&);
	void				do_at_find		(const String&, const String&);
	void				do_at_force		(const String&, const String&);
	void				do_at_from		(const String&, const String&);
	void				do_at_evaluate		(const String&, const String&);
	void				do_at_fuse		(const String&, const String&);
	void				do_fwho			(const String&, const String&);
	void				do_at_garbage_collect	(const String&, const String&);
	void				do_get			(const String&, const String&);
	void				do_give			(const String&, const String&);
	void				do_at_gravity_factor	(const String&, const String&);
	void				do_at_group		(const String&, const String&);
	void				do_gripe		(const String&, const String&);
#ifdef ALIASES
	void				do_at_unalias		(const String&, const String&);
	void				do_at_alias		(const String&, const String&);
	void				do_at_listaliases	(const String&, const String&);
#endif /* ALIASES */
	void				do_help		(const String&, const String&);
	void				do_inventory		(const String&, const String&);
	void				do_kill			(const String&, const String&);
	void				do_ladd			(const String&, const String&);
	void				do_leave		(const String&, const String&);
	void				do_lftocr		(const String&, const String&);
	void				do_at_link		(const String&, const String&);
	void				do_llist		(const String&, const String&);
	void				do_at_location		(const String&, const String&);
	void				do_lock			(const String&, const String&);
	void				do_look_at		(const String&, const String&);
	void				do_lremove		(const String&, const String&);
	void				do_lset			(const String&, const String&);
	void				do_at_parent		(const String&, const String&);
	void				do_at_peak		(const String&, const String&);
	void				do_at_mass		(const String&, const String&);
	void				do_at_mass_limit	(const String&, const String&);
	void				do_move			(const String&, const String&);
	void				do_at_name		(const String&, const String&);
	void				do_at_natter		(const String&, const String&);
	void				do_at_newpassword	(const String&, const String&);
	void				do_at_areanotify	(const String&, const String&);
	void				do_notify		(const String&, const String&);
	void				do_at_odrop		(const String&, const String&);
	void				do_at_ofail		(const String&, const String&);
	void				do_open			(const String&, const String&);
	void				do_at_osuccess		(const String&, const String&);
	void				do_at_owner		(const String&, const String&);
	void				do_page			(const String&, const String&);
	void				do_at_password		(const String&, const String&);
	void				do_at_pcreate		(const String&, const String&);
	void				do_at_pemote		(const String&, const String&);
	void				do_pose			(const String&, const String&);
	void				do_emote		(const String&, const String&, bool oemote);
	void				do_at_property		(const String&, const String&);
	void				do_at_puppet		(const String&, const String&);
	void				do_query_address	(const String&, const String&);
	void				do_query_area		(const String&, const String&);
	void				do_query_article	(const String&, const String&);
	void				do_query_arrays		(const String&, const String&);
	void				do_query_atime		(const String&, const String&);
	void				do_query_bps		(const String&, const String&);
	void				do_query_can		(const String&, const String&);
	void				do_query_cfail		(const String&, const String&);
	void				do_query_channel	(const String&, const String&);
	void				do_query_colour		(const String&, const String&);
	void				do_query_commands	(const String&, const String&);
	void				do_query_connected	(const String&, const String&);
	void				do_query_connectedplayers (const String&, const String&);
	void				do_query_contents	(const String&, const String&);
	void				do_query_controller	(const String&, const String&);
	void				do_query_cstring	(const String&, const String&);
	void				do_query_csucc		(const String&, const String&);
	void				do_query_ctime		(const String&, const String&);
	void				do_query_descendantfrom	(const String&, const String&);
	void				do_query_description	(const String&, const String&);
	void				do_query_destination	(const String&, const String&);
	void				do_query_dictionaries	(const String&, const String&);
	void				do_query_drop		(const String&, const String&);
	void				do_query_elements	(const String&, const String&);
	void				do_query_email		(const String&, const String&);
	void				do_query_exist		(const String&, const String&);
	void				do_query_exits		(const String&, const String&);
	void				do_query_fail		(const String&, const String&);
	void				do_query_first_name	(const String&, const String&);
	void				do_query_fuses		(const String&, const String&);
	void				do_query_gravity_factor	(const String&, const String&);
	void				do_query_hardcodecommands (const String&, const String&);
	void				do_query_id		(const String&, const String&);
	void				do_query_idletime	(const String&, const String&);
	void				do_query_interval	(const String&, const String&);
	void				do_query_key		(const String&, const String&);
	void				do_query_last_entry	(const String&, const String&);
	void				do_query_location	(const String&, const String&);
	void				do_query_lock		(const String&, const String&);
	void				do_query_mass		(const String&, const String&);
	void				do_query_mass_limit	(const String&, const String&);
	void				do_query_money		(const String&, const String&);
	void				do_query_mtime		(const String&, const String&);
	void				do_query_my		(const String&, const String&);
	void				do_query_myself		(const String&, const String&);
	void				do_query_name		(const String&, const String&);
	void				do_query_next		(const String&, const String&);
	void				do_query_numconnected	(const String&, const String&);
	void				do_query_odrop		(const String&, const String&);
	void				do_query_ofail		(const String&, const String&);
	void				do_query_osuccess	(const String&, const String&);
	void				do_query_owner		(const String&, const String&);
	void				do_query_parent		(const String&, const String&);
	void				do_query_pending	(const String&, const String&);
	void				do_query_pid		(const String&, const String&);
	void				do_query_properties	(const String&, const String&);
	void				do_query_aliases	(const String&, const String&);
	void				do_query_race		(const String&, const String&);
	void				do_query_rand		(const String&, const String&);
	void				do_query_realtime	(const String&, const String&);
	void				do_query_score		(const String&, const String&);
	void				do_query_set		(const String&, const String&);
	void				do_query_size		(const String&, const String&);
	void				do_query_stepsleft	(const String&, const String&);
	void				do_query_success	(const String&, const String&);
	void				do_query_terminal	(const String&, const String&);
	void				do_query_time		(const String&, const String&);
	void				do_query_typeof		(const String&, const String&);
	void				do_query_variables	(const String&, const String&);
	void				do_query_version	(const String&, const String&);
	void				do_query_volume		(const String&, const String&);
	void				do_query_volume_limit	(const String&, const String&);
	void				do_query_weight		(const String&, const String&);
	void				do_query_who		(const String&, const String&);
	void				do_quit			(const String&, const String&);
	void				do_at_race		(const String&, const String&);
	void				do_at_recall		(const String&, const String&);
	void				do_at_rem		(const String&, const String&);
	void				do_remote		(const String&, const String&);
	void				do_at_return		(const String&, const String&);
	void				do_at_returnchain	(const String&, const String&);
	void				do_say			(const String&, const String&);
	void				do_score		(const String&, const String&);
	void				do_set			(const String&, const String&);
	void				do_at_shutdown		(const String&, const String&);
	void				do_at_smd		(const String&, const String&);
	void				do_at_smdread		(const String&, const String&);
	void				do_at_stats		(const String&, const String&);
	void				do_swho			(const String&, const String&);
	void				do_at_success		(const String&, const String&);
	void				do_tell			(const String&, const String&);
	void				do_tellemote		(const String&, const String&);
	void				do_at_terminal		(const String&, const String&);
	void				do_at_test		(const String&, const String&);
	void				do_at_truncate		(const String&, const String&);
	void				do_at_unchpid		(const String&, const String&);
	void				do_at_unlink		(const String&, const String&);
	void				do_unlock		(const String&, const String&);
	void				do_at_variable		(const String&, const String&);
	void				do_at_version		(const String&, const String&);
	void				do_at_volume		(const String&, const String&);
	void				do_at_volume_limit	(const String&, const String&);
	void				do_at_wall		(const String&, const String&);
	void				do_at_warn		(const String&, const String& = NULLSTRING);
	void				do_at_error		(const String&, const String& = NULLSTRING);
	void				do_at_welcome		(const String&, const String&);
	void				do_whisper		(const String&, const String&);
	void				do_who			(const String&, const String&);
#endif	/* !NO_GAME_CODE */
};


class Dependency
{
    private:
	Dependency(const Dependency&); // DUMMY
	Dependency& operator=(const Dependency&); // DUMMY

			context		*blocker;
			context		*blocked;
			void		(context::*proceed_function) ();
    public:
					Dependency	(context *br, context *bd, void (context::*p) ())	: blocker (br), blocked (bd), proceed_function (p)	{}
					~Dependency	()		{ blocked->execute (proceed_function); }
		const	context		*get_blocker	()	const	{ return blocker; }
};


/**
 * Scheduler keeps track of who wants to do what to whom.  Each 'what' is a
 *	context, which is repeatedly single-stepped until it reports end-of-job.
 *
 * At present, the code is single-tasking.  The scheduler is therefore a LIFO
 *	stack of jobs.
 *
 * Blocking is implemented; if a context must wait until another command
 *	completes, it may set up a Dependency on that job.  Frequently, commands
 *	must do some part of themselves, kick off a command, then complete
 *	processing.  Dependencies implement this by allowing a function to be
 *	called when the dependency is cleared.  The Scheduler notifies the
 *	Dependency of its completion by calling its destructor.
 *
 * Deadlock detection is minimal; if no context may run, the system must be
 *	deadlocked.  Deadlock is handled by forcing the most recent context
 *	(top of the stack) to return control.
 */

class	Scheduler
{
    private:
			std::stack <context *>	contexts;
    public:
					Scheduler	(): contexts()	{}
					~Scheduler	()		{}
		bool			runnable	()	const	{ return !contexts.empty (); }
		context			*step		();
		void			push_job	(context *c)	{ c->scheduled (true); contexts.push (c); }
		context			*push_express_job	(context *c);
		context			*push_new_express_job	(context *c);
};


/**
 * The only one there is
 */

extern	Scheduler	mud_scheduler;

#endif	/* _CONTEXT_H */

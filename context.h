#ifndef	_CONTEXT_H
#define	_CONTEXT_H

#ifndef	_STACK_H
#include "stack.h"
#endif	/* !_STACK_H */

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
		String			name;
		String			value;
    public:
					String_pair	(const CString& n, const CString& v);
					~String_pair	();
		void			set_value	(const String& v);
	const	String&			get_name	() 	const	{ return name; }
	const	String&			get_value	()	const	{ return value; }
};

typedef	Link_stack <String_pair *>	String_pair_stack;
typedef	Link_iterator <String_pair *>	String_pair_iterator;


/*
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

class context;

class	Variable_stack
{
    private:
		String_pair_stack variable_stack;
    public:
				~Variable_stack		();
		String_pair	*addarg			(const CString& n, const CString& v);
		String_pair	*check_and_add_arg	(const CString& n, const CString& v);
		bool		updatearg		(const CString& n, const CString& v);
		String_pair	*locatearg		(const CString& name)	const;

};

class	Scope
: public Variable_stack
{
    private:

		const	Scope		*outer_scope;
    public:
			int		current_line;
					Scope			(const Scope &os);
	virtual				~Scope () {}
	virtual	const	int		line_for_outer_scope	()	const	= 0;
	virtual	const	int		line_for_inner_scope	()	const;
			void		set_current_line	(int cl)	{ current_line = cl; }
		const	Scope		&get_outer_scope	()	const	{ return *outer_scope; }
	virtual	const	dbref		get_command		()	const	{ return outer_scope->get_command (); }
    public:
	virtual		Command_action	step_once		(context *)		= 0;
	virtual		void		do_at_elseif		(const bool ok);
			String_pair	*locate_stack_arg	(const CString& name)	const;
};

typedef	Array_stack <Scope *>		Scope_stack;


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
					Brace_scope		(const Scope &os, const context &c);
	virtual				~Brace_scope		();
	virtual		Command_action	step_once		(context *);
};
#endif	/* HELL_HAS_FROZEN_OVER */


class	If_scope
: public Scope
{
			bool		if_ok;
			bool		no_endif;
			int		stop_line;
			int		endif_line;
    protected:
	virtual	const	int		line_for_outer_scope	()	const;
    public:
					If_scope	(const Scope &os, bool i);
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
					Loop		(const Scope &os);
	virtual	const	int		line_for_outer_scope	()	const;
	virtual		bool		loopagain	()			= 0;
    public:
	virtual				~Loop		()			{}
			Command_action	step_once	(context *);
};


class	For_loop
: public Loop
{
    private:
			int		start;
			int		end;
			int		step;
			String_pair	*argument;
    protected:
	virtual		bool		loopagain	();
    public:
					For_loop	(const Scope &os, int in_start, int in_end, int in_step, const CString& name);
	virtual				~For_loop	()			{}
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
};


class	With_loop
: public Loop
{
    private:
			int		counter;
			dbref		dict;
			String_pair	*index;
			String_pair	*element;
    protected:
	virtual		bool		loopagain	();
    public:
					With_loop	(const Scope &os, dbref d, const char *index_name, const char *element_name);
	virtual				~With_loop	()			{}
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
};


class	Command_and_arguments
{
    private:
			Matcher		*matcher;
			Pending_fuse	*sticky_fuses;
		const	char		*simple_command;
		const	char		*arg1;
		const	char		*arg2;
			void		set_matcher(const Matcher *);
    protected:
			void		set_simple_command	(const char *);
			void		set_arg1		(const char *);
			void		set_arg2		(const char *);
    public:
					Command_and_arguments	(const char *sc, const char *a1, const char *a2, Matcher *m);
					~Command_and_arguments	();
			void		pend_fuse		(dbref fuse, bool success, const char *simple_command, const char *arg1, const char *arg2, const Matcher &matcher);
			void		fire_sticky_fuses	(context &c);
		const	char		*get_simple_command	()	const		{ return (simple_command); }
		const	char		*get_arg1		()	const		{ return (arg1); }
		const	char		*get_arg2		()	const		{ return (arg2); }
			Matcher		*get_matcher		()	const		{ return (matcher); }
};


class	Compound_command_and_arguments
: public Command_and_arguments
, public Scope
{
    private:
			dbref		command;
			dbref		player;
			dbref		effective_id;
			dbref		csucc_cache;
			dbref		cfail_cache;
			Scope_stack	scope_stack;
    protected:
	virtual	const	int		line_for_outer_scope	()	const	{ return current_line + 1; };
    public:
					Compound_command_and_arguments	(dbref c, context *, const char *sc, const char *a1, const char *a2, dbref eid, Matcher *m);
	virtual				~Compound_command_and_arguments	();
			Command_action	step			(context *);
		const	char		*set_command		(dbref c);
		const	dbref		get_command		()	const		{ return (command); }
			void		set_effective_id	(dbref i)		{ effective_id = i; }
		const	dbref		get_effective_id	()	const		{ return (effective_id); }
			void		chpid			()			{ effective_id = db [command].get_owner (); }
		const	bool		inside_subscope		()	const		{ return !scope_stack.is_empty(); }
			bool		push_scope		(Scope *s)		{ return scope_stack.push (s); }
			Command_action	step_once	(context *);
			void		do_at_elseif		(const bool ok);
			void		do_at_end		(context &);
			void		do_at_return		(context &);
			void		do_at_returnchain	(context &);
		const	Scope		&innermost_scope	()	const;
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
		String_pair		*locate_innermost_arg	(const CString& name)	const;
};

typedef	Array_stack <Compound_command_and_arguments *>	Call_stack;


/* For later */
class	Dependency;
class	Scheduler;


class	context
: public Command_and_arguments
, public Variable_stack
{
    friend Scheduler;
    friend Dependency;
    private:
		dbref			player;
		dbref			trace_command;
		dbref			unchpid_id;
		Call_stack		call_stack;
		int			commands_executed;
		int			sneaky_executed_depth;
		int			step_limit;
		int			depth_limit;
		String			return_string;
		Command_status		return_status;
		bool			called_from_command;
		context &		creator;
		bool			scheduled;
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
					context			(dbref pl, context &your_creator = *(context *) NULL);
					~context		();
		void			execute_commands	();
		void			command_executed	()	{ commands_executed++; }
		void			set_step_limit		(int new_limit)	{ step_limit = new_limit; }
		void			set_depth_limit		(int new_limit)	{ depth_limit = new_limit; }
	const	bool			set_effective_id	(dbref i);
		void			set_unchpid_id		(dbref i)	{ unchpid_id = i; }
		void			set_scheduled		(bool s)	{ scheduled = s; }
	const	bool			get_scheduled		()	const	{ return scheduled; }
		void			set_return_string	(const CString& rs) { return_string = rs; }
		void			calling_from_command	()		{ called_from_command = true; }
	const	int			get_commands_executed	()	const	{ return (commands_executed); }
	const	int			get_sneaky_executed_depth()	const	{ return (sneaky_executed_depth); }
		void			set_sneaky_executed_depth(int d)	{ sneaky_executed_depth = d; }
	const	int			get_depth_limit		()	const	{ return (depth_limit); }
	const	dbref			get_player		()	const	{ return (player); }
	const	dbref			get_unchpid_id		()	const	{ return (unchpid_id); }
	const	Command_status		get_return_status	()	const	{ return (return_status); }
	const	char			*get_innermost_arg1	()	const;
	const	char			*get_innermost_arg2	()	const;
	const	bool			in_command		()	const;
	const	dbref			get_current_command	()	const;
	const	bool			gagged_command		()	const	{ return ((get_current_command () != NOTHING) && Silent (get_current_command ())); }
	const	String&			get_return_string	()	const	{ return (return_string); }
	const	dbref			get_effective_id	()	const;
		void			copy_returns_from	(const context &source);
	const	bool			controls_for_read	(const dbref what) const;
	const	bool			controls_for_write	(const dbref what) const;
		String_pair		*locate_innermost_arg	(const CString& name)	const;
#ifndef	NO_GAME_CODE
	bool				allow_another_step	();
	void				log_recursion		(dbref command, const char *arg1, const char *arg2);
	void				process_basic_command	(const char *);
	String				sneakily_process_basic_command	(const CString&, Command_status &);
	bool				can_do_compound_command	(const char *command, const char *arg1, const char *arg2);
	bool				can_override_command (const char *command, const char *arg1, const char *arg2);
	Command_action			do_compound_command	(dbref command, const char *simple_command, const char *arg1, const char *arg2, dbref effective_id = NOTHING, Matcher &matcher = *(Matcher *) NULL);

	/* Functions from move.c that needed to be inside a context */
	void				enter_room              (dbref loc);
	void				send_home		(dbref thing);
	void				maybe_dropto		(dbref loc, dbref dropto);
	void				send_contents		(dbref loc, dbref dropto);

	const	bool			variable_substitution	(const char *arg, char *result, unsigned int max_length);
	const	bool			nested_variable_substitution	(const char *&, char *, const int, const int space_left);
	const	bool			dollar_substitute	(const char *&argp, char *&resp, const int depth, unsigned int space_left);
		void			brace_substitute	(const char *&, char *&, unsigned int space_left);

	void				do_alarm		(const CString&, const CString&);
	void				do_ammo_type		(const CString&, const CString&);
	void				do_ammunition		(const CString&, const CString&);
	void				do_armour		(const CString&, const CString&);
	void				do_array		(const CString&, const CString&);
	void				do_at_censor		(const CString&, const CString&);
	void				do_at_censorinfo	(const CString&, const CString&);
	void				do_at_command		(const CString&, const CString&);
	void				do_at_connect		(const CString&, const CString&);
	void				do_at_disconnect	(const CString&, const CString&);
	void				do_at_drop		(const CString&, const CString&);
	void				do_at_else		(const CString&, const CString&);
	void				do_at_elseif		(const CString&, const CString&);
	void				do_at_endif		(const CString&, const CString&);
	void				do_at_end		(const CString&, const CString&);
	void				do_at_exclude		(const CString&, const CString&);
	void				do_at_false		(const CString&, const CString&);
	void				do_at_for		(const CString&, const CString&);
	void				do_at_global		(const CString&, const CString&);
	void				do_at_if		(const CString&, const CString&);
	void				do_at_insert		(const CString&, const CString&);
	void				do_at_key		(const CString&, const CString&);
	void				do_at_list		(const CString&, const CString&);
	void				do_at_listcolours	(const CString&, const CString&);
	void				do_at_local		(const CString&, const CString&);
	void				do_at_lock		(const CString&, const CString&);
	void				do_at_motd		(const CString&, const CString&);
	void				do_at_note		(const CString&, const CString&);
	void				do_at_notify		(const CString&, const CString&);
	void				do_at_oecho		(const CString&, const CString&);
	void				do_at_oemote		(const CString&, const CString&);
	void				do_at_open		(const CString&, const CString&);
	void				do_at_queue		(const CString&, const CString&);
	void				do_at_recursionlimit	(const CString&, const CString&);
	void				do_at_score		(const CString&, const CString&);
	void				do_at_sort		(const CString&, const CString&);
	void				do_at_true		(const CString&, const CString&);
	void				do_at_uncensor		(const CString&, const CString&);
	void				do_at_unexclude		(const CString&, const CString&);
	void				do_at_unlock		(const CString&, const CString&);
	void				do_at_who		(const CString&, const CString&);
	void				do_at_with		(const CString&, const CString&);
	void				do_attack		(const CString&, const CString&);
	void				do_attributes		(const CString&, const CString&);
	void				do_beep			(const CString&, const CString&);
	void				do_boot			(const CString&, const CString&);
	void				do_broadcast		(const CString&, const CString&);
	void				do_cfailure		(const CString&, const CString&);
	void				do_channel		(const CString&, const CString&);
	void				do_channel_who		(const CString&, const CString&);
	void				do_chat			(const CString&, const CString&);
	void				do_chpid		(const CString&, const CString&);
	void				do_close		(const CString&, const CString&);
	void				do_colour		(const CString&, const CString&);
	void				do_container		(const CString&, const CString&);
	void				do_controller		(const CString&, const CString&);
	void				do_create		(const CString&, const CString&);
	void				do_credit		(const CString&, const CString&);
	void				do_cstring		(const CString&, const CString&);
	void				do_csuccess		(const CString&, const CString&);
	void				do_debit		(const CString&, const CString&);
	void				do_decompile		(const CString&, const CString&);
	void				do_describe		(const CString&, const CString&);
	void				do_destroy		(const CString&, const CString&);
	void				do_dictionary		(const CString&, const CString&);
	void				do_dig			(const CString&, const CString&);
	void				do_drop			(const CString&, const CString&);
	void				do_dump			(const CString&, const CString&);
	void				do_echo			(const CString&, const CString&);
	void				do_email		(const CString&, const CString&);
	void				dump_email_addresses		();
	void				do_empty		(const CString&, const CString&);
	void				do_enter		(const CString&, const CString&);
	void				do_event		(const CString&, const CString&);
	void				do_examine		(const CString&, const CString&);
	void				do_fail			(const CString&, const CString&);
	void				do_fchat		(const CString&, const CString&);
	void				do_find			(const CString&, const CString&);
	void				do_force		(const CString&, const CString&);
	void				do_from			(const CString&, const CString&);
	void				do_full_evaluate	(const CString&, const CString&);
	void				do_fuse			(const CString&, const CString&);
	void				do_fwho			(const CString&, const CString&);
	void				do_garbage_collect	(const CString&, const CString&);
	void				do_get			(const CString&, const CString&);
	void				do_give			(const CString&, const CString&);
	void				do_gravity_factor	(const CString&, const CString&);
	void				do_group		(const CString&, const CString&);
	void				do_gripe		(const CString&, const CString&);
#ifdef ALIASES
	void				do_unalias		(const CString&, const CString&);
	void				do_alias		(const CString&, const CString&);
	void				do_listaliases		(const CString&, const CString&);
#endif /* ALIASES */
	void				do_inventory		(const CString&, const CString&);
	void				do_kill			(const CString&, const CString&);
	void				do_ladd			(const CString&, const CString&);
	void				do_leave		(const CString&, const CString&);
	void				do_load			(const CString&, const CString&);
	void				do_lftocr		(const CString&, const CString&);
	void				do_link			(const CString&, const CString&);
	void				do_llist		(const CString&, const CString&);
	void				do_location		(const CString&, const CString&);
	void				do_lock			(const CString&, const CString&);
	void				do_look_at		(const CString&, const CString&);
	void				do_lremove		(const CString&, const CString&);
	void				do_lset			(const CString&, const CString&);
	void				do_parent		(const CString&, const CString&);
	void				do_peak			(const CString&, const CString&);
	void				do_mass			(const CString&, const CString&);
	void				do_mass_limit		(const CString&, const CString&);
	void				do_modify		(const CString&, const CString&);
	void				do_move			(const CString&, const CString&);
	void				do_name			(const CString&, const CString&);
	void				do_natter		(const CString&, const CString&);
	void				do_newpassword		(const CString&, const CString&);
	void				do_at_areanotify	(const CString&, const CString&);
	void				do_notify		(const CString&, const CString&);
	void				do_odrop		(const CString&, const CString&);
	void				do_ofail		(const CString&, const CString&);
	void				do_open			(const CString&, const CString&);
	void				do_osuccess		(const CString&, const CString&);
	void				do_owner		(const CString&, const CString&);
	void				do_page			(const CString&, const CString&);
	void				do_password		(const CString&, const CString&);
	void				do_pcreate		(const CString&, const CString&);
	void				do_pemote		(const CString&, const CString&);
	void				do_pose			(const CString&, const CString&);
	void				do_property		(const CString&, const CString&);
	void				do_puppet		(const CString&, const CString&);
	void				do_query_address	(const CString&, const CString&);
	void				do_query_area		(const CString&, const CString&);
	void				do_query_article	(const CString&, const CString&);
	void				do_query_arrays		(const CString&, const CString&);
	void				do_query_bps		(const CString&, const CString&);
	void				do_query_can		(const CString&, const CString&);
	void				do_query_cfail		(const CString&, const CString&);
	void				do_query_channel	(const CString&, const CString&);
	void				do_query_colour		(const CString&, const CString&);
	void				do_query_commands	(const CString&, const CString&);
	void				do_query_connected	(const CString&, const CString&);
	void				do_query_contents	(const CString&, const CString&);
	void				do_query_controller	(const CString&, const CString&);
	void				do_query_cstring	(const CString&, const CString&);
	void				do_query_csucc		(const CString&, const CString&);
	void				do_query_descendantfrom	(const CString&, const CString&);
	void				do_query_description	(const CString&, const CString&);
	void				do_query_destination	(const CString&, const CString&);
	void				do_query_dictionaries	(const CString&, const CString&);
	void				do_query_drop		(const CString&, const CString&);
	void				do_query_elements	(const CString&, const CString&);
	void				do_query_email		(const CString&, const CString&);
	void				do_query_exist		(const CString&, const CString&);
	void				do_query_exits		(const CString&, const CString&);
	void				do_query_fail		(const CString&, const CString&);
	void				do_query_first_name	(const CString&, const CString&);
	void				do_query_fuses		(const CString&, const CString&);
	void				do_query_gravity_factor	(const CString&, const CString&);
	void				do_query_id		(const CString&, const CString&);
	void				do_query_idletime	(const CString&, const CString&);
	void				do_query_interval	(const CString&, const CString&);
	void				do_query_key		(const CString&, const CString&);
	void				do_query_last_entry	(const CString&, const CString&);
	void				do_query_location	(const CString&, const CString&);
	void				do_query_lock		(const CString&, const CString&);
	void				do_query_mass		(const CString&, const CString&);
	void				do_query_mass_limit	(const CString&, const CString&);
	void				do_query_money		(const CString&, const CString&);
	void				do_query_my		(const CString&, const CString&);
	void				do_query_myself		(const CString&, const CString&);
	void				do_query_name		(const CString&, const CString&);
	void				do_query_next		(const CString&, const CString&);
	void				do_query_numconnected	(const CString&, const CString&);
	void				do_query_odrop		(const CString&, const CString&);
	void				do_query_ofail		(const CString&, const CString&);
	void				do_query_osuccess	(const CString&, const CString&);
	void				do_query_owner		(const CString&, const CString&);
	void				do_query_parent		(const CString&, const CString&);
	void				do_query_pending	(const CString&, const CString&);
	void				do_query_properties	(const CString&, const CString&);
	void				do_query_aliases	(const CString&, const CString&);
	void				do_query_race		(const CString&, const CString&);
	void				do_query_rand		(const CString&, const CString&);
	void				do_query_realtime	(const CString&, const CString&);
	void				do_query_score		(const CString&, const CString&);
	void				do_query_set		(const CString&, const CString&);
	void				do_query_size		(const CString&, const CString&);
	void				do_query_success	(const CString&, const CString&);
	void				do_query_time		(const CString&, const CString&);
	void				do_query_typeof		(const CString&, const CString&);
	void				do_query_variables	(const CString&, const CString&);
	void				do_query_volume		(const CString&, const CString&);
	void				do_query_volume_limit	(const CString&, const CString&);
	void				do_query_weight		(const CString&, const CString&);
	void				do_query_who		(const CString&, const CString&);
	void				do_quit			(const CString&, const CString&);
	void				do_race			(const CString&, const CString&);
	void				do_at_rem		(const CString&, const CString&);
	void				do_remote		(const CString&, const CString&);
	void				do_return		(const CString&, const CString&);
	void				do_returnchain		(const CString&, const CString&);
	void				do_say			(const CString&, const CString&);
	void				do_score		(const CString&, const CString&);
	void				do_set			(const CString&, const CString&);
	void				do_shutdown		(const CString&, const CString&);
	void				do_smd			(const CString&, const CString&);
	void				do_smdread		(const CString&, const CString&);
	void				do_stats		(const CString&, const CString&);
	void				do_swho			(const CString&, const CString&);
	void				do_success		(const CString&, const CString&);
	void				do_tell			(const CString&, const CString&);
	void				do_tellemote		(const CString&, const CString&);
	void				do_terminal_set		(const CString&, const CString&);
	void				do_test			(const CString&, const CString&);
	void				do_truncate		(const CString&, const CString&);
	void				do_unchpid		(const CString&, const CString&);
	void				do_unlink		(const CString&, const CString&);
	void				do_unlock		(const CString&, const CString&);
	void				do_variable		(const CString&, const CString&);
	void				do_version		(const CString&, const CString&);
	void				do_volume		(const CString&, const CString&);
	void				do_volume_limit		(const CString&, const CString&);
	void				do_wall			(const CString&, const CString&);
	void				do_weapon		(const CString&, const CString&);
	void				do_wear			(const CString&, const CString&);
	void				do_welcome		(const CString&, const CString&);
	void				do_whisper		(const CString&, const CString&);
	void				do_wield		(const CString&, const CString&);
	void				do_who			(const CString&, const CString&);
#endif	/* !NO_GAME_CODE */
};


/*
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


class Dependency
{
    private:
			context		*blocker;
			context		*blocked;
			void		(context::*proceed_function) ();
    public:
					Dependency	(context *br, context *bd, void (context::*p) ())	: blocker (br), blocked (bd), proceed_function (p)	{}
					~Dependency	()		{ blocked->execute (proceed_function); }
		const	context		*get_blocker	()	const	{ return blocker; }
};


typedef	Link_stack <context *>	Context_stack;

class	Scheduler
{
    private:
			Context_stack	contexts;
    public:
					Scheduler	()		{}
					~Scheduler	()		{}
		bool			runnable	()	const	{ return !contexts.is_empty (); }
		context			*step		();
		void			push_job	(context *c)	{ c->set_scheduled (true); contexts.push (c); }
		context			*push_express_job	(context *c);
};

/* The only one there is */
extern	Scheduler	mud_scheduler;

#endif	/* _CONTEXT_H */

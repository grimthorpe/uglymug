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
		char			*name;
		char			*value;
    public:
					String_pair	(const char *n, const char *v);
					~String_pair	();
		void			set_value	(const char *v);
	const	char			*get_name	() 	const	{ return name; }
	const	char			*get_value	()	const	{ return value; }
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
		String_pair	*addarg			(const char *n, const char *v);
		String_pair	*check_and_add_arg	(const char *n, const char *v);
		Boolean		updatearg		(const char *n, const char *v);
		String_pair	*locatearg		(const char *name)	const;

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
	virtual		void		do_at_elseif		(const Boolean ok);
			String_pair	*locate_stack_arg	(const char *name)	const;
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
			Boolean		if_ok;
			Boolean		no_endif;
			int		stop_line;
			int		endif_line;
    protected:
	virtual	const	int		line_for_outer_scope	()	const;
    public:
					If_scope	(const Scope &os, Boolean i);
	virtual				~If_scope	()			{}
			Command_action	step_once	(context *);
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
	virtual		void		do_at_elseif	(const Boolean ok);
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
	virtual		Boolean		loopagain	()			= 0;
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
	virtual		Boolean		loopagain	();
    public:
					For_loop	(const Scope &os, int in_start, int in_end, int in_step, const char *name);
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
	virtual		Boolean		loopagain	();
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
			void		pend_fuse		(dbref fuse, Boolean success, const char *simple_command, const char *arg1, const char *arg2, const Matcher &matcher);
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
		const	Boolean		inside_subscope		()	const		{ return !scope_stack.is_empty(); }
			Boolean		push_scope		(Scope *s)		{ return scope_stack.push (s); }
			Command_action	step_once	(context *);
			void		do_at_elseif		(const Boolean ok);
			void		do_at_end		(context &);
			void		do_at_return		(context &);
			void		do_at_returnchain	(context &);
		const	Scope		&innermost_scope	()	const;
	static	const	int		parse_command	(object *cmd, const int start_line, char *errs);
		String_pair		*locate_innermost_arg	(const char *name)	const;
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
		const	char		*return_string;
		Command_status		return_status;
		Boolean			called_from_command;
		context &		creator;
		Boolean			scheduled;
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
	const	Boolean			set_effective_id	(dbref i);
		void			set_unchpid_id		(dbref i)	{ unchpid_id = i; }
		void			set_scheduled		(Boolean s)	{ scheduled = s; }
	const	Boolean			get_scheduled		()	const	{ return scheduled; }
		void			set_return_string	(const char *rs);
		void			maybe_free		(const char *s)	const;
		void			calling_from_command	()		{ called_from_command = True; }
	const	int			get_commands_executed	()	const	{ return (commands_executed); }
	const	int			get_sneaky_executed_depth()	const	{ return (sneaky_executed_depth); }
		void			set_sneaky_executed_depth(int d)	{ sneaky_executed_depth = d; }
	const	int			get_depth_limit		()	const	{ return (depth_limit); }
	const	dbref			get_player		()	const	{ return (player); }
	const	dbref			get_unchpid_id		()	const	{ return (unchpid_id); }
	const	Command_status		get_return_status	()	const	{ return (return_status); }
	const	char			*get_innermost_arg1	()	const;
	const	char			*get_innermost_arg2	()	const;
	const	Boolean			in_command		()	const;
	const	dbref			get_current_command	()	const;
	const	Boolean			gagged_command		()	const	{ return ((get_current_command () != NOTHING) && Silent (get_current_command ())); }
	const	char			*get_return_string	()	const	{ return (return_string); }
	const	dbref			get_effective_id	()	const;
		void			copy_returns_from	(const context &source);
	const	Boolean			controls_for_read	(const dbref what) const;
	const	Boolean			controls_for_write	(const dbref what) const;
		String_pair		*locate_innermost_arg	(const char *name)	const;
#ifndef	NO_GAME_CODE
	Boolean				allow_another_step	();
	void				log_recursion		(dbref command, const char *arg1, const char *arg2);
	void				process_basic_command	(const char *);
	const	char			*sneakily_process_basic_command	(const char *, Command_status &);
	Boolean				can_do_compound_command	(const char *command, const char *arg1, const char *arg2);
	Boolean				can_override_command (const char *command, const char *arg1, const char *arg2);
	Command_action			do_compound_command	(dbref command, const char *simple_command, const char *arg1, const char *arg2, dbref effective_id = NOTHING, Matcher &matcher = *(Matcher *) NULL);

	/* Functions from move.c that needed to be inside a context */
	void				enter_room              (dbref loc);
	void				send_home		(dbref thing);
	void				maybe_dropto		(dbref loc, dbref dropto);
	void				send_contents		(dbref loc, dbref dropto);

	const	Boolean			variable_substitution	(const char *arg, char *result, unsigned int max_length);
	const	Boolean			nested_variable_substitution	(const char *&, char *, const int, const int space_left);
	const	Boolean			dollar_substitute	(const char *&argp, char *&resp, const int depth, unsigned int space_left);
		void			brace_substitute	(const char *&, char *&, unsigned int space_left);

	void				do_alarm		(const char *name, const char *command_name);
	void				do_ammo_type		(const char *name, const char *command_name);
	void				do_ammunition		(const char *name, const char *command_name);
	void				do_armour		(const char *name, const char *command_name);
	void				do_array		(const char *name, const char *dummy);
	void				do_at_censor		(const char *name, const char *command_name);
	void				do_at_censorinfo	(const char *name, const char *command_name);
	void				do_at_command		(const char *name, const char *command);
	void				do_at_connect		(const char *player, const char *dummy);
	void				do_at_disconnect	(const char *player, const char *dummy);
	void				do_at_drop		(const char *name, const char *message);
	void				do_at_else		(const char *, const char *);
	void				do_at_elseif		(const char *, const char *);
	void				do_at_endif		(const char *, const char *);
	void				do_at_end		(const char *, const char *);
	void				do_at_exclude		(const char *name, const char *command_name);
	void				do_at_false		(const char *dummy, const char *dummy2);
	void				do_at_for		(const char *, const char *);
	void				do_at_global		(const char *, const char *);
	void				do_at_if		(const char *, const char *);
	void				do_at_insert		(const char *, const char *);
	void				do_at_key		(const char *object, const char *keyname);
	void				do_at_list		(const char *, const char *);
	void				do_at_listcolours	(const char *, const char *);
	void				do_at_local		(const char *, const char *);
	void				do_at_lock		(const char *name, const char *keyname);
	void				do_at_motd		(const char *dummy1, const char *dummy2);
	void				do_at_note		(const char *arg1, const char *arg2);
	void				do_at_notify		(const char *arg1, const char *arg2);
	void				do_at_oecho		(const char *arg1, const char *arg2);
	void				do_at_oemote		(const char *arg1, const char *arg2);
	void				do_at_open		(const char *direction, const char *linkto);
	void				do_at_queue		(const char *player, const char *what);
	void				do_at_recursionlimit	(const char *limit, const char *dummy);
	void				do_at_score		(const char *name, const char *value);
	void				do_at_sort		(const char *, const char *);
	void				do_at_true		(const char *dummy, const char *dummy2);
	void				do_at_uncensor		(const char *name, const char *command_name);
	void				do_at_unexclude		(const char *name, const char *command_name);
	void				do_at_unlock		(const char *name, const char *dummy);
	void				do_at_who		(const char *victim, const char *string);
	void				do_at_with		(const char *, const char *);
	void				do_attack		(const char *, const char *);
	void				do_attributes		(const char *, const char *);
	void				do_beep			(const char *dummy, const char *dummy2);
	void				do_boot			(const char *victim, const char *reason);
	void				do_broadcast		(const char *dummy, const char *dummy2);
	void				do_cfailure		(const char *name, const char *command_name);
	void				do_channel		(const char *arg1, const char *arg2);
	void				do_channel_who		(const char *name, const char *);
	void				do_chat			(const char *arg1, const char *arg2);
	void				do_chpid		(const char *dummy1, const char *dummy2);
	void				do_close		(const char *object, const char *dummy);
	void				do_colour		(const char *cia, const char *colour_codes);
	void				do_container		(const char *object, const char *description);
	void				do_controller		(const char *name, const char *other);
	void				do_create		(const char *name, const char *description);
	void				do_credit		(const char *arg1, const char *arg2);
	void				do_cstring		(const char *object, const char *description);
	void				do_csuccess		(const char *name, const char *command_name);
	void				do_debit		(const char *arg1, const char *arg2);
	void				do_decompile		(const char *name, const char *args);
	void				do_describe		(const char *name, const char *description);
	void				do_destroy		(const char *name, const char *dummy);
	void				do_dictionary		(const char *name, const char *dummy);
	void				do_dig			(const char *name, const char *description);
	void				do_drop			(const char *name, const char *where);
	void				do_dump			(const char *dummy1, const char *dummy2);
	void				do_echo			(const char *arg1, const char *arg2);
	void				do_email		(const char *name, const char *email_addr);
	void				do_empty		(const char *name, const char *email_addr);
	void				do_enter		(const char *object, const char *dummy);
	void				do_event		(const char *npc, const char *event);
	void				do_examine		(const char *name, const char *dummy);
	void				do_fail			(const char *name, const char *message);
	void				do_fchat		(const char *name, const char *message);
	void				do_find			(const char *descriptor, const char *string);
	void				do_force		(const char *what, const char *command);
	void				do_from			(const char *name, const char *dummy);
	void				do_full_evaluate	(const char *expleft, const char *expright);
	void				do_fuse			(const char *name, const char *command_name);
	void				do_fwho			(const char *name, const char *message);
	void				do_garbage_collect	(const char *dummy1, const char *dummy2);
	void				do_get			(const char *what, const char *where);
	void				do_give			(const char *recipient, const char *object_or_amount);
	void				do_gravity_factor	(const char *name, const char *value);
	void				do_group		(const char *group_string, const char *canvasser_string);
	void				do_gripe		(const char *arg1, const char *arg2);
#ifdef ALIASES
	void				do_unalias		(const char *person, const char *alias);
	void				do_alias		(const char *person, const char *alias);
	void				do_listaliases		(const char *person, const char *notused);
#endif /* ALIASES */
	void				do_inventory		(const char *dummy1, const char *dummy2);
	void				do_kill			(const char *name, const char *amount);
	void				do_ladd			(const char *arg1, const char *arg2);
	void				do_leave		(const char *dummy1, const char *dummy2);
	void				do_load			(const char *dummy1, const char *dummy2);
	void				do_lftocr		(const char *z, const char *dummy);
	void				do_link			(const char *name, const char *room_name);
	void				do_llist		(const char *arg1, const char *);
	void				do_location		(const char *name, const char *new_location);
	void				do_lock			(const char *object, const char *dummy);
	void				do_look_at		(const char *name, const char *dummy);
	void				do_lremove		(const char *arg1, const char *arg2);
	void				do_lset			(const char *arg1, const char *arg2);
	void				do_parent		(const char *name, const char *parent_name);
	void				do_peak			(const char *number, const char *dummy);
	void				do_mass			(const char *name, const char *value);
	void				do_mass_limit		(const char *name, const char *value);
	void				do_modify		(const char *target, const char *settings);
	void				do_move			(const char *direction, const char *dummy);
	void				do_name			(const char *name, const char *newname);
	void				do_natter		(const char *arg1, const char *arg2);
	void				do_newpassword		(const char *name, const char *password);
	void				do_notify		(const char *arg1, const char *arg2);
	void				do_odrop		(const char *name, const char *message);
	void				do_ofail		(const char *name, const char *message);
	void				do_open			(const char *object, const char *dummy);
	void				do_osuccess		(const char *name, const char *message);
	void				do_owner		(const char *name, const char *newowner);
	void				do_page			(const char *arg1, const char *arg2);
	void				do_password		(const char *old, const char *newpw);
	void				do_pcreate		(const char *name, const char *password);
	void				do_pemote		(const char *name, const char *what);
	void				do_pose			(const char *arg1, const char *arg2);
	void				do_property		(const char *name, const char *value);
	void				do_puppet		(const char *name, const char *dummy);
	void				do_query_address	(const char *const name, const char *const dummy);
	void				do_query_area		(const char *const name, const char *const the_area);
	void				do_query_article	(const char *const name, const char *const others);
	void				do_query_arrays		(const char *const name, const char *const dummy);
	void				do_query_bps		(const char *const name, const char *const dummy);
	void				do_query_cfail		(const char *const name, const char *const dummy);
	void				do_query_channel	(const char *const name, const char *const dummy);
	void				do_query_colour		(const char *const name, const char *const dummy);
	void				do_query_commands	(const char *const name, const char *const dummy);
	void				do_query_connected	(const char *const name, const char *const dummy);
	void				do_query_contents	(const char *const name, const char *const dummy);
	void				do_query_controller	(const char *const name, const char *const dummy);
	void				do_query_cstring	(const char *const name, const char *const dummy);
	void				do_query_csucc		(const char *const name, const char *const dummy);
	void				do_query_descendantfrom	(const char *const first, const char *const parent);
	void				do_query_description	(const char *const name, const char *const dummy);
	void				do_query_destination	(const char *const name, const char *const dummy);
	void				do_query_dictionaries	(const char *const name, const char *const dummy);
	void				do_query_drop		(const char *const name, const char *const dummy);
	void				do_query_elements	(const char *const name, const char *const dummy);
	void				do_query_email		(const char *const name, const char *const dummy);
	void				do_query_exist		(const char *const name, const char *const owner_string);
	void				do_query_exits		(const char *const name, const char *const dummy);
	void				do_query_fail		(const char *const name, const char *const dummy);
	void				do_query_first_name	(const char *const name, const char *const dummy);
	void				do_query_fuses		(const char *const name, const char *const dummy);
	void				do_query_gravity_factor	(const char *const name, const char *const dummy);
	void				do_query_id		(const char *const name, const char *const dummy);
	void				do_query_idletime	(const char *const name, const char *const dummy);
	void				do_query_interval	(const char *const name, const char *const dummy);
	void				do_query_key		(const char *const name, const char *const dummy);
	void				do_query_last_entry	(const char *const name, const char *const dummy);
	void				do_query_location	(const char *const name, const char *const dummy);
	void				do_query_lock		(const char *const name, const char *const dummy);
	void				do_query_mass		(const char *const name, const char *const dummy);
	void				do_query_mass_limit	(const char *const name, const char *const dummy);
	void				do_query_money		(const char *const name, const char *const dummy);
	void				do_query_my		(const char *const type, const char *const arg);
	void				do_query_myself		(const char *const dummy1, const char *const dummy2);
	void				do_query_name		(const char *const name, const char *const dummy);
	void				do_query_next		(const char *const name, const char *const dummy);
	void				do_query_numconnected	(const char *const, const char *const);
	void				do_query_odrop		(const char *const name, const char *const dummy);
	void				do_query_ofail		(const char *const name, const char *const dummy);
	void				do_query_osuccess	(const char *const name, const char *const dummy);
	void				do_query_owner		(const char *const name, const char *const dummy);
	void				do_query_parent		(const char *const name, const char *const dummy);
	void				do_query_pending	(const char *const dummy1, const char *const dummy2);
	void				do_query_properties	(const char *const name, const char *const dummy);
	void				do_query_aliases	(const char *const name, const char *const dummy);
	void				do_query_race		(const char *const name, const char *const dummy);
	void				do_query_rand		(const char *const value, const char *const dummy);
	void				do_query_realtime	(const char *const name, const char *const dummy);
	void				do_query_score		(const char *const name, const char *const dummy);
	void				do_query_set		(const char *const name, const char *const flag);
	void				do_query_size		(const char *const name, const char *const dummy);
	void				do_query_success	(const char *const name, const char *const dummy);
	void				do_query_time		(const char *const dummy1, const char *const dummy2);
	void				do_query_typeof		(const char *const name, const char *const type);
	void				do_query_variables	(const char *const name, const char *const dummy);
	void				do_query_volume		(const char *const name, const char *const dummy);
	void				do_query_volume_limit	(const char *const name, const char *const dummy);
	void				do_query_weight		(const char *const name, const char *const  dummy);
	void				do_query_who		(const char *const name, const char *const dummy);
	void				do_quit			(const char *dummy1, const char *dummy2);
	void				do_race			(const char *name, const char *newrace);
	void				do_remote		(const char *loc_string, const char *command);
	void				do_return		(const char *arg1, const char *arg2);
	void				do_returnchain		(const char *arg1, const char *arg2);
	void				do_say			(const char *arg1, const char *arg2);
	void				do_score		(const char *dummy1, const char *dummy2);
	void				do_set			(const char *name, const char *flag);
	void				do_shutdown		(const char *arg1, const char *arg2);
	void				do_smd			(const char *arg1, const char *arg2);
	void				do_smdread		(const char *dummy1, const char *dummy2);
	void				do_stats		(const char *name, const char *dummy);
	void				do_swho			(const char *dummy1, const char *dummy2);
	void				do_success		(const char *name, const char *message);
	void				do_tell			(const char *arg1, const char *arg2);
	void				do_tellemote		(const char *arg1, const char *arg2);
	void				do_terminal_set		(const char *command, const char *arg);
	void				do_test			(const char *arg1, const char *arg2);
	void				do_truncate		(const char *dummy1, const char *dummy2);
	void				do_unchpid		(const char *dummy1, const char *dummy2);
	void				do_unlink		(const char *name, const char *dummy);
	void				do_unlock		(const char *object, const char *dummy);
	void				do_variable		(const char *name, const char *value);
	void				do_version		(const char *, const char *);
	void				do_volume		(const char *name, const char *value);
	void				do_volume_limit		(const char *name, const char *value);
	void				do_wall			(const char *arg1, const char *arg2);
	void				do_weapon		(const char *arg1, const char *arg2);
	void				do_wear			(const char *arg1, const char *arg2);
	void				do_welcome		(const char *arg1, const char *arg2);
	void				do_whisper		(const char *arg1, const char *arg2);
	void				do_wield		(const char *arg1, const char *arg2);
	void				do_who			(const char *name, const char *dummy);
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
		Boolean			runnable	()	const	{ return !contexts.is_empty (); }
		context			*step		();
		void			push_job	(context *c)	{ c->set_scheduled (True); contexts.push (c); }
		context			*push_express_job	(context *c);
};

/* The only one there is */
extern	Scheduler	mud_scheduler;

#endif	/* _CONTEXT_H */

/* static char SCCSid[] = "@(#)context.c	1.26\t9/25/95"; */

/*
 * context.c: Things to do with execution contexts.
 *
 *	Peter Crowther, 7/1/94.
 *
 * Modified by Dunk to add control structures.
 * Heavily modified by PJC 24/12/96.
 */

#include <stdio.h>
#include "db.h"
#include "alarm.h"
#include "command.h"
#pragma implementation "context.h"
#include "context.h"
#include "externs.h"
#include "interface.h"

#include "config.h"
#include "log.h"

#define	ASSIGN_STRING(d,s)	{ if (d) free (d);  if ((s) && *(s)) (d)=strdup(s); else (d)=NULL; }


/* The only one there is */
Scheduler	mud_scheduler;

context context::DEFAULT_CONTEXT (true);

/************************************************************************/
/*									*/
/*				Scope					*/
/*									*/
/************************************************************************/

Scope::Scope (
const	Scope	&os)
: outer_scope (&os), current_line((&os)?outer_scope->line_for_inner_scope():1)

{
}


/**
 * Delete any local variables
 */

Variable_stack::~Variable_stack()
{
	while (!m_string_pair_stack.empty ())
	{
		delete m_string_pair_stack.back ();
		m_string_pair_stack.pop_back ();
	}
}


String_pair *
Variable_stack::addarg (
const	String& n,
const	String& v)

{
	m_string_pair_stack.push_back (new String_pair (n, v));
	return m_string_pair_stack.back ();
}


String_pair *
Variable_stack::check_and_add_arg (
const	String& n,
const	String& v)

{
	if (locatearg (n))
		return 0;
	else
		return addarg (n, v);
}


bool
Variable_stack::updatearg (
const	String& name,
const	String& val)

{
	String_pair	*temp;

	if (!(temp = locatearg (name))) /* This shouldn't happen! */
	{
		log_bug("Variable_stack::updatearg: trying to find non-existent argument: %s", name.c_str());
		return false;
	}
	temp->set_value (val);
	return true;
}


String_pair *
Variable_stack::locatearg (
const	String& name)
const

{
	for (std::list<String_pair *>::const_reverse_iterator i (m_string_pair_stack.rbegin ()); i != m_string_pair_stack.rend (); ++i)
	{
		if (!string_compare ((*i)->name(), name))
			return *i;
	}

	/* If we get here, we didn't find it on this stack*/
	return 0;
}

String_pair *
Scope::locate_stack_arg (
const String& name)
const
{
	String_pair *result;
	if ((result= locatearg(name)))
		return result;
	/* If we get here we didn't find the result in this scope */
	else if (outer_scope)
		return outer_scope->locate_stack_arg (name);
	else
		return 0;
}


/*
 * line_for_inner_scope: A scope immediately inside us needs to know where it
 *	starts.  As this is always called during the time that we're on the
 *	start line, we can simply return our notion of the current line.
 *
 * PJC 23/1/97: Oh no we're not, we're one past the start line...
 */

const int
Scope::line_for_inner_scope ()
const

{
	return current_line - 1;
}


void
Scope::do_at_elseif (
const	bool	ok)

{
	log_bug("Scope::do_at_elseif called");
}


/************************************************************************/
/*									*/
/*			Command_and_arguments				*/
/*									*/
/************************************************************************/

Command_and_arguments::Command_and_arguments (
const	String&sc,
const	String&a1,
const	String&a2,
	Matcher	*m)
: matcher ((Matcher *)NULL)
, sticky_fuses (0)
, simple_command (a1)
, arg1 (a1)
, arg2 (a2)
{
	set_matcher (m);
}


Command_and_arguments::~Command_and_arguments ()

{
	set_matcher ((Matcher *)NULL);
}

void Command_and_arguments::set_matcher(const Matcher *use_this_one)
{
	// In some cases the delete would not be needed, ie just use copy 
	// constructor.  Doubt it would be much quicker anyway.
	if(matcher)
	{
		delete matcher;
		matcher = (Matcher *)NULL;
	}
	if(use_this_one)
		matcher = new Matcher(*use_this_one);
}

void
Command_and_arguments::set_simple_command (
const	String& s)
{
	simple_command = s;
}


void
Command_and_arguments::set_arg1 (
const	String& s)
{
	arg1 = s;
}


void
Command_and_arguments::set_arg2 (
const	String& s)
{
	arg2 = s;
}


void
Command_and_arguments::pend_fuse (
dbref		fuse,
bool		success,
const	String& simple_command,
const	String& arg1,
const	String& arg2,
const	Matcher	&matcher)

{
	Pending_fuse	*pendant;

	pendant = new Pending_fuse (fuse, success, simple_command, arg1, arg2, matcher);
	pendant->insert_into ((Pending **) &sticky_fuses);
}


/************************************************************************/
/*									*/
/*			Compound_command_and_arguments			*/
/*									*/
/************************************************************************/

Compound_command_and_arguments::Compound_command_and_arguments (
	dbref	cmd,
	context *c,
const	String& sc,
const	String& a1,
const	String& a2,
const	dbref	eid,
	Matcher	*m,
	bool	silent)
: Command_and_arguments (sc, a1, a2, m)
, Scope (*(Scope *)0)
, command(NOTHING)
, player(c->get_player())
, effective_id (eid)
, csucc_cache (NOTHING)
, cfail_cache (NOTHING)
, gagged(silent)
{
	const	char	*err;

//	player = c->get_player();
	if ((err = (set_command (cmd))) != 0)
	{
		notify(c->get_player(), "%s %s", err, unparse_object(*c, cmd));
		set_command (NOTHING);
	}
	else if(Silent(cmd))
		gagged = true;
}


Compound_command_and_arguments::~Compound_command_and_arguments ()

{
	/* Delete any active scopes (may be left after recursion) */
	empty_scope_stack ();
}


/**
 * Remove and delete any remaining scopes.
 */

void
Compound_command_and_arguments::empty_scope_stack ()

{
	while (!scope_stack.empty())
	{
		delete scope_stack.top ();
		scope_stack.pop ();
	}
}


const char *
Compound_command_and_arguments::set_command (
dbref	c)

{
	static	char	errs [BUFFER_LEN];
	command = c;
	current_line = 1;

	/* Set up the caches */
	*errs = '\0';
	if (command != NOTHING)
	{
		csucc_cache = db [c].get_inherited_csucc ();
		cfail_cache = db [c].get_inherited_cfail ();
		if (db [c].alloc_parse_helper ())
			parse_command (db + c, 1, errs);
		if(Silent(command))
			gagged = true;
	}
	else
	{
		csucc_cache = NOTHING;
		cfail_cache = NOTHING;
	}

	/* Did anybody print an error? */
	if (*errs)
	{
		db [c].flush_parse_helper ();
		return errs;
	}
	else
		return 0;
}


/*
 * locate_innermost_arg: Someone's just referenced an argument in eg. a variable
 *	substitution.  Check whether it's one of the local variables or loop
 *	arguments in the current command; if so, return it.
 */

String_pair *
Compound_command_and_arguments::locate_innermost_arg (
const	String& name)
const

{
	if (scope_stack.empty ())
		return locatearg (name);
	else
		return scope_stack.top ()->locate_stack_arg (name);
}


const Scope &
Compound_command_and_arguments::innermost_scope ()
const

{
	if (scope_stack.empty ())
		return *this;
	else
		return *scope_stack.top ();
}


/*
 * step: Do one (part of a) command in a compound command.  This involves
 *	asking the currently active scope (which may well be us if nothing
 *	else is invoked) to step, and then clearing up the mess. If we've 
 *	finished executing we set up the next command.
 *
 * Return values:
 *	ACTION_HALT	 Stop - we've recursed.
 *	ACTION_STOP	 This scope/cmd has finished executing
 *	ACTION_CONTINUE	 This nesting level needs stepping again
 *	ACTION_RESTART	 Nope, this match failed.  Delete me, restart the matcher and try again.
 */

Command_action
Compound_command_and_arguments::step (
context		*context)

{
	Command_action	action=ACTION_UNSET;

	/* If we've executed too many commands, give up */
	if (!context->allow_another_step ())
	{
		// First nuke the scope stack
		empty_scope_stack ();
		log_recursion(	player, db[player].get_name(),
						command, db[command].get_name(),
						reconstruct_message(get_arg1(), get_arg2())
		);
		
		return ACTION_HALT;
	}

	/* Check for a parse error */
	if (command == NOTHING)
	{
		empty_scope_stack ();
		notify_colour (context->get_player (), context->get_player (), COLOUR_ERROR_MESSAGES, "Parse failure in command");
		return ACTION_STOP;
	}

	/* Check to see if the command has been destroyed. If it has been 'replaced' then assume the replacer knows what they're doing. */
	if(Typeof(command) != TYPE_COMMAND)
	{
		current_line = -1;
	}
	/* Check for a return from the chain */
	if ((current_line != -1) && (current_line <= (int)db[command].get_inherited_number_of_elements()))
	{
		if (action==ACTION_UNSET)
			action= (scope_stack.empty() ? step_once (context) :  scope_stack.top ()->step_once (context));
		switch (action)
		{
			case ACTION_HALT:
				/* This should never happen.. Deal with it anyway */
				log_bug("ACTION_HALT returned to Compound_command_and_arguments::step()");
				empty_scope_stack ();
				return ACTION_HALT;
	
			case ACTION_UNSET:
				log_bug("ACTION_UNSET returned to Compound_command_and_arguments::step()");
				/* Fallthrough */

			case ACTION_STOP:
				/* The scope doesn't want to step again */
				if (!scope_stack.empty ())
				{
					/* Let the next outermost scope do its stuff */
					int	next_line = scope_stack.top ()->line_for_outer_scope ();
					delete scope_stack.top ();
					scope_stack.pop ();
					if (scope_stack.empty ())
						set_current_line (next_line);
					else
						scope_stack.top ()->set_current_line (next_line);
					action = ACTION_CONTINUE;
				}
				break;

			case ACTION_CONTINUE:
				/* We want to continue, which is fine */
				break;
			case ACTION_RESTART:
				/* We're restarting.  Clean up outstanding scopes. */
				empty_scope_stack ();
				break;
		}
	}
	else
	{
		/*
		 * Set up next command. First remove any cruft from this one
		 *	(there might be some left if we've @returned)
		 */
		empty_scope_stack ();

		dbref	player = context->get_player ();
		context->command_executed();

#ifdef LOG_COMMANDS
		log_command(player,											/* playerid */
					db[player].get_name(),							/* playername */
					get_effective_id(),								/* effectiveid */
					db[get_effective_id()].get_name(),				/* effectivename */
					db[player].get_location(),						/* locationid */
					db[db[player].get_location()].get_name(),		/* locationid */
					db [command].get_name ()						/* command */
		);
#endif

		/* Now work out where to go */
		switch (context->get_return_status ())
		{
			case COMMAND_FAIL:
				/* follow the failure route if possible */
				if (Typeof (command) != TYPE_FREE)
				{
					if (db [command].get_inherited_fail_message ())
						notify_public_colour (player, player, COLOUR_FAILURE, "%s", db[command].get_inherited_fail_message ().c_str());
					if ((!Silent (player)) && db [command].get_inherited_ofail ())
					{
						pronoun_substitute (scratch_buffer, BUFFER_LEN, player, db[command].get_inherited_ofail ());
						notify_except (db[db[player].get_location ()].get_contents (), player, player, scratch_buffer);
					}
				}
				else
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Something destroyed the command you're executing. Continuing anyway...");
				if (cfail_cache == HOME)
				{
					set_effective_id(player);
					set_command (NOTHING);
					return ACTION_RESTART;
				}
				else if ((cfail_cache != NOTHING) && (Typeof (cfail_cache) == TYPE_COMMAND) && (could_doit (*context, cfail_cache)))
				{
					int temp_cache=cfail_cache;
					const char *err;
					if ((err = set_command (cfail_cache)))
					{
						notify_colour (player, player, COLOUR_FAILURE, "%s %s", err, unparse_object(*context, temp_cache));
						set_command (NOTHING);
					}
				}
				else
					set_command (NOTHING);
				break;
			case COMMAND_SUCC:
				/* follow the success route if possible */
				if (Typeof (command) != TYPE_FREE)
				{
					if(db[command].get_inherited_succ_message ())
						notify_public_colour(player, player, COLOUR_SUCCESS, "%s", db[command].get_inherited_succ_message ().c_str());
					if((!Silent (player)) && db[command].get_inherited_osuccess () && !Dark(player))
					{
						pronoun_substitute(scratch_buffer, BUFFER_LEN, player, db[command].get_inherited_osuccess ());
						notify_except(db[db[player].get_location ()].get_contents (), player, player, scratch_buffer);
					}
				}
				else
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Something destroyed the command you're executing. Continuing anyway...");
				if (csucc_cache == HOME)
				{
					set_effective_id(player);
					set_command (NOTHING);
					return ACTION_RESTART;
				}
				else if ((csucc_cache != NOTHING) && (Typeof (csucc_cache) == TYPE_COMMAND) && (could_doit (*context, csucc_cache)))
				{
					int temp_cache=csucc_cache;
					const char *err;
					if ((err = set_command (csucc_cache)))
					{
						notify_colour (player, player, COLOUR_FAILURE, "%s %s", err, unparse_object(*context, temp_cache));
						set_command (NOTHING);
					}
				}
				else
					set_command (NOTHING);
				break;
			case COMMAND_EXITHOME:
				set_effective_id (player);
				set_command (NOTHING);
				return ACTION_RESTART;
			case COMMAND_HALT:
				set_command(NOTHING);
				return ACTION_HALT;
			default:
				log_bug("player %s(#%d) ran command %s(#%d) which returned an invalid boolean");
				set_command (NOTHING);
		}
		action= (command==NOTHING)? ACTION_STOP : ACTION_CONTINUE;
	}
	
	/* That's all we need to do based on this step. */
	return action;
}


/*
 * step_once: This is the compound command version, which goes linearly down a
 *	command until it falls off the end, then steps to the next command in
 *	the chain.
 */

Command_action
Compound_command_and_arguments::step_once (
context	*c)

{
	//const	char	*err;
	char	command_block[MAX_COMMAND_LEN];

	/* current_line incremented here in case the command modifies it */
	current_line+=db[command].reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, current_line);
	c->process_basic_command (command_block);
	return ACTION_CONTINUE;
}


void
Compound_command_and_arguments::do_at_elseif (
const	bool	if_ok)

{
#ifdef	DEBUG
	if (scope_stack.empty ())
	{
		log_bug ("Compound command do_at_elseif called with no if scope active");
		return;
	}
#endif	/* DEBUG */

	scope_stack.top ()->do_at_elseif (if_ok);
}


void
Compound_command_and_arguments::do_at_return (
context	&c)

{
	/* We're at the end of a step, so simply setting current_line somewhere past the end of the command will work */
	if (command != NOTHING)
		current_line = db [command].get_inherited_number_of_elements () + 1;
	else
		current_line= 1;
}


void
Compound_command_and_arguments::do_at_returnchain (
context	&c)

{
	/* A current_line less than zero will ditch the chain in step_once */
	current_line = -1;
}


/************************************************************************/
/*									*/
/*				context					*/
/*									*/
/************************************************************************/

context::context (const bool is_default)
: Command_and_arguments (NULLSTRING, NULLSTRING, NULLSTRING, 0)
, player (NOTHING)
, trace_command (NOTHING)
, unchpid_id (NOTHING)
, commands_executed (0)
, sneaky_executed_depth (0)
, step_limit (COMPOUND_COMMAND_BASE_LIMIT)
, depth_limit (10)
, return_string (unset_return_string)
/* Initialisation changed to SUCC from INIT by PJC 14/1/96 */
, return_status (COMMAND_SUCC)
, called_from_command (false)
, creator(*this)
, m_scheduled (false)
, dependency (0)

{
	if (!is_default)
		throw "TODO: Better exception to say that is_default must be true for the default context (and there are no others constructed this way)";
}

context::context (
dbref	new_player,
const context& your_maker)
: Command_and_arguments (NULLSTRING, NULLSTRING, NULLSTRING, 0)
, player (new_player)
, trace_command (NOTHING)
, unchpid_id (new_player)
, commands_executed (0)
, sneaky_executed_depth (0)
, step_limit (your_maker.step_limit)
, depth_limit (your_maker.depth_limit)
, return_string (unset_return_string)
/* Initialisation changed to SUCC from INIT by PJC 14/1/96 */
, return_status (COMMAND_SUCC)
, called_from_command (false)
, creator(your_maker)
, m_scheduled (false)
, dependency (0)

{
	set_effective_id(new_player);
}


context::~context ()

{
}


bool
context::allow_another_step ()

{
	if (commands_executed > step_limit)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Too many steps in command");
		return false;
	}

	return true;
}


void
context::copy_returns_from (
const	context	&source)

{
	return_status = source.return_status;
	set_return_string (source.return_string);
}


const bool
context::really_in_command ()
const
{
	return (!call_stack.empty () || (get_current_command() != NOTHING));
}

const bool
context::in_command ()
const

{
/*
 * We used to check commands_executed here, but it screws up brace_expand
 * when used on a compound command at the top level.
 *
 * AJS, 4 Feb 2000
 */
/* Oh, and some things want to check if we're *REALLY* in a command right now,
 * which really screws up with @force
 */
	return (!call_stack.empty () || called_from_command || (get_current_command() != NOTHING));
}


const String&
context::get_innermost_arg1 ()
const

{
	if (call_stack.empty ())
		return get_arg1 ();
	else
		return call_stack.top ()->get_arg1 ();
}


const String&
context::get_innermost_arg2 ()
const

{
	if (call_stack.empty ())
		return get_arg2 ();
	else
		return call_stack.top ()->get_arg2 ();
}


const dbref
context::get_current_command ()
const

{
	if (call_stack.empty ())
		return (NOTHING);
	else
		return (call_stack.top ()->get_command ());
}


const bool
context::set_effective_id (
dbref	id)

{
	if (call_stack.empty ())
		return (false);
	else
	{
		call_stack.top ()->set_effective_id (id);
		return (true);
	}
}


const dbref
context::get_effective_id ()
const

{
	if (call_stack.empty ())
		return (player);
	else
		return (call_stack.top ()->get_effective_id ());
}


/*
 * locate_innermost_arg: Someone's just referenced an argument in eg. a variable
 *	substitution.  Check whether it's one of the local variables or loop
 *	arguments in the current command; if so, return it.
 */

String_pair *
context::locate_innermost_arg (
const	String& name)
const

{
	if (call_stack.empty ())
		return 0;
	else
		return call_stack.top ()->locate_innermost_arg (name);
}

/*
 * step: Step through one basic command, or part of a basic command if it
 *	needs to do some command-related checks in the middle.  The rule is
 *	that a step is allowed to kick off other contexts, but must not
 *	directly cause their processing.  If the context needs to know the
 *	result, it should use a Dependency.
 *
 * Stepping a context executing a basic command causes that command to be
 *	(partially or completely) executed.  Stepping a context running a
 *	compound command causes the context to step the innermost scope, and
 *	then to clean up afterwards.
 *
 * Return values:
 *	ACTION_STOP	The context has reached its conclusion and can be deleted
 *	ACTION_CONTINUE	The context wishes to be stepped again
 *			(although it may have generated a Dependency which means
 *			it cannot be stepped immediately).
 *	ACTION_RESTART	should never be generated.
 */

const
Command_action
context::step ()

{
	/* Are we running a command at all? - the outer context may not know and may call us anyway */
	if (call_stack.empty ())
	{
		fire_sticky_fuses (*this);
		return ACTION_STOP;
	}

	/* Do one thing in the command */
	switch (call_stack.top ()->step (this))
	{
		case ACTION_HALT:
			/* IE Recursion */
			if (!call_stack.empty())
				call_stack.top ()->fire_sticky_fuses (*this);
			while (!call_stack.empty())
			{
				delete call_stack.top ();
				call_stack.pop ();
			}
			fire_sticky_fuses(*this);
			return_status=COMMAND_HALT;
			return ACTION_STOP;
		case ACTION_STOP:
			/* The command chain has come to its end */
			if (!call_stack.empty())
			{
				call_stack.top ()->fire_sticky_fuses (*this);
				delete call_stack.top ();
				call_stack.pop ();
			}
			else
				log_bug("empty call stack when ACTION_STOP returned to context::step()");

			/* If that's all we've got to do, we've finished */
			if (call_stack.empty ())
			{
				fire_sticky_fuses (*this);
				return ACTION_STOP;
			}
			break;
		case ACTION_CONTINUE:
			/* Keep going - no action */
			break;
		case ACTION_RESTART:
			/** Fix me **/
			call_stack.top ()->fire_sticky_fuses (*this);
			delete call_stack.top ();
			call_stack.pop ();
			return (ACTION_RESTART);
		case ACTION_UNSET:
			// When do we get this?
			log_bug("WARNING: Command_action::step is in state ACTION_UNSET");
	}

	/* If we get here, there's more to do, so request that we be stepped again */
	return ACTION_CONTINUE;
}

/************************************************************************/
/*									*/
/*				String_pair				*/
/*									*/
/************************************************************************/

String_pair::String_pair (
const	String& n,
const	String& v)
: m_name (n)
, m_value (v)

{
}


String_pair::~String_pair ()

{
}


void
String_pair::set_value(
const	String& v)

{
	m_value = v;
}


/************************************************************************/
/*									*/
/*				If_scope				*/
/*									*/
/************************************************************************/

If_scope::If_scope (
const	Scope	&os,
	bool	i)
: Scope (os)
, if_ok (i)
, stop_line ((db[get_command()].get_parse_helper(current_line)) & 0xff)
, endif_line((db[get_command()].get_parse_helper(current_line)) >> 8)
{
//	dbref	command = get_command ();
//	unsigned short	temp = db [command].get_parse_helper (current_line);

//	stop_line = temp & 0xff;
//	endif_line = temp >> 8;

	/* Flip to the elseif / endif if required */
	if (!if_ok)
	{
		current_line = stop_line;
		stop_line = -1;
	}
	else
		current_line++;
		// Only increment line number if not doing @else[if]
}


void
If_scope::do_at_elseif (
const	bool	ok)

{
	if_ok = ok;
}


const int
If_scope::line_for_outer_scope ()
const

{
	return endif_line + 1;
}


Command_action
If_scope::step_once (
context	*c)

{
	char command_block[MAX_COMMAND_LEN];
	dbref	command = get_command ();

	/* We know only that we are stepping.  If we know a real stop line, it's a normal step. */
	if (current_line < stop_line)
	{
		/* current_line incremented here in case the command modifies it */
		current_line+=db[command].reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, current_line);
#ifdef	DEBUG
		log_debug("if: currentline: %s", command_block);
#endif	/* DEBUG */
		c->process_basic_command (command_block);
		return ACTION_CONTINUE;
	}

	/* If if_ok is set, we've finished executing the only block, and can zap straight to the endif */
	if (if_ok)
		return ACTION_STOP;

	/* If our stop line is -1, we are evaluating an elseif, an else or have reached the endif without executing anything */
	if (stop_line == -1)
	{
		/* Check for the endif */
		if (current_line >= endif_line)
			return ACTION_STOP;

		/* OK, we're evaluating an elseif / else */
		db[command].reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, current_line);
#ifdef	DEBUG
		log_debug("if: currentline: %s", command_block);
#endif	/* DEBUG */
		c->process_basic_command (command_block);
		/* do_at_elseif will have been called by the elseif / else */
		if (if_ok)
		{
			/* We're doing this block */
			stop_line = db [command].get_parse_helper (current_line) & 0xff;
			current_line++;
			return ACTION_CONTINUE;
		}
		else
		{
			/* Skip to the next block, which may be the endif */
			current_line = db [command].get_parse_helper (current_line) & 0xff;

			/* Check for the endif */
			if (current_line >= endif_line)
				return ACTION_STOP;

			/* Looks like we're just gonna have to keep hunting */
			if(current_line > 0)
			{
				stop_line = -1;
				return ACTION_CONTINUE;
			}
			/* Fallthrough line 0 doesn't exist */
		}
	}

	/* If we get here, something *really* odd has happened */
	log_bug ("Weird state in If_scope::step_once. Ditching block");
	return ACTION_STOP;
}


/************************************************************************/
/*									*/
/*				Loop					*/
/*									*/
/************************************************************************/

Loop::Loop (
const	Scope	&os)
: Scope (os)
, start_line (os.line_for_inner_scope ())
, end_line (db [get_command ()].get_parse_helper (start_line))
{

	/* We need to skip our start line as it's not useful */
	current_line++;
}


/*
 * line_for_outer_scope: What line should our outer scope restart on?  In
 *	this case, it's one past our @end.
 */

const int
Loop::line_for_outer_scope ()
const

{
	return end_line + 1;
}


Command_action
Loop::step_once (
context	*c)

{
	char command_block[MAX_COMMAND_LEN];

	/* Should we run now? */
	if(!shouldrun())
	{
		return ACTION_STOP;
	}

	/* Go round again? */
	c->command_executed();
	if (current_line >= end_line)
	{
		if (!loopagain ())
			return ACTION_STOP;
		current_line = start_line + 1;
	}

	/* Empty loop? - if so, don't bother evaluating it */
	if (current_line >= end_line)
		return ACTION_STOP;

	/* It's a normal line */
	current_line+=db[get_command()].reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, current_line);
#ifdef	DEBUG
	log_debug("loop: currentline: %s", command_block);
#endif	/* DEBUG */
	c->process_basic_command (command_block);
	return ACTION_CONTINUE;
}


With_loop::With_loop (
const	Scope	&os,
	dbref	d,
const	char	*index_name,
const	char	*element_name)
: Loop (os)
, elements()
, pos(elements.end())
, index (addarg (index_name, NULLSTRING))
, element (addarg (element_name, NULLSTRING))

{
	int total = db[d].get_number_of_elements();
	int i;
	switch(Typeof(d))
	{
	case TYPE_DICTIONARY:
		for(i = 1; i <= total; i++)
		{
			String_pair entry(db[d].get_index(1), db[d].get_element(i));
			elements.push_back(entry);
		}
		break;
	case TYPE_ARRAY:
		for(i = 1; i <= total; i++)
		{
			char number[20];
			sprintf(number, "%d", i);
			String_pair entry(number, db[d].get_element(i));
			elements.push_back(entry);
		}
		break;
	}

	pos = elements.begin();

	if(pos != elements.end())
	{
		index->set_value(pos->name());
		element->set_value(pos->value());
	}
}

bool
With_loop::shouldrun () const
{
	return (pos != elements.end());
}

/*
 * loopagain: Check the next element and decide whether we need another
 *	iteration of the loop.  Return whether we should loop again, and
 *	set up the loop for the next iteration if this is the case.
 */

bool
With_loop::loopagain ()

{
	// Increment the counter *BEFORE* we check if we should run again.
	pos++;

	if(!shouldrun())
		return false;


	index->set_value(pos->name());
	element->set_value(pos->value());

	return true;
}


For_loop::For_loop (
const	Scope	&os,
	int	in_start,
	int	in_end,
	int	in_step,
const	String&name)
: Loop (os)
, start (in_start)
, end (in_end)
, step (in_step)
, argument(0)
{
	char	workspace [11];

	sprintf (workspace, "%d", start);
	argument = addarg (name, workspace);
}

bool
For_loop::shouldrun () const
{
	if(step > 0)
		return start <= end;
	return start >= end;
}

bool
For_loop::loopagain ()

{
	char	workspace [11];

	start += step;
	sprintf (workspace, "%d", start);
	argument->set_value (workspace);

	return shouldrun();
}


/************************************************************************/
/*									*/
/*				Scheduler				*/
/*									*/
/************************************************************************/

/*
 * step: Do one step of a command and come back out.  PJC 27/12/96.
 *
 * Return value:
 *	0	Nothing unusual happened.
 *	non-0	The returned context finished.  Hopefully the caller will
 *		know what to do with it, because we don't know whether or
 *		not to delete it.
 */

context *
Scheduler::step ()

{
	context	*returned;

#ifdef	DEBUG
	if (!runnable ())
	{
		log_bug ("Scheduler can't step when it's not runnable");
		return 0;
	}
#endif	/* DEBUG */

	/* What's the highest runnable context (ie. one with no outstanding dependencies)? */
#ifdef	DEBUG
	/* If we're running single-user, the top-most context *must* be able to run */
	if (contexts.top ()->get_dependency ())
	{
		log_bug ("Top of run queue has a dependency");
		/* Should really do something sensible here, like grab a lower context */
		return 0;
	}
#endif	/* DEBUG */

	/* In a multiprocessing scheduler, this is where the job scheduling goes.  Here, it's trivial */
	context	*run_context = contexts.top ();

	/* OK, we have a context to run.  Step it. */
	switch (run_context->step ())
	{
		case ACTION_HALT:
			log_bug("context returned a halt request to the scheduler. (Ignore the next 2 BUGs)");
			/* FALLTHROUGH - Ignore the next error*/
		case ACTION_RESTART:
			log_bug("context returned a restart request to the scheduler");
			/* Treat a restart as a stop */
			/* FALLTHROUGH */
		case ACTION_STOP:
			returned = contexts.top ();
			contexts.pop ();
			returned->scheduled (false);
			return returned;
		case ACTION_UNSET:
			// When do we get this?
			log_bug("WARNING: Scheduler::step is in state ACTION_UNSET");
			break;
		case ACTION_CONTINUE:
			/* Do nothing */
			;
	}

	/* All updates performed, next step ready to go, all pigs fed and ready to fly. */
	return 0;
}


/**
 * This job must be evaluated at a higher priority than all
 *	non-express jobs and all previous express jobs.  It must then be executed
 *	until it is complete.
 */

context *
Scheduler::push_new_express_job (
context	*c)

{
	// If the job is scheduled, scream
	if (c->scheduled ())
		log_bug("Scheduler::push_new_express_job: Job is already scheduled (and Peter doesn't think it should be)");

	context	*returned;

	// Push it on the queue at the top
	size_t	old_depth = contexts.size ();
	c->scheduled (true);
	contexts.push (c);
	/* Now keep going while the queue is deeper */
// HACK: We might hit a loop here...
static int loop = 1;
int old_loop = loop;

	while ((contexts.size () > old_depth) && (loop++ < 1500))
		returned = step ();

	while (contexts.size() > old_depth)
	{
		returned = contexts.top();
		contexts.pop();
	}

	loop = old_loop; // We can safely reset it here, because we'll pop the jobs that cause the problem.
		// I hope!

	/* The last step that popped the queue must have returned something useful */
	return returned;
}


// I don't think this is used - notably, I don't think any express job has ever
// been pushed as a non-express job.  Hence removed, with a check in push_new_express_job.
#if 0
/**
 * This job must be evaluated at a higher priority than all
 *	non-express jobs and all previous express jobs.  It must then be executed
 *	until it is complete.
 *
 * An express job may previously have been pushed as a non-express job.
 */

context *
Scheduler::push_express_job (
context	*c)

{
	/* If the job is scheduled, zap it out of the queue */
	if (c->scheduled ())
		contexts.remove (c);

	return push_new_express_job (c);
}
#endif

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


#define	ASSIGN_STRING(d,s)	{ if (d) free (d);  if ((s) && *(s)) (d)=strdup(s); else (d)=NULL; }


/* The only one there is */
Scheduler	mud_scheduler;


/************************************************************************/
/*									*/
/*				Scope					*/
/*									*/
/************************************************************************/

Scope::Scope (
const	Scope	&os)
: outer_scope (&os)

{
	if (&os)
		current_line = outer_scope->line_for_inner_scope ();
	else
		current_line = 1;
}


Variable_stack::~Variable_stack()
{
	/* Lose any local variables */
	while (!variable_stack.is_empty ())
		delete variable_stack.pop ();
}

String_pair *
Variable_stack::addarg (
const	char	*n,
const	char	*v)

{
	variable_stack.push (new String_pair (n, v));
	return variable_stack.top ();
}


String_pair *
Variable_stack::check_and_add_arg (
const	char	*n,
const	char	*v)

{
	if (locatearg (n))
		return 0;
	else
		return addarg (n, v);
}


Boolean
Variable_stack::updatearg (
const	char	*name,
const	char	*val)

{
	String_pair	*temp;

	if (!(temp = locatearg (name))) /* This shouldn't happen! */
	{
		Trace("BUG: Trying to find non-existent argument %s.\n", name);
		return False;
	}
	temp->set_value (val);
	return True;
}


String_pair *
Variable_stack::locatearg (
const	char	*name)
const

{
	for (String_pair_iterator i (variable_stack); !i.finished (); i.step ())
	{
		if (!strcasecmp (i.current ()->get_name().c_str(), name))
			return i.current ();
	}

	/* If we get here, we didn't find it on this stack*/
	return 0;
}

String_pair *
Scope::locate_stack_arg (
const char *name)
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
const	Boolean	ok)

{
	fputs ("BUG: Scope::do_at_elseif called.\n", stderr);
}


/************************************************************************/
/*									*/
/*			Command_and_arguments				*/
/*									*/
/************************************************************************/

Command_and_arguments::Command_and_arguments (
const	char	*sc,
const	char	*a1,
const	char	*a2,
	Matcher	*m)
: matcher ((Matcher *)NULL)
, sticky_fuses (0)
, simple_command (alloc_string (a1))
, arg1 (alloc_string (a1))
, arg2 (alloc_string (a2))
{
	set_matcher (m);
}


Command_and_arguments::~Command_and_arguments ()

{
	ASSIGN_STRING (const_cast <char *> (simple_command), (const char *) 0);
	ASSIGN_STRING (const_cast <char *> (arg1), (const char *) 0);
	ASSIGN_STRING (const_cast <char *> (arg2), (const char *) 0);
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
const	char	*s)

{
	ASSIGN_STRING (const_cast <char *> (simple_command), s);
}


void
Command_and_arguments::set_arg1 (
const	char	*s)

{
	ASSIGN_STRING (const_cast <char *> (arg1), s);
}


void
Command_and_arguments::set_arg2 (
const	char	*s)

{
	ASSIGN_STRING (const_cast <char *> (arg2), s);
}


void
Command_and_arguments::pend_fuse (
dbref		fuse,
Boolean		success,
const	char	*simple_command,
const	char	*arg1,
const	char	*arg2,
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
const	char	*sc,
const	char	*a1,
const	char	*a2,
const	dbref	eid,
	Matcher	*m)
: Command_and_arguments (sc, a1, a2, m)
, Scope (*(Scope *)0)
, effective_id (eid)
, csucc_cache (NOTHING)
, cfail_cache (NOTHING)
, scope_stack (MAX_NESTING)

{
	const	char	*err;

	if ((err = (set_command (cmd))) != 0)
	{
		notify(c->get_player(), "%s %s", err, unparse_object(*c, cmd));
		set_command (NOTHING);
	}
}


Compound_command_and_arguments::~Compound_command_and_arguments ()

{
	/* Delete any active scopes (may be left after recursion) */

	while (!scope_stack.is_empty())
		delete scope_stack.pop ();
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
const	char	*name)
const

{
	if (scope_stack.is_empty ())
		return locatearg (name);
	else
		return scope_stack.top ()->locate_stack_arg (name);
}


const Scope &
Compound_command_and_arguments::innermost_scope ()
const

{
	if (scope_stack.is_empty ())
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
		while (!scope_stack.is_empty())
			delete scope_stack.pop();
		context->log_recursion (command, get_arg1 (), get_arg2 ());
		return ACTION_HALT;
	}

	/* Check for a parse error */
	if (command == NOTHING)
	{
		while (!scope_stack.is_empty())
			delete scope_stack.pop();
		notify_colour (context->get_player (), context->get_player (), COLOUR_ERROR_MESSAGES, "Parse failure in command");
		return ACTION_STOP;
	}

	/* Check to see if the command has been destroyed. If it has been 'replaced' then assume the replacer knows what they're doing. */
	if(Typeof(command) != TYPE_COMMAND)
	{
		current_line = -1;
	}
	/* Check for a return from the chain */
	if ((current_line != -1) && (current_line <= db[command].get_inherited_number_of_elements()))
	{
		if (action==ACTION_UNSET)
			action= (scope_stack.is_empty() ? step_once (context) :  scope_stack.top ()->step_once (context));
		switch (action)
		{
			case ACTION_HALT:
				/* This should never happen.. Deal with it anyway */
				Trace( "BUG: ACTION_HALT returned to Compound_command_and_arguments::step()\n");
				while (!scope_stack.is_empty())
					delete scope_stack.pop();
				return ACTION_HALT;
	
			case ACTION_UNSET:
				Trace( "BUG: ACTION_UNSET returned to Compound_command_and_arguments::step()\n");
				/* Fallthrough */

			case ACTION_STOP:
				/* The scope doesn't want to step again */
				if (!scope_stack.is_empty ())
				{
					/* Let the next outermost scope do its stuff */
					int	next_line = scope_stack.top ()->line_for_outer_scope ();
					delete scope_stack.pop ();
					if (scope_stack.is_empty ())
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
				while (!scope_stack.is_empty ())
					delete scope_stack.pop ();
				break;
		}
	}
	else
	{
		/*
		 * Set up next command. First remove any cruft from this one
		 *	(there might be some left if we've @returned)
		 */
		while (!scope_stack.is_empty())
			delete scope_stack.pop();

		dbref	player = context->get_player ();
		context->command_executed();

#ifdef LOG_COMMANDS
		Trace( "COMPOUND: %s(%d)\n", value_or_empty (db [get_current_command ()].get_name ()), get_current_command ());
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
						pronoun_substitute (scratch_buffer, BUFFER_LEN, player, db[command].get_inherited_ofail ().c_str());
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
						pronoun_substitute(scratch_buffer, BUFFER_LEN, player, db[command].get_inherited_osuccess ().c_str());
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
				Trace( "BUG: Player %s(%d) ran command %s(%d) which returned invalid boolean.\n",
					getname (player), player,
					getname (command), command);
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
const	Boolean	if_ok)

{
#ifdef	DEBUG
	if (scope_stack.is_empty ())
	{
		fputs ("BUG: Compound command do_at_elseif called with no if scope active.\n", stderr);
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

context::context (
dbref	new_player,
context &your_maker)
: Command_and_arguments (0, 0, 0, 0)
, player (new_player)
, trace_command (NOTHING)
, unchpid_id (new_player)
, call_stack (MAX_COMMAND_DEPTH)
, commands_executed (0)
, sneaky_executed_depth (0)
, step_limit (COMPOUND_COMMAND_BASE_LIMIT)
, depth_limit (10)
, return_string (unset_return_string)
/* Initialisation changed to SUCC from INIT by PJC 14/1/96 */
, return_status (COMMAND_SUCC)
, called_from_command (False)
, creator (your_maker)
, scheduled (False)
, dependency (0)

{
	set_effective_id(new_player);
}


context::~context ()

{
	set_return_string (0);
}


Boolean
context::allow_another_step ()

{
	if (commands_executed > step_limit)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Too many steps in command");
		return False;
	}

	return True;
}


void
context::copy_returns_from (
const	context	&source)

{
	return_status = source.return_status;
	set_return_string (source.return_string);
}


const Boolean
context::in_command ()
const

{
/*
 * We used to check commands_executed here, but it screws up brace_expand
 * when used on a compound command at the top level.
 *
 * AJS, 4 Feb 2000
 */
	return (!call_stack.is_empty () || called_from_command || (get_current_command() != NOTHING));
}


const char *
context::get_innermost_arg1 ()
const

{
	if (call_stack.is_empty ())
		return get_arg1 ();
	else
		return call_stack.top ()->get_arg1 ();
}


const char *
context::get_innermost_arg2 ()
const

{
	if (call_stack.is_empty ())
		return get_arg2 ();
	else
		return call_stack.top ()->get_arg2 ();
}


const dbref
context::get_current_command ()
const

{
	if (call_stack.is_empty ())
		return (NOTHING);
	else
		return (call_stack.top ()->get_command ());
}


const Boolean
context::set_effective_id (
dbref	id)

{
	if (call_stack.is_empty ())
		return (False);
	else
	{
		call_stack.top ()->set_effective_id (id);
		return (True);
	}
}


const dbref
context::get_effective_id ()
const

{
	if (call_stack.is_empty ())
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
const	char	*name)
const

{
	if (call_stack.is_empty ())
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
	if (call_stack.is_empty ())
	{
		fire_sticky_fuses (*this);
		return ACTION_STOP;
	}

	/* Do one thing in the command */
	switch (call_stack.top ()->step (this))
	{
		case ACTION_HALT:
			/* IE Recursion */
			if (!call_stack.is_empty())
				call_stack.top ()->fire_sticky_fuses (*this);
			while (!call_stack.is_empty())
				delete call_stack.pop();
			fire_sticky_fuses(*this);
			return_status=COMMAND_HALT;
			return ACTION_STOP;
		case ACTION_STOP:
			/* The command chain has come to its end */
			if (!call_stack.is_empty())
			{
				call_stack.top ()->fire_sticky_fuses (*this);
				delete call_stack.pop ();
			}
			else
				Trace( "BUG: Empty call stack when ACTION_STOP returned to context::step()\n");

			/* If that's all we've got to do, we've finished */
			if (call_stack.is_empty ())
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
			delete call_stack.pop ();
			return (ACTION_RESTART);
		case ACTION_UNSET:
			// When do we get this?
			Trace("WARNING: Command_action::step is in state ACTION_UNSET.\n");
	}

	/* If we get here, there's more to do, so request that we be stepped again */
	return ACTION_CONTINUE;
}


void
context::maybe_free (
const	char	*s)
const

{
	if (s != 0
		&& s != empty_string
		&& s != unset_return_string
		&& s != ok_return_string
		&& s != error_return_string)
		free (const_cast <char *> (s));
}


void
context::set_return_string (
const	char	*rs)

{
	/* Ditch the old one if we don't know what it is */
	if (return_string != 0
		&& return_string != empty_string
		&& return_string != unset_return_string
		&& return_string != ok_return_string
		&& return_string != error_return_string)
	{
		free (const_cast <char *> (return_string));
		return_string = 0;
	}

	/* Assign or copy the new one.  Remember blank strings as empty so strcat et al don't barf in the rest of the code. */
	if (!rs)
		return_string = empty_string;
	else if (rs != unset_return_string
		&& rs != ok_return_string
		&& rs != error_return_string)
		return_string=strdup(rs); 
	else
		return_string = rs;
}


/************************************************************************/
/*									*/
/*				String_pair				*/
/*									*/
/************************************************************************/

String_pair::String_pair (
const	char	*n,
const	char	*v)
: name (n)
, value (v)

{
}


String_pair::~String_pair ()

{
}


void
String_pair::set_value(
const	String& v)

{
	value = v;
}


/************************************************************************/
/*									*/
/*				If_scope				*/
/*									*/
/************************************************************************/

If_scope::If_scope (
const	Scope	&os,
	Boolean	i)
: Scope (os)
, if_ok (i)

{
	dbref	command = get_command ();
	unsigned short	temp = db [command].get_parse_helper (current_line);

	stop_line = temp & 0xff;
	endif_line = temp >> 8;

	/* Set up backwards-compatibility if required */
	//no_endif = Backwards (command);

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
const	Boolean	ok)

{
	if_ok = ok;
}


const int
If_scope::line_for_outer_scope ()
const

{
	/* For backwards compatibility: there may not always be an endif line */
	//return endif_line + (no_endif ? 0 : 1);
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
		Trace("If: CurrentLine %s\n", command_block);
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
		Trace("If: CurrentLine %s\n", command_block);
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
	fputs ("BUG: Weird state in If_scope::step_once. Ditching block.\n", stderr);
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

{
	end_line = db [get_command ()].get_parse_helper (start_line);

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
	Trace("Loop: CurrentLine %s\n", command_block);
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
, counter (1)
, dict (d)
, index (addarg (index_name, 0))
, element (addarg (element_name, 0))

{
	/* Make safe for immediate return if the collection is empty */
	if (db [dict].get_number_of_elements () > 0)
	{
		switch (Typeof (dict))
		{
			case TYPE_DICTIONARY:
				index->set_value (db [dict].get_index (1));
				break;
			case TYPE_ARRAY:
				index->set_value ("1");
				break;
			default:
				/* Should never happen */
				;
		}
		element->set_value (db [dict].get_element (1));
	}
}


/*
 * loopagain: Check the next element and decide whether we need another
 *	iteration of the loop.  Return whether we should loop again, and
 *	set up the loop for the next iteration if this is the case.
 */

Boolean
With_loop::loopagain ()

{
	if (Typeof(dict) == TYPE_FREE)
		return False;

	if (++counter > db[dict].get_number_of_elements ())
		return False;

	switch (Typeof(dict))
	{
		case TYPE_ARRAY:
			{
				char	workspace [11];
				sprintf (workspace, "%d", counter);
				index->set_value (workspace);
			}
			break;
		case TYPE_DICTIONARY:
			index->set_value (db[dict].get_index(counter));
			break;
		default:
			/* Probably means that the collection has been destroyed */
			return False;
	}
	element->set_value (db[dict].get_element(counter));
	return True;
}


For_loop::For_loop (
const	Scope	&os,
	int	in_start,
	int	in_end,
	int	in_step,
const	char	*name)
: Loop (os)
, start (in_start)
, end (in_end)
, step (in_step)

{
	char	workspace [11];

	sprintf (workspace, "%d", start);
	argument = addarg (name, workspace);
}


Boolean
For_loop::loopagain ()

{
	char	workspace [11];

	start += step;
	sprintf (workspace, "%d", start);
	argument->set_value (workspace);

	if (step > 0)
		return start <= end;
	else
		return start >= end;
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
		fputs ("BUG: Scheduler can't step when it's not runnable\n", stderr);
		return 0;
	}
#endif	/* DEBUG */

	/* What's the highest runnable context (ie. one with no outstanding dependencies)? */
#ifdef	DEBUG
	/* If we're running single-user, the top-most context *must* be able to run */
	if (contexts.top ()->get_dependency ())
	{
		fputs ("BUG: Top of run queue has a dependency.\n", stderr);
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
			fputs("BUG: context returned a halt request to the scheduler. (Ignore the next 2 BUGs)\n", stderr);
			/* FALLTHROUGH - Ignore the next error*/
		case ACTION_RESTART:
			fputs ("BUG: context returned a restart request to the scheduler.\n", stderr);
			/* Treat a restart as a stop */
			/* FALLTHROUGH */
		case ACTION_STOP:
			returned = contexts.pop ();
			returned->set_scheduled (False);
			return returned;
		case ACTION_UNSET:
			// When do we get this?
			Trace("WARNING: Scheduler::step is in state ACTION_UNSET.\n");
			break;
		case ACTION_CONTINUE:
			/* Do nothing */
			;
	}

	/* All updates performed, next step ready to go, all pigs fed and ready to fly. */
	return 0;
}


/*
 * push_express_job: This job must be evaluated at a higher priority than all
 *	non-express jobs and all previous express jobs.  It must then be executed
 *	until it is complete.
 *
 * An express job may previously have been pushed as a non-express job.
 */

context *
Scheduler::push_express_job (
context	*c)

{
	context	*returned;

	/* If the job is scheduled, zap it out of the queue */
	if (c->get_scheduled ())
		contexts.remove (c);

	/* Push it on the queue at the top, just to make sure */
	int	old_depth = contexts.get_depth ();
	c->set_scheduled (True);
	contexts.push (c);
	/* Now keep going while the queue is deeper */
// HACK: We might hit a loop here...
int loop = 0;
	while ((contexts.get_depth () > old_depth) && (loop++ < 1500))
		returned = step ();

while(contexts.get_depth() > old_depth)
{
	returned = contexts.pop();
}


	/* The last step that popped the queue must have returned something useful */
	return returned;
}

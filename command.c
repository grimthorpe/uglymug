/* static char SCCSid[] = "@(#)command.c	1.30\t9/25/95"; */
/*
 * command.c: File for compound command implementation.
 *
 *	Peter Crowther, 31/8/90.
 *	Keith 'Pointer' Garrett, 23/10/93
 *	Ian 'Sarcastic' Whalley, 23/10/93 (bit later).
 *	Duncan 'Crap' Millard, 16/8/95 (Loop the loop)
 *	Keith 'To fight a superior enemy for a rightous cause' Garrett (Loop in my Loop)
 *	Peter 'Maybe I should think of a good quote' Crowther, 23/12/96.
 *	Removed warnings.  Rewrote to add scopes, to make nested compound
 *	command execution that bit cleaner.  General code tidy-up.
 *	Prepared code for multi-tasking by forcing single-step execution
 *	on everything except the Scheduler.
 */

#include <time.h>
#include <string.h>
#include <ctype.h>

#include "interface.h"
#include "command.h"
#include "config.h"
#include "externs.h"
#include "match.h"
#include "context.h"
#include "colour.h"

#define	ASSIGN_STRING(d,s)	{ if ((d)) free((d)); if ((s && *s!=0)) (d)=strdup((s)); else (d)=NULL;}
#define SKIP_SPACES(x)		while (*(x) && isspace(*(x))) (x)++;
#define SKIP_DIGITS(x)		while (*(x) && isdigit(*(x))) (x)++;


const	char	empty_string		[]	= "";
const	char	ok_return_string	[]	= "OK";
const	char	error_return_string	[]	= "Error";
const	char	recursion_return_string	[]	= "Recursion.";
const	char	unset_return_string	[]	= "Unset_return_value.";
const	char	permission_denied	[]	= "Permission denied.";
	char	scratch_return_string	[BUFFER_LEN];
	char	scratch_buffer		[2 * BUFFER_LEN];


Boolean
context::can_do_compound_command (
const	char	*command_string,
const	char	*arg1,
const	char	*arg2)

{
	dbref	command;

	/* Match commands */
	Matcher matcher (player, command_string, TYPE_COMMAND, get_effective_id ());
	matcher.check_keys ();
	matcher.match_command ();
	matcher.match_absolute ();

	/* Did we find one? */
	if ((command = matcher.match_result ()) == NOTHING)
		return (False);

	/* Check ambiguity */
	if (command == AMBIGUOUS)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Ambiguous command.");
		return (False);
	}

	/* Cannot directly do dark commands - if we find one, keep hunting. */
	do
	{
		if (!Dark (command))
		{
			if (could_doit (*this, command))
				do_compound_command (command, command_string, arg1, arg2, NOTHING, matcher);
			/* Whether it worked or not, we've tried our best */
			return True;
		}
		matcher.match_restart ();
	} while ((command = matcher.match_result ()) != NOTHING);

	/* If we get here, we've not found a command, but we keep quiet about it */
	return True;
}


Boolean
context::can_override_command (
const	char	*command_string,
const	char	*arg1,
const	char	*arg2)
{
	dbref command;

	Matcher matcher (player, command_string, TYPE_COMMAND, get_effective_id ());
	matcher.check_keys ();
	matcher.match_bounded_area_command (COMMAND_LAST_RESORT, COMMAND_LAST_RESORT);
	matcher.match_absolute ();

	if ((command = matcher.match_result ()) == NOTHING)
		return (False);

	if (command == AMBIGUOUS)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Ambiguous command.");
		Trace( "BUG: Ambiguous command (%s) in #%d\n", command_string, COMMAND_LAST_RESORT);
		notify_wizard_unconditional ("Error: There's an ambiguous match on '%s' in #%d!", command_string, COMMAND_LAST_RESORT);
		return (False);
	}

	do {
		if (!Dark (command) && Wizard (command))
			if (could_doit (*this, command))
			{
				do_compound_command (command, command_string, arg1, arg2, NOTHING, matcher);
				return (True);
			}
		matcher.match_restart ();
	} while ((command = matcher.match_result ()) != NOTHING);

	return (False);
}


/*
 * This code is called to start a new compound command.  This context may or may
 *	not be a new context.  We get the command ready for starting, but it's
 *	up to the step() method to do the execution itself - this is required
 *	for scheduling.  We don't call step() at all; this separation of setup
 *	and run should make user-level debugging calls dead easy, as we
 *	now have a separate step (this one) for each function call.
 */

Command_action
context::do_compound_command (
dbref		command,
const	char	*sc,
const	char	*a1,
const	char	*a2,
dbref		eid,
Matcher		&matcher)
{
	/* If we're handed an illegal command, give up now */
	if (command < 0 || command >= db.get_top () || (Typeof(command) != TYPE_COMMAND))
		return ACTION_STOP;
	/* Push stack, with current EID if we already happen to be nested */
	if ((call_stack.depth () >= depth_limit) || (!call_stack.push (new Compound_command_and_arguments (command, this, sc, a1, a2, eid == NOTHING ? get_effective_id () : eid, &matcher))))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Recursion in command");
		log_recursion (command, a1, a2);
		return_status= COMMAND_HALT;
		return ACTION_HALT;
	}

	/* Schedule ourselves */
	if (!scheduled)
		mud_scheduler.push_job (this);
	return ACTION_CONTINUE;
}


/*
 * parse_command: A cheap'n'cheerful recursive descent parser for control
 *	structures within commands that fills in the parse_helper array.
 *
 * Return value:
 *	The line number of the line beyond the one it finishes at.
 *
 * errs must be an externally allocated character buffer.  It is filled in
 *	with any error string.
 */

const int
Compound_command_and_arguments::parse_command (
	object	*cmd,
const	int	start_line,
	char	*errs)

{
	char	command_block[MAX_COMMAND_LEN];
		int		lines_in_block;
		int		line = start_line;
		int		end_line = cmd->get_inherited_number_of_elements ();

	while (line > 0 && line <= end_line)
	{
		lines_in_block=cmd->reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, line);
#ifdef	DEBUG
		Trace("Compound_command parse %d: %s\n", line, command_block);
#endif	/* DEBUG */

		switch (what_is_next (command_block))
		{
			case NORMAL_NEXT:
				break;
			case ELSEIF_NEXT:
			case ELSE_NEXT:
			case ENDIF_NEXT:
				sprintf (errs, "Missing @if at line %d in command", line);
				line = -1;
				break;
			case END_NEXT:
				sprintf (errs, "Missing @for or @with at line %d in command", line);
				line = -1;
				break;
			case IF_NEXT:
				line = If_scope::parse_command (cmd, line, errs);
				break;
			case FOR_NEXT:
				line = For_loop::parse_command (cmd, line, errs);
				break;
			case WITH_NEXT:
				line = With_loop::parse_command (cmd, line, errs);
				break;
			default:
				fputs ("BUG: Unknown next in parse_command\n", stderr);
				line = -1;
		}
		if (line != -1)
			line+=lines_in_block;
	}

	/* Either we finished or there was a parse error (ie. -1 in line) */
	return line;
}


const int
For_loop::parse_command (
	object	*cmd,
const	int	start_line,
	char	*errs)

{
	char command_block[MAX_COMMAND_LEN];
	int lines_in_block;
	int		line = start_line + 1;
	int		end_line = cmd->get_inherited_number_of_elements ();

	while (line > 0 && line <= end_line)
	{
		lines_in_block=cmd->reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, line);
#ifdef	DEBUG
		Trace("For_loop parse: %s\n", command_block);
#endif	/* DEBUG */

		switch (what_is_next (command_block))
		{
			case NORMAL_NEXT:
				break;
			case ELSEIF_NEXT:
			case ELSE_NEXT:
			case ENDIF_NEXT:
				sprintf (errs, "Missing @if at line %d in command", line);
				line = -1;
				break;
			case END_NEXT:
				/* Our start line needs to know where our end line is */
				cmd->set_parse_helper (start_line, line);
				return line;
			case IF_NEXT:
				line = If_scope::parse_command (cmd, line, errs);
				break;
			case FOR_NEXT:
				line = For_loop::parse_command (cmd, line, errs);
				break;
			case WITH_NEXT:
				line = With_loop::parse_command (cmd, line, errs);
				break;
			default:
				fputs ("BUG: Unknown next in parse_command\n", stderr);
				line = -1;
		}
		if (line != -1)
			line+=lines_in_block;
	}

	/* Either we fell off the command or there was a parse error (ie. -1 in line) */
	if (line != -1)
		sprintf (errs, "No @end to match the @for in %d in command", start_line);
	return -1;
}


const int
With_loop::parse_command (
	object	*cmd,
const	int	start_line,
	char	*errs)

{
	char command_block[MAX_COMMAND_LEN];
	int lines_in_block;
	int		line = start_line + 1;
	int		end_line = cmd->get_inherited_number_of_elements ();

	while (line > 0 && line <= end_line)
	{
		lines_in_block=cmd->reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, line);
#ifdef	DEBUG
		Trace("With_loop parse: %s\n", command_block);
#endif	/* DEBUG */

		switch (what_is_next (command_block))
		{
			case NORMAL_NEXT:
				break;
			case ELSEIF_NEXT:
			case ELSE_NEXT:
			case ENDIF_NEXT:
				sprintf (errs, "Missing @if at line %d in command", line);
				line = -1;
				break;
			case END_NEXT:
				/* Our start line needs to know where our end line is */
				cmd->set_parse_helper (start_line, line);
				return line;
			case IF_NEXT:
				line = If_scope::parse_command (cmd, line, errs);
				break;
			case FOR_NEXT:
				line = For_loop::parse_command (cmd, line, errs);
				break;
			case WITH_NEXT:
				line = With_loop::parse_command (cmd, line, errs);
				break;
			default:
				fputs ("BUG: Unknown next in parse_command\n", stderr);
				line = -1;
		}
		if (line != -1)
			line+=lines_in_block;
	}

	/* Either we fell off the command or there was a parse error (ie. -1 in line) */
	if (line != -1)
		sprintf (errs, "No @end to match the @with in %d in command", start_line);
	return -1;
}


const int
If_scope::parse_command (
	object	*cmd,
const	int	start_line,
	char	*errs)

{
		int		previous_block = start_line;
		char command_block[MAX_COMMAND_LEN];
		int lines_in_block;
		int		line = start_line + 1;
		int		end_line = cmd->get_inherited_number_of_elements ();
		int		force_end_line = 0;
		Boolean		force_endif = False;
		Boolean		backwards = cmd->get_flag(FLAG_BACKWARDS);
		Boolean		one_line_done = False;
		Command_next	temp_result;

	while (line > 0 && line <= end_line)
	{
		lines_in_block=cmd->reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, line);
#ifdef	DEBUG
		Trace("If_scope parse: %s\n", command_block);
#endif	/* DEBUG */

		temp_result=what_is_next(command_block);

		// Have we got a backwards command with no @else?

		if (backwards && 
		    ( (line == end_line) || 
		     ((temp_result != ELSE_NEXT) && (one_line_done==True))
		    )
		   )
			force_endif = True;

		/* This will force an exit */
		if (force_end_line == line)
			force_endif = True;

		switch (force_endif ? ENDIF_NEXT : temp_result)
		{
			case NORMAL_NEXT:
				if (backwards)
					one_line_done= True;
				break;
			case ELSEIF_NEXT:
				if (cmd->get_flag (FLAG_BACKWARDS))
				{
					sprintf (errs, "Line %d: Cannot use @elseif in a command with BACKWARDS set on it.", line);
					line = -1;
					break;
				}
				/* FALLTHROUGH */
			case ELSE_NEXT:
				cmd->set_parse_helper (previous_block, line);
				previous_block = line;
				if (backwards)
					force_end_line = line + 2;
				one_line_done=False;
				break;
			case ENDIF_NEXT:
				/* If force_endif is set, there's a fake endif inserted just here (and our return from here fixes it) */
				if (!force_endif && backwards)
				{
					sprintf (errs, "Line %d: Cannot use @endif in a command with BACKWARDS set on it.", line);
					line = -1;
					break;
				}
				// If we're here with force_endif set, it's because either
				// we've hit the end of the command (one_line_done will be false) or we've
				// got a 2nd line which is not an @if (o_l_d will be true). In the former case, store
				// line+1 to go to end of command.

				cmd->set_parse_helper (previous_block, line + ((force_endif && !one_line_done) ? 1:0));
				/* Our start line needs to know where our end line is */
				cmd->set_parse_helper (start_line, ( ( line + ((force_endif && !one_line_done)?1:0)) << 8) + cmd->get_parse_helper (start_line));
				return line - ((force_endif) ? 1: 0);
			case END_NEXT:
				sprintf (errs, "Missing @for or @with at line %d in command", line);
				line = -1;
				break;
			case IF_NEXT:
				line = If_scope::parse_command (cmd, line, errs);
				break;
			case FOR_NEXT:
				line = For_loop::parse_command (cmd, line, errs);
				break;
			case WITH_NEXT:
				line = With_loop::parse_command (cmd, line, errs);
				break;
			default:
				fputs ("BUG: Unknown next in parse_command\n", stderr);
				line = -1;
		}

		if (line != -1)
			line+=lines_in_block;
	}

	/* Either we fell off the command or there was a parse error (ie. -1 in line) */
	if (line != -1)
		sprintf (errs, "No @endif to match the @if in %d in command", start_line);
	return -1;
}


Command_next
what_is_next (
const	char	*text)

{
	char		temp_buffer[256];
	char		*p=temp_buffer;
	const char	*q=text;
	int		charssofar=0;
	
	while (q && (*q) && (*q !=' ') && (charssofar++ < 255))
		*p++=*q++;
	*p='\0';

	/* Swift hunt for the common case */
	if (!text || *text != '@')
		return NORMAL_NEXT;

	/* Damn, we've got to do some work */
	if(string_prefix("@if", temp_buffer))
		return IF_NEXT;
	else if(string_prefix("@endif", temp_buffer) && (!string_prefix("@end", temp_buffer)))
		return ENDIF_NEXT;
	else if(string_prefix("@end", temp_buffer))
		return END_NEXT;
	else if(string_prefix("@elseif", temp_buffer) && (!string_prefix("@else", temp_buffer)))
		return ELSEIF_NEXT;
	else if(string_prefix("@else", temp_buffer))
		return ELSE_NEXT;
	else if(string_prefix("@force", temp_buffer) && (!string_prefix("@for", temp_buffer)))
		return NORMAL_NEXT;
	else if(string_prefix("@for", temp_buffer))
		return FOR_NEXT;
	else if(string_prefix("@with", temp_buffer))
		return WITH_NEXT;
	else
		return NORMAL_NEXT;
}


/*
 * do_at_endif: If we ever try to execute an @endif directly, we've got problems.
 *	Within a command, it's parsed out, so we can only get here if a player
 *	has typed it directly on the command line.
 */

void
context::do_at_endif (
const	char	*,
const	char	*)

{
	notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@endif can only be used inside commands.");
	RETURN_FAIL;
}


void
context::do_chpid (
const	char	*,
const	char	*)

{
	if (call_stack.is_empty ())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@chpid can only be used inside commands.");
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
	}
	else
	{
		call_stack.top ()->chpid ();
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_unchpid (
const	char	*,
const	char	*)

{
	if (call_stack.is_empty ())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@unchpid can only be used inside commands.");
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
	}
	else
	{
		call_stack.top ()->set_effective_id (unchpid_id);
//		return_status = COMMAND_SUCC;
//		set_return_string (ok_return_string);
	}
}


void
context::do_at_false (
const	char	*,
const	char	*)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
}


void
context::do_at_true (
const	char	*,
const	char	*)

{
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_return (
const	char	*arg1,
const	char	*arg2)

{
	set_return_string (error_return_string);

	if (!in_command () || call_stack.is_empty ())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@return can only be used inside a command.");
		return_status = COMMAND_FAIL;
		return;
	}
	if (!arg1 || !arg2)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Usage: @return <true | false | current> = <return_string>.");
		return_status = COMMAND_FAIL;
		return;
	}

	if (!strcasecmp (arg1, "true") || !strcasecmp (arg1, "t"))
		return_status = COMMAND_SUCC;
	else if (!strcasecmp (arg1, "false") || !strcasecmp (arg1, "f"))
		return_status = COMMAND_FAIL;
	else if (!strcasecmp (arg1, "current") || !strcasecmp (arg1, "c"))
	{
		if (return_status != COMMAND_SUCC && return_status != COMMAND_FAIL)
		{
			return_status = COMMAND_SUCC;
			if (!gagged_command ())
				notify_colour (player, player, COLOUR_MESSAGES, "Warning: No previous state for @return current, assuming success.");
		}
	}
	else
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Invalid return state passed to @return.");
		return_status = COMMAND_FAIL;
		return;
	}

	/* Set up the second argument */
	set_return_string (arg2);
	call_stack.top ()->do_at_return (*this);
}


void
context::do_returnchain (
const	char	*arg1,
const	char	*arg2)

{
	set_return_string (error_return_string);

	if (!in_command () || call_stack.is_empty ())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@return can only be used inside a command.");
		return_status = COMMAND_FAIL;
		return;
	}

	/* Syntax check: We must have a first argument, and we must have a second unless the first is "home" */
	if ((!arg1) || ((!arg2) && (strcasecmp (arg1, "home"))))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Usage: @returnchain home | <true | false | current> = <return_string>.");
		return_status = COMMAND_FAIL;
		return;
	}

	if (!arg2)
	{
		/* We must have 'home' as the 1st arg, or we'd have failed the syntax check */
		return_status= COMMAND_EXITHOME;
	}
	else
	{
		/* Two arguments */
		if (!(strcasecmp (arg1, "true")) || !(strcasecmp (arg1, "t")))
			return_status = COMMAND_SUCC;
		else if (!strcasecmp (arg1, "false") || !strcasecmp (arg1, "f"))
			return_status = COMMAND_FAIL;
		else if (!strcasecmp (arg1, "current") || !strcasecmp (arg1, "c"))
		{
			/* @returnchain current; fine as long as the current is sensible */
			if (return_status != COMMAND_SUCC && return_status != COMMAND_FAIL)
			{
				return_status = COMMAND_SUCC;
				if (!gagged_command ())
					notify_colour (player, player, COLOUR_MESSAGES, "Warning: No previous state for @return current, assuming success.");
			}
		}
		else
		{
			/* Oops */
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Invalid return state passed to @return.");
			return_status = COMMAND_FAIL;
			return;
		}

		/* Set up the second argument */
		set_return_string (arg2);
	}
	call_stack.top ()->do_at_returnchain (*this);
}


void
context::do_at_if (
const	char	*arg1,
const	char	*)

{
	if ((!in_command()) || (call_stack.is_empty()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@if may only be used inside compound commands.");
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
		return;
	}

	/* From here on we'll always return some kind of result */
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);

	/* A few things fail, everything else succeeds */
	Boolean	if_ok = True;

	/* Check for the failure cases */
	if (arg1 && *arg1)
	{
		/* Strip leading spaces */
		while (isspace (*arg1))
			arg1++;

		/* Check for 0, Unset_return_value or Error */
		if_ok = string_compare ("0", arg1)
			&& string_compare (unset_return_string, arg1)
			&& string_compare (error_return_string, arg1);
		if (!if_ok)
		{
			return_status=COMMAND_FAIL;
			set_return_string(error_return_string);
		}
	}

	/* We've decided what to do, so... */
	call_stack.top ()->push_scope (new If_scope (call_stack.top ()->innermost_scope (), if_ok));
}


void
context::do_at_elseif (
const char *arg1,
const char *)

{
	if ((!in_command()) || (call_stack.is_empty()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@elseif may only be used in compound commands.");
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
		return;
	}

	/* From here on we'll always return some kind of result */
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);

	/* A few things fail, everything else succeeds */
	Boolean	if_ok = True;

	/* Check for the failure cases */
	if (arg1 && *arg1)
	{
		/* Strip leading spaces */
		while (isspace (*arg1))
			arg1++;

		/* Check for 0, Unset_return_value or Error */
		if_ok = string_compare ("0", arg1)
			&& string_compare (unset_return_string, arg1)
			&& string_compare (error_return_string, arg1);
	}

	/* We've decided what to do, so... */
	call_stack.top ()->do_at_elseif (if_ok);
}


void
context::do_at_else (
const	char	*,
const	char	*)

{
	if ((!in_command()) || (call_stack.is_empty()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@else may only be used in compound commands.");
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
		return;
	}

	/* An else is simply an elseif that succeeds */
	call_stack.top ()->do_at_elseif (True);
}


void
context::do_at_end(
const char *,
const char *)

{
	return_status= COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!in_command() || call_stack.is_empty())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only use @end in a command");
		return;
	}

	/* Should never get here */
	Trace( "BUG: @end processed by %s in a command with no scope (%s).\n", unparse_object (GOD_ID, player), unparse_object (GOD_ID, get_current_command ()));
}


void
context::do_at_with(
const char *what,
const char *args)

{
	char		arg1	[MAX_COMMAND_LEN];
	dbref		target;
	char		*p;

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!in_command() || call_stack.is_empty ())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with may only be used within compound commands.");
		return;
	}

	if (!(what && *what) || !(args && *args))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Usage: @with <array|dict> = <varname1>, <varname2>");
		return;
	}

	/* Check we know about the dictionary */
	Matcher dict (player, what, TYPE_DICTIONARY, get_effective_id());
	dict.match_everything();
	if ((target = dict.noisy_match_result()) == NOTHING)
		return;
	if ((Typeof (target) != TYPE_DICTIONARY) && (Typeof (target) != TYPE_ARRAY))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with can only be used with arrays or dictionaries.");
		return;
	}

	/* Grab the first argument (up to comma or end of string) */
	p = arg1;
	while (*args && ((*p++=*args++) != ','))
		;
	*--p = '\0';
	if (!ok_name(arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Bad name '%s' for first argument.", arg1);
		return;
	}

	SKIP_SPACES(args)
	if (!*args)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Missing second argument.");
		return;
	}

	if (!ok_name(args))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Bad name '%s' for second argument.", args);
		return;
	}

	/* Check we're safe */
	if (locate_innermost_arg (arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Variable name %s already in use.", arg1);
		return;
	}
	if (locate_innermost_arg (args))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Variable name %s already in use.", args);
		return;
	}

	if (!call_stack.top()->push_scope (new With_loop (call_stack.top ()->innermost_scope (), target, arg1, args)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unable to set up loop - see a wizard.");
		return;
	}
	return_status= COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_for(
const char *countername,
const char *args)

{
		int	start;
		int	end;
		int	step = 1;
	static	char	usage [] = "Usage: @for <variable>=<start> to <end> [step <interval>]";

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if ((!in_command()) || (call_stack.is_empty()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@for may only be used in compound commands.");
		return;
	}

	if (!ok_name(countername))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@for: Bad variable name.");
		return;
	}

	if ((!args) || (!*args))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, usage);
		return;
	}

	/* If we get to here, we've got a second argument.  Try some different legal parses... */
	if (sscanf (args, "%d%*[ ]to%*[ ]%d%*[ ]step%*[ ]%d", &start, &end, &step) != 3
		&& sscanf (args, "%dto%dstep%d", &start, &end, &step) != 3)
	{
		/* It wasn't the full 3-arg version.  Try 2-arg */
		if (sscanf (args, "%d%*[ ]to%*[ ]%d", &start, &end) != 2
			&& sscanf (args, "%dto%d", &start, &end) != 2)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, usage);
			return;
		}

		/* We got 2-args OK.  Set the step */
		step = (end >= start) ? 1 : -1;
	}

	if (step == 0 || step * (end - start) < 0)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@for: Step must move the start towards the end.");
		return;
	}

	if (locate_innermost_arg (countername))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@for: Variable name already in use.");
		return;
	}

	if (!call_stack.top()->push_scope (new For_loop (call_stack.top ()->innermost_scope (), start, end, step, countername)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unable to set up loop - see a wizard!");
		return;
	}

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}

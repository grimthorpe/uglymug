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
#include "log.h"


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


bool
context::can_do_compound_command (
const	CString& command_string,
const	CString& arg1,
const	CString& arg2)

{
	dbref	command;

#ifdef HACK_HUNTING
	// If we're inside a chpid, and not in a special command (fuse, .enter, .leave, alarm, etc.)
	if(get_effective_id() != get_player())
	{
		/* If we try to call a command that doesn't start
		 * with '#' (ie an absolute number, or relative to
		 * an absolute), then print a nasty warning.
		 * We'll let commands relative to 'here' through...
		 */
		if((command_string.c_str()[0] != '#')
			&& (strncasecmp(command_string.c_str(), "here:", 5) != 0))
		{
			notify_colour(get_effective_id(), get_effective_id(), COLOUR_ERROR_MESSAGES, "HACK: Command #%d has non-absolute command (%s) called while in @chpid.", get_current_command(), command_string.c_str());
			log_hack(get_effective_id(),
				"player %s(#%d): effective: %s(#%d): command %s(#%d) has non-absolute command '%s' called while in @chpid",
						getname(get_player()),
						get_player(),
						getname(get_effective_id()),
						get_effective_id(),
						getname(get_current_command()),
						get_current_command(),
						command_string.c_str()
			);
		}
	}
#endif /* HACK_HUNTING */
	/* Match commands */
	Matcher matcher (player, command_string, TYPE_COMMAND, get_effective_id ());
	matcher.check_keys ();
	matcher.match_command ();
	matcher.match_absolute ();

	/* Did we find one? */
	if ((command = matcher.match_result ()) == NOTHING)
		return (false);

	/* Check ambiguity */
	if (command == AMBIGUOUS)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Ambiguous command.");
		return (false);
	}

	/* Cannot directly do dark commands - if we find one, keep hunting. */
	do
	{
		if (!Dark (command))
		{
			if (could_doit (*this, command))
			{
				do_compound_command (command, command_string, arg1, arg2, NOTHING, matcher);
			}
			/* Whether it worked or not, we've tried out best */
			return true;
		}
		matcher.match_restart ();
	} while ((command = matcher.match_result ()) != NOTHING);

	/* If we get here, we've not found a command, but we keep quiet about it */
	/* AJS: Why do we keep quiet about it? DARK commands shouldn't affect
	 *	the game this way (we don't generate Huh?, things fail silently)
	 */
	//return true;
	return false;
}


bool
context::can_override_command (
const	CString& command_string,
const	CString& arg1,
const	CString& arg2)
{
	dbref command;

	Matcher matcher (player, command_string, TYPE_COMMAND, get_effective_id ());
	matcher.check_keys ();
	matcher.match_bounded_area_command (COMMAND_LAST_RESORT, COMMAND_LAST_RESORT);
	matcher.match_absolute ();

	if ((command = matcher.match_result ()) == NOTHING)
		return (false);

	if (command == AMBIGUOUS)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Ambiguous command.");
		log_bug("ambiguous command '%s' in #%d", command_string.c_str(), COMMAND_LAST_RESORT);
		notify_wizard ("Error: There's an ambiguous match on '%s' in #%d!", command_string.c_str(), COMMAND_LAST_RESORT);
		return (false);
	}

	do {
		if (!Dark (command) && Wizard (command))
			if (could_doit (*this, command))
			{
				do_compound_command (command, command_string, arg1, arg2, NOTHING, matcher);
				return (true);
			}
		matcher.match_restart ();
	} while ((command = matcher.match_result ()) != NOTHING);

	return (false);
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
const	CString& sc,
const	CString& a1,
const	CString& a2,
dbref		eid,
Matcher		&matcher)
{
	/* If we're handed an illegal command, give up now */
	if (command < 0 || command >= db.get_top () || (Typeof(command) != TYPE_COMMAND))
		return ACTION_STOP;
	/* Push stack, with current EID if we already happen to be nested */
	if ((call_stack.depth () >= depth_limit) || (!call_stack.push (new Compound_command_and_arguments (command, this, sc, a1, a2, eid == NOTHING ? get_effective_id () : eid, &matcher, gagged_command()))))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Recursion in command");
		log_recursion(player, db[player].get_name(), depth_limit, unparse_object (GOD_ID, command), reconstruct_message(get_arg1(), get_arg2()));
		return_status= COMMAND_HALT;
		return ACTION_HALT;
	}

	if(Backwards(command))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Attempt to run BACKWARDS command not supported any more.");
		log_bug("player %s(#%d) tried to execure BACKWARDS command %s(#%d)",
				getname(player),
				player,
				getname(command),
				command
		);
		return_status = COMMAND_HALT;
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
		log_debug("compound_command parse %d: %s", line, command_block);
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
				log_bug("Unknown next in parse_command");
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
		log_debug("for_loop parse: %s", command_block);
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
				log_bug(" Unknown next in parse_command");
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
		log_debug("with_loop parse: %s", command_block);
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
				log_bug(" Unknown next in parse_command");
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
	char		command_block[MAX_COMMAND_LEN];
	int		lines_in_block;
	int		line = start_line + 1;
	int		end_line = cmd->get_inherited_number_of_elements ();

	cmd->set_parse_helper(start_line, 0); // Clear out the start_line stuff.
	while (line > 0 && line <= end_line)
	{
		lines_in_block=cmd->reconstruct_inherited_command_block(command_block, MAX_COMMAND_LEN, line);
#ifdef	DEBUG
		log_debug("if_scope parse line %d: %s", line, command_block);
#endif	/* DEBUG */

		switch (what_is_next(command_block))
		{
			case NORMAL_NEXT:
				break;
			case ELSEIF_NEXT:
			case ELSE_NEXT:
				cmd->set_parse_helper (previous_block, line);
				previous_block = line;
				break;
			case ENDIF_NEXT:
				cmd->set_parse_helper (previous_block, line);
				/* Our start line needs to know where our end line is */
				cmd->set_parse_helper (start_line, ( (line) << 8) + cmd->get_parse_helper (start_line));
				return line;
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
				log_bug(" Unknown next in parse_command");
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
	
	// Smash leading spaces, because we don't like them.
	// Actually, its because some people like to additionally indent...
	while (q && (*q) && (*q == ' '))
		q++;

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
 *	Or we have a bug in the parser. Which seems to be true more often than not.
 */

void
context::do_at_endif (
const	CString&,
const	CString&)

{
	if(in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "ERROR: @endif in command when not expected.");
		log_bug("@endif in command %s(#%d) not expected", getname(get_current_command()), get_current_command());
		// Cause the command to abort.
		commands_executed = step_limit + 1;
		RETURN_FAIL;
	}
	else
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@endif can only be used inside commands.");
	}
	RETURN_FAIL;
}


void
context::do_at_chpid (
const	CString&,
const	CString&)

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
context::do_at_unchpid (
const	CString&,
const	CString&)

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
const	CString&,
const	CString&)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
}


void
context::do_at_true (
const	CString&,
const	CString&)

{
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_return (
const	CString&arg1,
const	CString&arg2)

{
	set_return_string (error_return_string);

	if (!in_command () || call_stack.is_empty ())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@return can only be used inside a command.");
		return_status = COMMAND_FAIL;
		return;
	}
	// We want to support a NULL return value!
	if (!arg1)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Usage: @return <true | false | current> = <return_string>.");
		return_status = COMMAND_FAIL;
		return;
	}

	if ((string_compare(arg1, "true") == 0) || (string_compare(arg1, "t") == 0))
		return_status = COMMAND_SUCC;
	else if ((string_compare (arg1, "false") == 0) || (string_compare (arg1, "f") == 0))
		return_status = COMMAND_FAIL;
	else if ((string_compare (arg1, "current") == 0) || (string_compare (arg1, "c") == 0))
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
context::do_at_returnchain (
const	CString&arg1,
const	CString&arg2)

{
	set_return_string (error_return_string);

	if (!in_command () || call_stack.is_empty ())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@return can only be used inside a command.");
		return_status = COMMAND_FAIL;
		return;
	}

	/* Syntax check: We must have a first argument, and we must have a second unless the first is "home" */
	if ((!arg1) || ((!arg2) && (string_compare (arg1, "home") != 0)))
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
		if (!(string_compare (arg1, "true")) || !(string_compare (arg1, "t")))
			return_status = COMMAND_SUCC;
		else if (!string_compare (arg1, "false") || !string_compare (arg1, "f"))
			return_status = COMMAND_FAIL;
		else if (!string_compare (arg1, "current") || !string_compare (arg1, "c"))
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
const	CString&arg1,
const	CString&)

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
	bool	if_ok = true;

	/* Check for the failure cases */
	const char* carg1 = arg1.c_str();
	if (arg1 && *carg1)
	{
		/* Strip leading spaces */
		while (isspace (*carg1))
			carg1++;

		/* Check for 0, Unset_return_value or Error */
		if_ok = string_compare ("0", carg1)
			&& string_compare (unset_return_string, carg1)
			&& string_compare (error_return_string, carg1);
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
const CString&arg1,
const CString&)

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
	bool	if_ok = true;

	/* Check for the failure cases */
	const char* carg1 = arg1.c_str();
	if (arg1 && *carg1)
	{
		/* Strip leading spaces */
		while (isspace (*carg1))
			carg1++;

		/* Check for 0, Unset_return_value or Error */
		if_ok = string_compare ("0", carg1)
			&& string_compare (unset_return_string, carg1)
			&& string_compare (error_return_string, carg1);
	}

	/* We've decided what to do, so... */
	call_stack.top ()->do_at_elseif (if_ok);
}


void
context::do_at_else (
const	CString&,
const	CString&)

{
	if ((!in_command()) || (call_stack.is_empty()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@else may only be used in compound commands.");
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
		return;
	}

	/* An else is simply an elseif that succeeds */
	call_stack.top ()->do_at_elseif (true);
}


void
context::do_at_end(
const CString&,
const CString&)

{
	return_status= COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!in_command() || call_stack.is_empty())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only use @end in a command");
		return;
	}

	/* Should never get here */
	log_bug("@end processed by %s in a command with no scope (%s)",
			unparse_object (GOD_ID, player),
			unparse_object (GOD_ID, get_current_command ())
	);
}


void
context::do_at_with(
const CString&what,
const CString&args)

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

	if (!(what && *what.c_str()) || !(args && *args.c_str()))
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
	const char* cargs = args.c_str();
	while (*cargs && ((*p++=*cargs++) != ','))
		;
	*--p = '\0';
	if (!ok_name(arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Bad name '%s' for first argument.", arg1);
		return;
	}

	SKIP_SPACES(cargs)
	if (!*cargs)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Missing second argument.");
		return;
	}

	if (!ok_name(cargs))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Bad name '%s' for second argument.", cargs);
		return;
	}

	/* Check we're safe */
	if (locate_innermost_arg (arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Variable name %s already in use.", arg1);
		return;
	}
	if (locate_innermost_arg (cargs))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@with: Variable name %s already in use.", cargs);
		return;
	}

	if (!call_stack.top()->push_scope (new With_loop (call_stack.top ()->innermost_scope (), target, arg1, cargs)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unable to set up loop - see a wizard.");
		return;
	}
	return_status= COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_for(
const CString&countername,
const CString&args)

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

	if ((!args) || (!*args.c_str()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, usage);
		return;
	}

	/* If we get to here, we've got a second argument.  Try some different legal parses... */
	if (sscanf (args.c_str(), "%d%*[ ]to%*[ ]%d%*[ ]step%*[ ]%d", &start, &end, &step) != 3
		&& sscanf (args.c_str(), "%dto%dstep%d", &start, &end, &step) != 3)
	{
		/* It wasn't the full 3-arg version.  Try 2-arg */
		if (sscanf (args.c_str(), "%d%*[ ]to%*[ ]%d", &start, &end) != 2
			&& sscanf (args.c_str(), "%dto%d", &start, &end) != 2)
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

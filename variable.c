static char SCCSid[] = "@(#)variable.c	1.50\t10/11/95";
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "db.h"
#include "interface.h"
#include "command.h"
#include "config.h"
#include "match.h"
#include "regexp_interface.h"
#include "context.h"
#include "externs.h"
#include "colour.h"

#define	MAX_NESTED_SUBSTITUTIONS	10
#define MAX_OPERANDS	4
#define DOWNCASE(x) (isupper(x) ? tolower(x) : (x))


static	const	char	too_big			[] = "TooBig";
static	const	char	ambiguous_variable	[] = "Ambiguous";
static	const	char	no_such_variable	[] = "No_such_variable";
static	const	char	unset_return_variable	[] = "Unset_return_value";


enum	eval_ops
{
	EVAL_OP_NONE		= 0,
	EVAL_OP_EQ		= 1,
	EVAL_OP_NEQ		= 2,
	EVAL_OP_LT		= 3,
	EVAL_OP_LE		= 4,
	EVAL_OP_GT		= 5,
	EVAL_OP_GE		= 6,
	EVAL_OP_NEGATE		= 7,
	EVAL_OP_ADD		= 8,
	EVAL_OP_SUB		= 9,
	EVAL_OP_MUL		= 10,
	EVAL_OP_DIVISION	= 11,
	EVAL_OP_MOD		= 12,
	EVAL_OP_NOT		= 13,
	EVAL_OP_AND		= 14,
	EVAL_OP_OR		= 15,
	EVAL_OP_BITNOT		= 16,
	EVAL_OP_BITAND		= 17,
	EVAL_OP_BITOR		= 18,
	EVAL_OP_BITXOR		= 19,
	EVAL_OP_CONCAT 		= 20,
	EVAL_OP_EXITMATCH	= 21,
	EVAL_OP_GETMATCH	= 22,
	EVAL_OP_SQRT		= 23,
	EVAL_OP_COS		= 24,
	EVAL_OP_SIN		= 25,
	EVAL_OP_TAN		= 26,
	EVAL_OP_ACOS		= 27,
	EVAL_OP_ASIN		= 28,
	EVAL_OP_ATAN		= 29,
	EVAL_OP_MIDSTRING	= 30,
	EVAL_OP_LENGTH		= 31,
	EVAL_OP_NCHAR		= 32,
	EVAL_OP_NITEM		= 33,
	EVAL_OP_NUMITEMS	= 34,
	EVAL_OP_SUBSTR		= 35,
	EVAL_OP_INT		= 36,
	EVAL_OP_DIV		= 37,
	EVAL_OP_DP		= 38,
	EVAL_OP_PI		= 39,
	EVAL_OP_NAMEC		= 40,
	EVAL_OP_MATCHITEM	= 41,
	EVAL_OP_HEAD		= 42,
	EVAL_OP_TAIL		= 43,
	EVAL_OP_REPLACE		= 44,
	EVAL_OP_DICMATCH	= 45,
	EVAL_OP_TYPEOF		= 46
};

enum	eval_types
{
	EVAL_TYPE_NONE,
	EVAL_TYPE_STRING,
	EVAL_TYPE_UNKNOWN,
	EVAL_TYPE_NUMERIC,
	EVAL_TYPE_FLOAT
};

#define EV_STRING	0x1
#define EV_NUMERIC	0x2
#define EV_UNARY	0x4
#define EV_FLOAT	0x8


struct	operand_block
{
	enum	eval_types	type;
	char			string [BUFFER_LEN];
	long			integer;
	double			floating;
};

struct	operation
{
	int			number_of_ops;
	int			type_of_operation[MAX_OPERANDS];
	int			position;
} ops[] =
{
	/* Apparently the 'position' field is used to say where the operator should be found, ie. 0 = prefix, 1 = infix - Lee */
	/* Operands	Types supported */
	{4,	{0,					0,					0, 0},		0},	/* NONE */

	{2,	{EV_STRING | EV_NUMERIC | EV_FLOAT, 	EV_STRING | EV_NUMERIC | EV_FLOAT},			1},	/* EQ */
	{2,	{EV_STRING | EV_NUMERIC | EV_FLOAT, 	EV_STRING | EV_NUMERIC | EV_FLOAT},			1},	/* NEQ */
	{2,	{EV_STRING | EV_NUMERIC | EV_FLOAT, 	EV_STRING | EV_NUMERIC | EV_FLOAT},			1},	/* LT */
	{2,	{EV_STRING | EV_NUMERIC | EV_FLOAT, 	EV_STRING | EV_NUMERIC | EV_FLOAT},			1},	/* LE */
	{2,	{EV_STRING | EV_NUMERIC | EV_FLOAT, 	EV_STRING | EV_NUMERIC | EV_FLOAT},			1},	/* GT */
	{2,	{EV_STRING | EV_NUMERIC | EV_FLOAT, 	EV_STRING | EV_NUMERIC | EV_FLOAT},			1},	/* GE */

	{1,	{EV_NUMERIC | EV_FLOAT},										0},	/* NEGATE - needs syntax checking to distiguish from SUB */
	{2,	{EV_NUMERIC | EV_FLOAT, 		EV_NUMERIC | EV_FLOAT},					1},	/* ADD */
	{2,	{EV_NUMERIC | EV_FLOAT, 		EV_NUMERIC | EV_FLOAT},					1},	/* SUB */
	{2,	{EV_NUMERIC | EV_FLOAT, 		EV_NUMERIC | EV_FLOAT},					1},	/* MUL */
	{2,	{EV_NUMERIC | EV_FLOAT, 		EV_NUMERIC | EV_FLOAT},					1},	/* DIVISION */
	{2,	{EV_NUMERIC, 				EV_NUMERIC},						1},	/* MOD */

	{1,	{EV_NUMERIC | EV_UNARY},									0},	/* NOT */
	{2,	{EV_NUMERIC, 				EV_NUMERIC},						1},	/* AND */
	{2,	{EV_NUMERIC,		 		EV_NUMERIC},						1},	/* OR */

	{1,	{EV_NUMERIC | EV_UNARY},									0},	/* BITNOT */
	{2,	{EV_NUMERIC, 				EV_NUMERIC},						1},	/* BITAND */
	{2,	{EV_NUMERIC, 				EV_NUMERIC},						1},	/* BITOR */
	{2,	{EV_NUMERIC, 				EV_NUMERIC},						1},	/* BITXOR */

	{2,	{EV_STRING, 				EV_STRING},						1}, 	/* CONCAT */
	{2,	{EV_STRING, 				EV_STRING},						1}, 	/* EXITMATCH */
	{2,	{EV_STRING, 				EV_STRING},						1},		/* GETMATCH */

	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* SQRT */
	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* COS */
	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* SIN */
	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* TAN */
	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* ACOS */
	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* ASIN */
	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* ATAN */

	{3,	{EV_NUMERIC,				EV_NUMERIC,				EV_STRING},	0},	/* MIDSTRING */
	{1,	{EV_STRING},											0},	/* LENGTH */
	{2,	{EV_NUMERIC,				EV_STRING},						0},	/* NCHAR */
	{3,	{EV_NUMERIC,				EV_STRING,				EV_STRING},	0},	/* NITEM */
	{2,	{EV_STRING,				EV_STRING},						0},	/* NUMITEMS */
	{3,	{EV_NUMERIC,				EV_STRING,				EV_STRING},	0},	/* SUBSTR */

	{1,	{EV_NUMERIC | EV_FLOAT},									0},	/* INT */
	{2,	{EV_NUMERIC | EV_FLOAT,			EV_NUMERIC | EV_FLOAT},					1},	/* DIV */
	{2,	{EV_NUMERIC,				EV_NUMERIC | EV_FLOAT},					0},	/* DP */
	{0,	{},												0},	/* PI */
	{1,	{EV_STRING},											0},	/* NAMEC */
	{3,	{EV_STRING,				EV_STRING,				EV_STRING},	0},	/* MATCHITEM */
	{2,	{EV_STRING,				EV_STRING},						0},	/* HEAD */
	{2,	{EV_STRING,				EV_STRING},						0},	/* TAIL */
	{4,	{EV_NUMERIC | EV_STRING,		EV_STRING,				EV_STRING,				EV_STRING},	0},	/* REPLACE */
	{2,	{EV_STRING,				EV_STRING},						0},	/* DICMATCH */
	{1,	{EV_NUMERIC | EV_STRING,		EV_STRING},						0}	/* TYPEOF */
};


static	const	char		*bracket_match			(const char *, const char, const char);
static	Command_status		full_parse_expression		(context &c, const char *expression, char *result_buffer, int space_left);
static	void			full_parse_operand		(context &c, const char *&expression, char *result, int space_left);
static	eval_ops		full_parse_operator		(context &c, const char *&expression, int ops_found);
static	Command_status		full_frig_types			(context &c, enum eval_ops, struct operand_block*, int);


/* Here is a description of how local and globals work:
   (NB) If a command is set backwards, @local acts as if it was @global

	@global variables can be created anywhere within a chain and persist
	until the end of the whole chain/nesting of compound commands.
	@globals created within a nested command persist after the nested
	command has finished executing
	An @global is visible to any command in the chain/nesting.
	Hence if command a creates a global called fred and then calls command
	b, and b attempts to create a global called fred, it will redefine the
	existing 'fred'.

	@local gives variables which are fully local to that nesting.
	That is, if command a calls command b, and command b creates
	locals, then when it ends, command a will not be able to see them
	as they are lost when b ends.
	Command b will not be able to see and locals previously created by a.
	Using @local with a var name used by a will create a local variable of
	that name, within command b. Contrast this with what happens to the
	variable 'fred' described above. Command a's local will not be visible.
	If you need it to be visible, use @global.

	Scope variables - that is, the variables associated with @For, @with
	and so on, may be fully nested within a compound command and disappear
	at the end of the @for/@with loop. This is for both old style and new
	commands. 
	Note that under the new system you can re-use a loop variable name
	in two different commands, eg:
	a: @for loop=1 to 2, b, @end
	b: @for loop=1 to 5, @echo yeah, @end
	this will now work. It wouldn't before - it would say you can't
	re-use the var name.

	Right now I've written it maybe I can code it!

	Incidentally this is written by using a variable_stack class,
	(dont' you love C++) and storing globals as part of the context,
	and locals in the call stack as part of compound_command_and_arguments

	- Dunk, with his longest comment ever. 17/5/97 */

void
context::do_at_local (
const	char	*name,
const	char	*val)

{
	set_return_string (error_return_string);
	return_status= COMMAND_FAIL;

	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "@local may only be used within compound commands.") ;
		return;
	}

	if (Backwards(get_current_command()))
	{
		do_at_global(name, val);
		return;
	}

	if (!ok_name(name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@local: Bad variable name.") ;
		return;
	}

	/* Check whether the argument exists in the top nesting */
	if (call_stack.is_empty())
	{
		Trace("BUG: Trying to add a local to an empty call stack!");
		return; /* Erm, where can we add it! This should never occur */
	}

	String_pair	*temp = call_stack.top()->locatearg (name);
	if (temp)
		/* It's an existing variable, so update its value */
		temp->set_value (val);
	else
		/* It doesn't exist - try to add it */
		if (!call_stack.top()->addarg (name, val))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "BUG: @local: Couldn't add local variable.") ;
			fputs ("BUG: @local: Couldn't add variable\n", stderr);
			return;
		}

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_global (
const	char	*name,
const	char	*val)

{
	set_return_string (error_return_string);
	return_status= COMMAND_FAIL;

	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "@global may only be used within compound commands.") ;
		return;
	}

	if (!ok_name(name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@global: Bad variable name.") ;
		return;
	}

	/* Check whether the argument exists in the global stack */

	String_pair	*temp = locatearg (name);
	if (temp)
		/* It's an existing variable, so update its value */
		temp->set_value (val);
	else
		/* It doesn't exist - try to add it */
		if (!addarg (name, val))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "BUG: @local: Couldn't add local variable.") ;
			fputs ("BUG: @global: Couldn't add variable\n", stderr);
			return;
		}

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_test (
const	char	*arg1,
const	char	*arg2)
{
	/* A few things fail, everything else succeeds */
	set_return_string (ok_return_string);
	return_status = COMMAND_SUCC;

	/* NULL first argument means success */
	if (!arg1)
		return;

	/* Anything with an = in it is clearly neither 0 nor Error */
	if (arg2 && *arg2)
		return;

	/* Strip leading spaces */
	while (isspace (*arg1))
		arg1++;

	/* Check for 0, Unset_return_value or Error */
	if ((!string_compare ("0", arg1))
		|| (!string_compare (unset_return_string, arg1))
		|| (!string_compare (error_return_string, arg1)))
	{
		set_return_string (error_return_string);
		return_status = COMMAND_FAIL;
	}

	/* Should be no trailing spaces */
}


const Boolean
context::variable_substitution (
const	char	*arg,
	char	*result,
const	int	max_length)

{
	Boolean retval;

	retval = nested_variable_substitution (arg, result, 0, max_length);
	if (strlen (result) > max_length)
	{
		Trace( "BUG: Buffer overflow: %s\n", getname (get_player ()));
		result[max_length -1] = '\0';
	}

	return retval;
}


/*
 * dollar_substitute: We've just found a string with a $ in it; argp points to the first
 *	character past the $.  Now we need to grab the variable name and do the substitution.
 *
 * This substitution may cause a recursive call to nested_variable_substitution.
 *
 * Return value:
 *	False	if a command needs to be executed to complete the substitution.
 *	True	if the substitution proceeded to completion.
 *
 * PJC 22/2/97.
 */

const Boolean
context::dollar_substitute (
const	char	*&argp,
	char	*&resp,
const	int	depth,
	int	space_left)

{
		char		source_buffer [BUFFER_LEN];
		char		result_buffer [BUFFER_LEN + 100]; /* Not bombproof, but should be good enough */
	const	char		*name_start;
	const	char		*name_end;

	if (*argp == '{')
	{
		const	char	*source_pointer = 0;
		/* It's a nested substitution */
		++argp;
		name_end = bracket_match (argp, '{', '}');
		strncpy (source_buffer, argp, name_end - argp);
		source_buffer [name_end - argp] = '\0';
		source_pointer = source_buffer;
		nested_variable_substitution (source_pointer, result_buffer, depth + 1, space_left);
		if (strlen (result_buffer) > space_left)
		{
			Trace( "Buffer overflow by %s.\n", getname (get_player ()));
			strcpy (result_buffer, too_big);
		}
		name_start = result_buffer;

		/* Skip the close-brace */
		argp = name_end + 1;
	}
	else
	{
		int	in_square = 0;

		/* Normal substitution */
		name_start = argp;

		/* A-Za-z0-9_:#[] are legal characters for variable names */
		while ((
				(*argp >= '0' && *argp <= '9')
				|| (*argp >= 'A' && *argp <= 'Z')
				|| (*argp >= 'a' && *argp <= 'z')
				|| *argp == ':'
				|| *argp == '_'
				|| *argp == '['
				|| *argp == ']'
				|| *argp == '#')
			&& in_square >= 0)
			{
				/* Remember bracket-nesting, and spot mismatches */
				switch(*argp)
				{
					case '[':
						in_square++;
						break;
					case ']':
						in_square--;
						break;
				}
				argp++;
			}
		if(in_square < 0)
			argp--;

		/* Picked up the variable, now copy it. */
		strncpy (source_buffer, name_start, argp - name_start);
		source_buffer [argp - name_start] = '\0';
		name_start = source_buffer;
	}

	/*
	 * At this point, we've got the name to match in name_start, and argp points past
	 *	the last character of the variable name.  PJC 18/2/97.
	 *
	 * We can also guarantee that this substitution will proceed to completion from here.
	 */

	const	String_pair	*arg;
	const	char		*value = 0;
		int		length;

	/* ... check for $[0123], else match... */
	if (!strcmp (name_start, "0"))
	{
		/* Copy return string onto the result */
		value = get_return_string () ? get_return_string () : unset_return_variable;
	}
	else if (!strcmp (name_start, "1"))
	{
		/* Copy arg1 onto the result */
		value = get_innermost_arg1 () ? get_innermost_arg1 () : in_command () ? empty_string : no_such_variable;
	}
	else if (!strcmp (name_start, "2"))
	{
		/* Copy arg2 onto the result */
		value = get_innermost_arg2 () ? get_innermost_arg2 () : in_command () ? empty_string : no_such_variable;
	}
	else if (!strcmp (name_start, "3"))
	{
		/* Copy arg1 ## arg2 onto the result */
		char *dummy = resp;

		/* Copy the first argument if one exists */
		if ((value = get_innermost_arg1 ()))
		{
			if((length = strlen (value)) > space_left)
			{
				strcpy (resp, too_big);
				return True;
			}
			strcpy (resp, value);
			resp += length;
			space_left -= length;
		}
		if ((value = get_innermost_arg2 ()) && *value)
		{
			if((length = strlen (value)) > space_left)
			{
				strcpy (dummy, too_big);
				return True;
			}
			strcpy (resp++, "=");
			strcpy (resp, value);
			resp += length;
			space_left -= length + 1;
		}

		/* If dummy or value are set, we did it.  Either way, stop the variable match later. */
		value = (value || dummy) ? empty_string : in_command () ? empty_string : no_such_variable;
	}
	else if (in_command() && (arg = locate_innermost_arg (name_start)))
		/* Matched a local variable */
		value = arg->get_value ();

	else if (in_command() && (arg = locatearg (name_start)))
		/* Matched a global variable */
		value= arg->get_value ();
	else
	{
		/* Match... */
		Matcher matcher (get_player (), name_start, TYPE_NO_TYPE, get_effective_id ());
		matcher.match_variable ();
		matcher.match_array_or_dictionary ();

		/* ... copy the error string or the real one... */
		const	dbref	variable = matcher.match_result ();

		if (variable == NOTHING)
			value = no_such_variable;
		else if (variable == AMBIGUOUS)
			value = ambiguous_variable;
		else switch(Typeof(variable))
		{
			case TYPE_VARIABLE:
			case TYPE_PROPERTY:
				if (db[variable].get_description() != NULL)
					value = value_or_empty (db[variable].get_description());
				break;
			case TYPE_ARRAY:
				if(db[variable].exist_element (atoi (value_or_empty (matcher.match_index_result()))))
					value = value_or_empty (db[variable].get_element (atoi (value_or_empty (matcher.match_index_result()))));
				break;
			case TYPE_DICTIONARY:
				if(db[variable].exist_element(matcher.match_index_result()))
					value = value_or_empty (db[variable].get_element(db[variable].exist_element(matcher.match_index_result())));
				break;
		}
	}

	/* OK, we should have *something* by now... */
	if (value)
	{
		if ((length = strlen(value)) > space_left)
		{
			strcpy(resp, too_big);
			return True;
		}

		strcpy (resp, value);
		resp += length;
		space_left -= length;

	}
	else
		*resp = '\0';
	return True;
}


const Boolean
context::nested_variable_substitution (
const	char	*&argp,
	char	*resp,
const	int	depth,
	int	space_left)

{
	Boolean		backslash_primed = False;
	Boolean		breakout;
	char		*result;
	char		*end;
	char		*tmp;

	end = resp + space_left;
	/* Break out of the recursion if it exceeds a sensible level */
	if (depth > MAX_NESTED_SUBSTITUTIONS)
	{
		/** Match close-braces with *argp to allow higher levels to continue **/
		strcpy (resp, recursion_return_string);
		return True;
	}

	result = resp;
	/* Cope with multiple non-nested $-substitutions */
	while (True)
	{
		/* Copy to result until first unescaped $ or { */
		breakout = False;
		while (!breakout && *argp)
		{
			if (backslash_primed)
			{
				*resp++ = *argp++;
				backslash_primed = False;
				space_left--;
				if(space_left <= 0)
				{
					strcpy(result, too_big);
					return True;
				}
			}
			else
			{
				/* If we get here, this character can't have been backslash-escaped */
				switch (*argp)
				{
					case '\\':
						backslash_primed = True;
						argp++;
						break;
					case '$':
					case '{':
						breakout = True;
						break;
					default:
						*resp++ = *argp++;
						space_left--;
						if(space_left <= 0)
						{
							strcpy(result, too_big);
							return True;
						}
				}
			}
		}

		/* If we broke out due to finding a $ or a {, do the substitution; otherwise, we must have hit EOS */
		if (!breakout)
		{
			/* End of string */
			*resp = '\0';
			break;
		}
		else
		{
			tmp = resp;
			if (*argp == '$')
			{
				++argp;
				dollar_substitute (argp, resp, depth, space_left);
				if (resp >= end)
				{
					strcpy (tmp, too_big);
					return True;
				}
			}
			else if (*argp == '{')
			{
				++argp;
				brace_substitute (argp, resp, space_left);
				if (resp >= end)
				{
					strcpy (tmp, too_big);
					return True;
				}
			}
		}
	}

	return True;
}


/*
 * brace_substitute: Do the evaluation.
 */

void
context::brace_substitute (
const	char	*&argp,
	char	*&resp,
	int	space_left)

{
	const	char		*command_start;
	const	char		*result;
		Command_status	rs;
		char		buffer [MAX_COMMAND_LEN];

	command_start = argp;
	argp = bracket_match (argp, '{', '}');

	/* Copy and process the nested evaluation */
	strncpy (buffer, command_start, argp - command_start);
	buffer [argp - command_start] = '\0';

	/*** FIX ME!!! ***/
	result = sneakily_process_basic_command (buffer, rs);
	if(space_left < strlen(result))
		result = too_big;
	strcpy (resp, result);
	resp += strlen (result);
	maybe_free (result);

	/* Skip the close-brace if it exists */
	if (*argp == '}')
		++argp;
}


/*
 * bracket_match: Match character pairs. When called, current is the
 *	first character PAST the opening end, which is why level
 *	starts as 1.
 *
 * Return value:
 *	End-of-string if no matching character is found;
 *	The address of the matching character if one is found.
 */

static const char *
bracket_match (
const	char	*current,
const	char	left_match,
const	char	right_match)

{
	int	level;

	for (level = 1; *current && (level > 0); current++)
		if (*current == right_match)
			--level;
		else if (*current == left_match)
			++level;

	/* Correct overshoot */
	if (!level)
		--current;

	return current;
}


void
context::do_full_evaluate (
const	char	*arg1,
const	char	*arg2)

{
	char	buffer [BUFFER_LEN];
	char	scratch_return_string [BUFFER_LEN];

	/* Bombproof if NULL arg1 */
	if (arg1 == NULL)
		*buffer = '\0';
	else
		strcpy(buffer, arg1);

	/* Now dump arg2 in as well. */
	if (arg2 && *arg2)
	{
		int	len1;

		len1 = strlen (buffer);
		if ((len1 > 0) && (buffer [len1 - 1] != '!') && (buffer [len1 - 1] != '<') && (buffer [len1 - 1] != '>'))
			buffer [len1++] = ' ';
		buffer [len1++] = '=';
		if (*arg2 != '=')
		{
			buffer [len1++] = ' ';
		}
		strcpy (buffer + len1, arg2);
	}
	return_status = full_parse_expression (*this, buffer, scratch_return_string, BUFFER_LEN);
	set_return_string (scratch_return_string);
}


static Command_status
full_parse_expression (
	context	&c,
const	char	*expression,
	char	*result_buffer,
	int	space_left)

{

		enum	eval_ops	op = EVAL_OP_NONE;
	const	char			*match;
	const	char			*p;
	const	char			*cached_expression;
	struct	operand_block		results[MAX_OPERANDS];
		int			number_operands_found = 0;
		int			frig_count;
	struct	operand_block		final;
		char *			apointer;
		char *			bpointer;
		int			i;
		Command_status		return_status;

	return_status = COMMAND_SUCC;

	/* Skip leading spaces */
	while (isspace (*expression))
		expression++;

	while (1)
	{
		while(number_operands_found < (ops[op].number_of_ops))
		{
			if (!op)
			{
				/* full_parse_operator may move expression, so just in case, we'll save a copy */
				cached_expression = expression;
				if ((op = full_parse_operator(c, expression, number_operands_found))
					&& (ops[op].position == number_operands_found))
				{
					while (*expression && !isspace (*expression))
						expression++;
				}
				else
				{
					/* That copy was justified - the operator check failed. */
					expression = cached_expression;
					full_parse_operand(c, expression, results[number_operands_found++].string, BUFFER_LEN);
				}
			}
			else
			{
				full_parse_operand(c, expression, results[number_operands_found++].string, BUFFER_LEN);
			}
			/* Skip white space */
			while (isspace (*expression))
				expression++;
		}
		/* Evaluate: First make any required arguments numeric (or scream)... */

		if (!op)
		{
			if(strlen(results[0].string) > space_left)
				strcpy(result_buffer, too_big);
			else
				strcpy(result_buffer,results[0].string);
			return (return_status);
		}


		for (frig_count = 0; frig_count < ops[op].number_of_ops; frig_count++)
		{
			return_status = full_frig_types (c, op, &results [frig_count], ops[op].type_of_operation[frig_count]);
			if (results [frig_count].type == EVAL_TYPE_NONE)
			{
				strcpy (result_buffer, error_return_string);
				return (return_status);
			}
		}

#define OpType(x)	results[x].type

		/* ... then do the correct op. For numerics, if one side is numeric they both are by now. */
		switch (op)
		{
			case EVAL_OP_EQ:
				if ((OpType(0) == EVAL_TYPE_STRING) || (OpType(1) == EVAL_TYPE_STRING))
				{
					final.integer = !string_compare (results[0].string, results[1].string);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].floating == results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].integer == results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (results[0].floating == results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer == results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_NEQ:
				if ((OpType(0) == EVAL_TYPE_STRING) || (OpType(1) == EVAL_TYPE_STRING))
				{
					final.integer = string_compare (results[0].string, results[1].string);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].floating != results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].integer != results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (results[0].floating != results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer != results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_LT:
				if ((OpType(0) == EVAL_TYPE_STRING) || (OpType(1) == EVAL_TYPE_STRING))
				{
					final.integer = string_compare (results[0].string, results[1].string) < 0;
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].floating < results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].integer < results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (results[0].floating < results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer < results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_LE:
				if ((OpType(0) == EVAL_TYPE_STRING) || (OpType(1) == EVAL_TYPE_STRING))
				{
					final.integer = string_compare (results[0].string, results[1].string) <= 0;
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].floating <= results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].integer <= results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (results[0].floating <= results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer <= results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_GT:
				if ((OpType(0) == EVAL_TYPE_STRING) || (OpType(1) == EVAL_TYPE_STRING))
				{
					final.integer = string_compare (results[0].string, results[1].string) > 0;
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].floating > results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].integer > results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (results[0].floating > results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer > results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_GE:
				if ((OpType(0) == EVAL_TYPE_STRING) || (OpType(1) == EVAL_TYPE_STRING))
				{
					final.integer = string_compare (results[0].string, results[1].string) >= 0;
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].floating >= results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (results[0].integer >= results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (results[0].floating >= results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer >= results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_ADD:
				if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].floating + results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].integer + results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.floating = (results[0].floating + results[1].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.integer = (results[0].integer + results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_NEGATE:
				final.type=OpType(0);
				final.floating = -results[0].floating;
				final.integer = -results[0].integer;
				break;

			case EVAL_OP_SUB:
				if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].floating - results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].integer - results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.floating = (results[0].floating - results[1].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.integer = (results[0].integer - results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_MUL:
				if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].floating * results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].integer * results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.floating = (results[0].floating * results[1].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.integer = (results[0].integer * results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_DIVISION:
				if ((results[1].integer == 0) && (results[1].floating == 0))
				{
					notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Division by zero.");
					final.type = EVAL_TYPE_STRING;
					strcpy (final.string, error_return_string);
					return_status = COMMAND_FAIL;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].floating / results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.floating = (results[0].integer / results[1].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.floating = (results[0].floating / results[1].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = ((float)results[0].integer / (float)results[1].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_MOD:
				if (results[1].integer == 0)
				{
					notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Modulus of zero.");
					final.type = EVAL_TYPE_STRING;
					strcpy (final.string, error_return_string);
					return_status = COMMAND_FAIL;
				}
				else
				{
					final.integer = (results[0].integer % results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_NOT:
				final.integer = !results[0].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_AND:
				final.integer = results[0].integer && results[1].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_OR:
				final.integer = results[0].integer || results[1].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_BITNOT:
				final.integer = ~results[0].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_BITAND:
				final.integer = results[0].integer & results[1].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_BITOR:
				final.integer = results[0].integer | results[1].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_BITXOR:
				final.integer = results[0].integer ^ results[1].integer;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_CONCAT:
				strcpy (final.string, results[0].string);
				strcat (final.string, results[1].string);
				final.type = EVAL_TYPE_STRING;
				break;

			case EVAL_OP_EXITMATCH:
				final.integer = 0;
				match = results[1].string;
				while(*match)
				{
					/* check out this one */
					for(p = results[0].string;
						*p && DOWNCASE(*p) == DOWNCASE(*match) && *match != EXIT_DELIMITER;
						p++, match++);

					/* did we get it? */
					if(*p == '\0')
					{
						/* make sure there's nothing afterwards */
						while(isspace(*match))
							match++;
						if(*match == '\0' || *match == EXIT_DELIMITER)
						{
							/* we got it */
							final.integer = 1;
							break;
						}
					}
					/* we didn't get it, find next match */
					while(*match && *match++ != EXIT_DELIMITER)
						;
					while (isspace (*match))
						match++;
				}
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_GETMATCH:
				final.integer = (string_match (results[1].string, results[0].string) != NULL);
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_SQRT:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					if (results[0].integer != 0)
					{
						final.floating = sqrt((double)results[0].integer);
						final.type = EVAL_TYPE_FLOAT;
					}
					else
					{
						notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Outside sqr domain");
						strcpy (result_buffer, error_return_string);
						return (COMMAND_FAIL);
					}
				}
				else
				{
					final.floating = sqrt(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_COS:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					final.floating = cos((double)results[0].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = cos(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_SIN:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					final.floating = sin((double)results[0].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = sin(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_TAN:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					final.floating = tan((double)results[0].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = tan(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_ACOS:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					final.floating = acos((double)results[0].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = acos(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_ASIN:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					final.floating = asin((double)results[0].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = asin(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_ATAN:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
				{
					final.floating = atan((double)results[0].integer);
					final.type = EVAL_TYPE_FLOAT;
				}
				else
				{
					final.floating = atan(results[0].floating);
					final.type = EVAL_TYPE_FLOAT;
				}
				break;

			case EVAL_OP_MIDSTRING:
				if (strlen(results[2].string) < results[1].integer)
				{
					if (c.gagged_command()==False)
						notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Argument past end of string in midstring");

					strcpy (result_buffer, error_return_string);
					return (COMMAND_FAIL);
				}
				else
				{
					results[2].string[results[1].integer] = '\0';
					strcpy(final.string, results[2].string + results[0].integer - 1);
					final.type = EVAL_TYPE_STRING;
					break;
				}

			case EVAL_OP_TYPEOF:
				final.type = EVAL_TYPE_STRING;
				switch (OpType(0))
				{
					case EVAL_TYPE_NUMERIC:
						strcpy(final.string, "INTEGER");
						break;
					case EVAL_TYPE_STRING:
						strcpy(final.string, "STRING");
						break;
					case EVAL_TYPE_FLOAT:
						strcpy(final.string, "FLOAT");
						break;
					default:
						strcpy(final.string, "UNKNOWN");
				}
				break;

			case EVAL_OP_LENGTH:
				final.integer = strlen(results[0].string);
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_NCHAR:
				if (strlen(results[1].string) < results[0].integer)
				{
					if (c.gagged_command()==False)
						notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,"Argument past end of string in nchar");

					strcpy (result_buffer, error_return_string);
					return (COMMAND_FAIL);
				}
				else
				{
					final.string[0] = results[1].string[results[0].integer - 1];
					final.string[1] = '\0';
					final.type = EVAL_TYPE_STRING;
					break;
				}

			case EVAL_OP_NITEM:
				if (results[0].integer == 0)
				{
					notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Zero argument to nitem");
					strcpy (result_buffer, error_return_string);
					return (COMMAND_FAIL);
				}

				for (apointer = strtok(results[2].string,results[1].string), i = 1; i < results[0].integer; i++)
					apointer = strtok(NULL,results[1].string);
				if (apointer != NULL)
					strcpy(final.string,apointer);
				else
					final.string[0] = '\0';
				final.type = EVAL_TYPE_STRING;
				break;

			case EVAL_OP_NUMITEMS:
				if (results[0].string[0] == 0)
				{
					notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "No arguments passed to numitems.");
					strcpy (result_buffer, error_return_string);
					return (COMMAND_FAIL);
				}
				if (results[1].string[0] == 0)
				{
					notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Missing second argument to numitems.");
					strcpy (result_buffer, error_return_string);
					return (COMMAND_FAIL);
				}

				apointer = results[1].string;
				for (final.integer = 0; apointer != NULL; final.integer++)
					apointer = strstr(apointer + 1, results[0].string);
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_SUBSTR:
				if ((results[0].integer < 1) || (results[0].integer > strlen(results[2].string)))
				{
					notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Substr position out of range");
					strcpy (result_buffer, error_return_string);
					return (COMMAND_FAIL);
				}

				apointer = strstr((results[2].string + results[0].integer - 1), results[1].string);

				if (apointer == NULL)
					final.integer = 0;
				else
					final.integer = apointer - results[2].string + 1;
				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_INT:
				if (OpType(0) == EVAL_TYPE_NUMERIC)
					final.integer = results[0].integer;
				else
					final.integer = (int)results[0].floating;

				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_DIV:
				if (results[1].integer == 0)
				{
					notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Division by zero.");
					final.type = EVAL_TYPE_STRING;
					strcpy (final.string, error_return_string);
					return_status = COMMAND_FAIL;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (int)(results[0].floating / results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_NUMERIC) && (OpType(1) == EVAL_TYPE_FLOAT))
				{
					final.integer = (int)(results[0].integer / results[1].floating);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else if ((OpType(0) == EVAL_TYPE_FLOAT) && (OpType(1) == EVAL_TYPE_NUMERIC))
				{
					final.integer = (int)(results[0].floating / results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				else
				{
					final.integer = (results[0].integer / results[1].integer);
					final.type = EVAL_TYPE_NUMERIC;
				}
				break;

			case EVAL_OP_DP:
				if (OpType(1) == EVAL_TYPE_NUMERIC)
				{
					results[1].floating = results[1].integer;
					sprintf(final.string, "%.*f", (int) results[0].integer, results[1].floating);
				}
				else
				{
					sprintf(final.string, "%.*f", (int) results[0].integer, results[1].floating);
				}
				final.type = EVAL_TYPE_STRING;
				break;

			case EVAL_OP_PI:
				final.floating = M_PI;
				final.type = EVAL_TYPE_FLOAT;
				break;

			case EVAL_OP_NAMEC:
				final.type = EVAL_TYPE_STRING;
				strcpy (final.string, getname(match_connected_player(results[0].string)));
				break;

			case EVAL_OP_MATCHITEM:
				apointer = results[2].string;
				i = 1;
				final.integer = 0;

				apointer = strtok(results[2].string,results[1].string);

				while(apointer != NULL)
				{
					if (strcmp(results[0].string, apointer) == 0)
					{
						final.integer = i;
						break;
					}
					apointer = strtok(NULL,results[1].string);
					i++;
				}

				final.type = EVAL_TYPE_NUMERIC;
				break;

			case EVAL_OP_HEAD:
				final.string[0] = '\0';
				if(results[1].string)
				{
					apointer = strstr(results[1].string, results[0].string);
					if(apointer)
					{
						*apointer = '\0';
						strcpy(final.string,results[1].string);
					}
					else
						strcpy(final.string,results[1].string);
				}
				final.type = EVAL_TYPE_STRING;
				break;

			case EVAL_OP_TAIL:
				final.string[0] = '\0';
				if(results[1].string)
				{
					apointer = strstr(results[1].string, results[0].string);
					if(apointer)
					{
						apointer+=strlen(results[0].string);
						strcpy(final.string,apointer);
					}
				}
				final.type = EVAL_TYPE_STRING;
				break;

			case EVAL_OP_REPLACE:
				char *nextocc;
				int searchlen;

				final.type = EVAL_TYPE_STRING;
				apointer = results[3].string;
				bpointer = final.string;
				if (OpType(0) != EVAL_TYPE_NUMERIC)
				{
					if(OpType(0) == EVAL_TYPE_STRING && string_prefix(results[0].string, "all"))
						results[0].integer = -1;
					else
					{
						notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Bad number of replaces for @eval replace function - %s", results[0].string);
						final.type = EVAL_TYPE_NONE;
						strcpy (result_buffer, error_return_string);
						return (COMMAND_FAIL);
					}
				}
				if (results[0].integer == 0)
				{
					strcpy(final.string, results[3].string);
					break;
				}
				/* Check that the search string has at least one character in it - else we keep looping forever */
				if ((searchlen = strlen(results[1].string)) < 1)
				{
					notify_colour (c.get_player (),c.get_player(), COLOUR_ERROR_MESSAGES, "Zero length search string in @eval replace - doing no replacement.");
					strcpy (final.string, results [3].string);
					break;
				}
				final.string[0] = '\0';
				while(((results[0].integer--) != 0) && ((nextocc = strstr(apointer, results[1].string)) != NULL))
				{
					*nextocc = '\0';
					strcpy(bpointer, apointer);
					strcat(bpointer, results[2].string);
					bpointer += strlen(bpointer);
					apointer = nextocc + searchlen;
				}
				strcat(final.string, apointer);
				break;

			case EVAL_OP_DICMATCH:
				{
					dbref	dic;

					final.type = EVAL_TYPE_STRING;
					final.string [0] = '\0';

					Matcher matcher (c.get_player (), results[0].string, TYPE_DICTIONARY, c.get_effective_id ());
					matcher.match_variable ();
					matcher.match_absolute ();
					if ((dic = matcher.noisy_match_result ()) == NOTHING)
					{
						strcpy (result_buffer, error_return_string);
						return (COMMAND_FAIL);
					}
					if (Typeof (dic) != TYPE_DICTIONARY)
					{
						notify_colour (c.get_player (),c.get_player(), COLOUR_ERROR_MESSAGES, "%s is not a dictionary.", results[0].string);
						strcpy (result_buffer, error_return_string);
						return (COMMAND_FAIL);
					}
					for (i = 1; i <= db [dic].get_number_of_elements (); i++)
						if (string_match (results[1].string, db [dic].get_index (i)) != NULL)
						{
							if (final.string [0] != '\0')
								strcat (final.string, ";");
							strcat (final.string, value_or_empty (db [dic].get_element (i)));
						}
				}
				break;

			default:
				notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Unknown op.");
				strcpy (result_buffer, error_return_string);
				return (COMMAND_FAIL);
		}


		/* Skip trailing spaces */
		while (isspace (*expression))
			++expression;

		/* If we've hit EOS, we've completed */
		if (!*expression)
		{
			switch (final.type)
			{
				case EVAL_TYPE_NUMERIC:
					sprintf (result_buffer, "%ld", final.integer);
					break;
				case EVAL_TYPE_FLOAT:
					sprintf (result_buffer, "%0.10g", final.floating);
					break;
				case EVAL_TYPE_STRING:
				default:
					strcpy (result_buffer, final.string);
			}
			return (return_status);
		}

		/* Otherwise, there's more, so keep going */
		switch (final.type)
		{
			case EVAL_TYPE_NUMERIC:
				sprintf(results[0].string, "%ld", final.integer);
				break;
			case EVAL_TYPE_FLOAT:
				sprintf(results[0].string, "%0.10g", final.floating);
				break;
			case EVAL_TYPE_STRING:
			default:
				strcpy (results[0].string, final.string);
		}
		number_operands_found = 1;
		op = EVAL_OP_NONE;
		results[0].type = final.type;
	}
}


static void
full_parse_operand (
	context		&c,
const	char		*&expression,
	char		*result_buffer,
	int		space_left)

{
	const	char	*command_start;

	if (*expression == '(')
	{
		/* Nested evaluate */

		char	buffer [BUFFER_LEN];

		command_start = ++expression;
		expression = bracket_match (expression, '(', ')');

		/* Copy and process the nested evaluation */
		strncpy (buffer, command_start, expression - command_start);
		buffer [expression - command_start] = '\0';
		full_parse_expression (c, buffer, result_buffer, space_left);

		/* Skip the ')' */
		++expression;
	}
	else if (*expression == '"')
	{
		command_start = ++expression;
		expression = bracket_match (expression, '"', '"');
		strncpy (result_buffer, command_start, expression - command_start);
		result_buffer [expression - command_start] = '\0';
		++expression;
	}
	else if (*expression == '$')
	{
		++expression;
		c.dollar_substitute (expression, result_buffer, 0, space_left);
	}
	else if (*expression == '{')
	{
		++expression;
		c.brace_substitute (expression, result_buffer, space_left);
	}
	else
	{
		while (*expression && !isspace (*expression))
			*result_buffer++ = *expression++;
		*result_buffer = '\0';
	}
}



static enum eval_ops
full_parse_operator (
	context	&c,
const	char	*&expression,
	int	ops_found)

{
	switch (*expression)
	{
		case '~':
			++expression;
			switch (*expression)
			{
				case 'E':
				case 'e':
					return (EVAL_OP_EXITMATCH);
				case 'G':
				case 'g':
					return (EVAL_OP_GETMATCH);
				default:
					--expression;
					return (EVAL_OP_BITNOT);
			}
		case 'a':
		case 'A':
			++expression;
			switch (*expression)
			{
				case 'c':
				case 'C':
					if (string_prefix (expression, "cos"))
						return (EVAL_OP_ACOS);
					else
						return (EVAL_OP_NONE);
				case 's':
				case 'S':
					if (string_prefix (expression, "sin"))
						return (EVAL_OP_ASIN);
					else
						return (EVAL_OP_NONE);
				case 't':
				case 'T':
					if (string_prefix (expression, "tan"))
						return (EVAL_OP_ATAN);
					else
						return (EVAL_OP_NONE);
			}
			--expression;
			return (EVAL_OP_NONE);
		case 'c':
		case 'C':
			if (string_prefix (expression, "cos"))
				return (EVAL_OP_COS);
			else
				return (EVAL_OP_NONE);
		case 'd':
		case 'D':
			++expression;
			switch (*expression)
			{
				case 'i':
				case 'I':
					if (string_prefix (expression, "iv"))
						return (EVAL_OP_DIV);
					else
						return (EVAL_OP_NONE);
				case 'm':
				case 'M':
					if (string_prefix (expression, "match"))
						return (EVAL_OP_DICMATCH);
					else
						return (EVAL_OP_NONE);
				case 'p':
				case 'P':
					if (string_prefix (expression, "p"))
						return (EVAL_OP_DP);
					else
						return (EVAL_OP_NONE);
			}
			--expression;
			return (EVAL_OP_NONE);
		case 'h':
		case 'H':
			if (string_prefix (expression, "head"))
				return (EVAL_OP_HEAD);
			else
				return (EVAL_OP_NONE);

		case 'i':
		case 'I':
			if (string_prefix (expression, "int"))
				return (EVAL_OP_INT);
			else
				return (EVAL_OP_NONE);
		case 'l':
		case 'L':
			if (string_prefix (expression, "length"))
				return (EVAL_OP_LENGTH);
			else
				return (EVAL_OP_NONE);
		case 'm':
		case 'M':
			++expression;
			switch (*expression)
			{
				case 'a':
				case 'A':
					if (string_prefix (expression, "atchitem"))
						return (EVAL_OP_MATCHITEM);
					else
						return (EVAL_OP_NONE);
				case 'i':
				case 'I':
					if (string_prefix (expression, "idstring"))
						return (EVAL_OP_MIDSTRING);
					else
						return (EVAL_OP_NONE);
				case 'o':
				case 'O':
					if (string_prefix (expression, "od"))
						return (EVAL_OP_MOD);
					else
						return (EVAL_OP_NONE);
			}
			--expression;
			return (EVAL_OP_NONE);
		case 'n':
		case 'N':
			++expression;
			switch (*expression)
			{
				case 'a':
				case 'A':
					if (string_prefix (expression, "amec"))
						return (EVAL_OP_NAMEC);
					else
						return (EVAL_OP_NONE);
				case 'c':
				case 'C':
					if (string_prefix (expression, "char"))
						return (EVAL_OP_NCHAR);
					else
						return (EVAL_OP_NONE);
				case 'u':
				case 'U':
					if (string_prefix (expression, "umitems"))
						return (EVAL_OP_NUMITEMS);
					else
						return (EVAL_OP_NONE);
				case 'i':
				case 'I':
					if (string_prefix (expression, "item"))
						return (EVAL_OP_NITEM);
					else
						return (EVAL_OP_NONE);
			}
			--expression;
			return (EVAL_OP_NONE);
		case 'p':
		case 'P':
			if (string_prefix (expression, "pi"))
				return (EVAL_OP_PI);
			else
				return (EVAL_OP_NONE);
		case 'r':
		case 'R':
			if (string_prefix(expression, "replace"))
				return(EVAL_OP_REPLACE);
			else
				return(EVAL_OP_NONE);
		case 's':
		case 'S':
			++expression;
			switch (*expression)
			{
				case 'q':
				case 'Q':
					if (string_prefix (expression, "qrt"))
						return (EVAL_OP_SQRT);
					else
						return (EVAL_OP_NONE);
				case 'i':
				case 'I':
					if (string_prefix (expression, "in"))
						return (EVAL_OP_SIN);
					else
						return (EVAL_OP_NONE);
				case 'u':
				case 'U':
					if (string_prefix (expression, "ubstr"))
						return (EVAL_OP_SUBSTR);
					else
						return (EVAL_OP_NONE);
			}
			--expression;
			return (EVAL_OP_NONE);
		case 't':
		case 'T':
			++expression;
			switch (*expression)
			{
				case 'a':
				case 'A':
					++expression;
					switch (*expression)
					{
						case 'i':
						case 'I':
							if (string_prefix (expression, "il"))
								return (EVAL_OP_TAIL);
							else
								return (EVAL_OP_NONE);
						case 'n':
						case 'N':
							if (string_prefix (expression, "n"))
								return (EVAL_OP_TAN);
							else
								return (EVAL_OP_NONE);
					}
					break;
				case 'y':
				case 'Y':
					++expression;
					if (string_prefix (expression, "peof"))
						return (EVAL_OP_TYPEOF);
					else
						return (EVAL_OP_NONE);
			}
			--expression;
			return (EVAL_OP_NONE);
		case '^':
			return (EVAL_OP_BITXOR);
		case '&':
			if (*++expression == '&')
				return (EVAL_OP_AND);
			else
				return (EVAL_OP_BITAND);
		case '|':
			if (*++expression == '|')
				return (EVAL_OP_OR);
			else
				return (EVAL_OP_BITOR);
		case '+':
			return (EVAL_OP_ADD);
		case '-':
			if(ops_found == 0)
			{
				if (isspace(*++expression))
					return (EVAL_OP_NEGATE);
				else
					return (EVAL_OP_NONE);
			}
			return (EVAL_OP_SUB);
		case '*':
			return (EVAL_OP_MUL);
		case '/':
			return (EVAL_OP_DIVISION);
		case '%':
			return (EVAL_OP_MOD);
		case '#':
			if (*(expression + 1) == '#')
			{
				++expression;
				return (EVAL_OP_CONCAT);
			}
			else
				return (EVAL_OP_NONE);
		case '<':
			++expression;
			switch (*expression)
			{
				case '>':
					return (EVAL_OP_NEQ);
				case '=':
					return (EVAL_OP_LE);
				default:
					--expression;
					return (EVAL_OP_LT);
			}
		case '>':
			++expression;
			switch (*expression)
			{
				case '<':
					return (EVAL_OP_NEQ);
				case '=':
					return (EVAL_OP_GE);
				default:
					--expression;
					return (EVAL_OP_GT);
			}
		case '!':
			++expression;
			switch (*expression)
			{
				case '=':
					return (EVAL_OP_NEQ);
				default:
					--expression;
					return (EVAL_OP_NOT);
			}
		case '=':
			return (EVAL_OP_EQ);
		default:
			return (EVAL_OP_NONE);
	}
}


static Command_status
full_frig_types (
	context		&c,
enum	eval_ops	op,
struct	operand_block* 	results,
	int		types)

{
	char	*end;

	if (op == EVAL_TYPE_NONE)
	{
		/* Already notified during parse */
		results->type = EVAL_TYPE_NONE;
		return (COMMAND_FAIL);
	}

	/* First of all, see if we can get a number out of the thing.  Try a float, as integers are a subset. */
	results->floating = strtod (results->string, &end);
	if (end == results->string || *end != '\0')
	{
		/* It's not any kind of number. */
		if (types & EV_STRING)
		{
			results->type = EVAL_TYPE_STRING;
			return (COMMAND_SUCC);
		}
		else
		{
			notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Can't do numeric operation on a string.");
			results->type = EVAL_TYPE_NONE;
			return (COMMAND_FAIL);
		}
	}
	else
	{
		/* We know it's a floating-point number.  If we can get an integer out of it, do so - otherwise, leave it. */
		results->type = EVAL_TYPE_FLOAT;

		results->integer = strtol (results->string, &end, 10);
		if (end != results->string && *end == '\0')
			results->type = EVAL_TYPE_NUMERIC;
		return (COMMAND_SUCC);
	}
}

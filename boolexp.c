/* static char SCCSid[] = "@(#)boolexp.c	1.23\t2/17/95"; */
#include "copyright.h"

#include <ctype.h>
#include <stdlib.h>

#include "db.h"
#include "match.h"
#include "externs.h"
#include "config.h"
#include "interface.h"
#include "command.h"
#include "context.h"
#include "colour.h"

static	int	commands_executed;


boolexp::boolexp (
boolexp_type t)
: type (t)
, sub1 (NULL)
, sub2 (NULL)
, thing (NOTHING)

{
}


boolexp::~boolexp ()

{
	if (sub1 != TRUE_BOOLEXP)
		delete sub1;
	if (sub2 != TRUE_BOOLEXP)
		delete sub2;
}


const Boolean
boolexp::eval_internal (
const	context	&c,
	Matcher	&matcher)
const

{
	int	i;

	if (this == TRUE_BOOLEXP)
		return True;
	else
		switch(type)
		{
			case BOOLEXP_AND:
				return sub1->eval_internal (c, matcher) && sub2->eval_internal (c, matcher);
			case BOOLEXP_OR:
				return sub1->eval_internal (c, matcher) || sub2->eval_internal (c, matcher);
			case BOOLEXP_NOT:
				return !sub1->eval_internal (c, matcher);
			case BOOLEXP_CONST:
				if (Typeof (thing) != TYPE_COMMAND)
					return (thing == c.get_effective_id () || ::contains_inherited(thing, c.get_player ()));
					//return (thing == c.get_effective_id () || ::contains(thing, c.get_player ()));
				if (!could_doit (c, thing))
					return False; /* Automatically fails */
				else
				{
					/* The idea is that the player still gets to see what's going on, but the command can't unchpid to the player... */
					context *sub_context = new context (c.get_player());
					sub_context->set_unchpid_id (db[thing].get_owner());
					sub_context->set_step_limit (COMPOUND_COMMAND_BASE_LIMIT - commands_executed);
					sub_context->do_compound_command (thing, ".lockcheck", "", "", db[thing].get_owner(), matcher);
					mud_scheduler.push_express_job (sub_context);
					commands_executed += sub_context->get_commands_executed ();
					Command_status status = sub_context->get_return_status ();
					delete sub_context;
					switch (status)
					{
						case COMMAND_SUCC:
							return True;
						case COMMAND_FAIL:
							return False;
						default:
							notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,"Command in lock returned invalid boolean value.");
							return (False);
					}
				}
			case BOOLEXP_FLAG:
				for (i=0; flag_list[i].string!=NULL; i++)
					if (flag_list[i].flag == thing)
						break;
				if (flag_list[i].string != NULL)
					return db[c.get_player ()].get_flag (thing);
				else
				{
					Trace("BUG: eval_internal - Bad flag number %d\n", thing);
					return False;
				}

				break; /* Unreachable, but just for neatness */
			default:
				Trace("BUG: eval_internal - Bad node type %d.\n", type);
				return False;
		}

	/* Should never get here */
	return False;
}


const Boolean
boolexp::eval (
const	context	&c,
	Matcher	&matcher)
const

{
	commands_executed = 0;
	return eval_internal (c, matcher);
}


/* If the parser returns TRUE_BOOLEXP, you lose */
/* TRUE_BOOLEXP cannot be typed in by the user; use @unlock instead */

static const char *parsebuf;

static void
skip_whitespace()
{
	while(*parsebuf && isspace(*parsebuf))
		parsebuf++;
}

boolexp	*parse_boolexp_E (dbref player);

/* F -> (E); F -> !F; F -> @F; F -> object identifier */

boolexp *
parse_boolexp_F (
const	dbref	player)

{
	boolexp	*b;
	char	*p;
	char	buf [BUFFER_LEN];
	int	i;

	skip_whitespace();
	switch(*parsebuf)
	{
		case '(':
			parsebuf++;
			b = parse_boolexp_E (player);
			skip_whitespace();
			if(b != TRUE_BOOLEXP && *parsebuf++ != ')')
			{
				delete (b);
				b = TRUE_BOOLEXP;
			}
			return (b);
			/* break; */
		case NOT_TOKEN:
			parsebuf++;
			b = new boolexp (BOOLEXP_NOT);
			b->sub1 = parse_boolexp_F (player);
			if(b->sub1 == TRUE_BOOLEXP)
			{
				delete (b);
				b = TRUE_BOOLEXP;
			}
			return b;
			/* break */
		case COMMAND_TOKEN:
			/* must have a flag name */
			/* load the name into our buffer */
			p = buf;
			parsebuf++;
			while(*parsebuf
				&& *parsebuf != AND_TOKEN
				&& *parsebuf != OR_TOKEN
				&& *parsebuf != ')')
			{
				*p++ = *parsebuf++;
			}
			/* strip trailing whitespace */
			*p-- = '\0';
			while(isspace(*p))
				*p-- = '\0';

			b = new boolexp (BOOLEXP_FLAG);

			for (i=0; flag_list[i].string!=NULL; i++)
				if (string_prefix (buf, flag_list[i].string))
				{
					b->thing = flag_list[i].flag;
					break;
				}

			if (flag_list[i].string==NULL)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unknown flag \"%s\".", buf);
				delete (b);
				return TRUE_BOOLEXP;
			}
			return b;
			/* break */
		default:
			/* must have hit an object ref */
			/* load the name into our buffer */
			p = buf;
			while(*parsebuf
				&& *parsebuf != AND_TOKEN
				&& *parsebuf != OR_TOKEN
				&& *parsebuf != ')')
			{
				*p++ = *parsebuf++;
			}
			/* strip trailing whitespace */
			*p-- = '\0';
			while(isspace(*p))
				*p-- = '\0';

			b = new boolexp (BOOLEXP_CONST);

			/* do the match */
			Matcher matcher (player, buf, TYPE_THING, player);
			matcher.match_everything ();
			if ((b->thing = matcher.noisy_match_result()) == NOTHING)
			{
				delete (b);
				return TRUE_BOOLEXP;
			}
			else
			{
				switch (Typeof (b->thing))
				{
					/* Check for right things that can be locked - Robin */
					case TYPE_THING:
					case TYPE_PLAYER:
					case TYPE_PUPPET:
					case TYPE_COMMAND:
						db [b->thing].set_referenced ();
						return b;
					default:
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s can't be part of a lock.", buf);
						delete (b);
						return TRUE_BOOLEXP;
				}
			}
		/* break */
	}
}

/* T -> F; T -> F & T */
boolexp *
parse_boolexp_T (
const	dbref	player)

{
	boolexp *b;
	boolexp *b2;

	if((b = parse_boolexp_F (player)) == TRUE_BOOLEXP)
		return b;
	else
	{
		skip_whitespace();
		if(*parsebuf != AND_TOKEN)
			return b;
		else
		{
			parsebuf++;

			b2 = new boolexp (BOOLEXP_AND);
			b2->sub1 = b;
			if((b2->sub2 = parse_boolexp_T (player)) == TRUE_BOOLEXP)
			{
				delete (b2);
				b2 = TRUE_BOOLEXP;
			}
			return b2;
		}
	}
}

/* E -> T; E -> T | E */
boolexp *
parse_boolexp_E (
const	dbref	player)

{
    boolexp *b;
    boolexp *b2;

    if((b = parse_boolexp_T (player)) == TRUE_BOOLEXP) {
	return b;
    } else {
	skip_whitespace();
	if(*parsebuf == OR_TOKEN) {
	    parsebuf++;

	    b2 = new boolexp (BOOLEXP_OR);
	    b2->sub1 = b;
	    if((b2->sub2 = parse_boolexp_E (player)) == TRUE_BOOLEXP) {
		delete (b2);
		return TRUE_BOOLEXP;
	    } else {
		return b2;
	    }
	} else {
	    return b;
	}
    }
}

boolexp *
parse_boolexp (
const	dbref	player,
const	char	*buf)

{
	parsebuf = buf;
	return (parse_boolexp_E (player));
}


const Boolean
boolexp::contains (
dbref	object)
const

{
	if (this == TRUE_BOOLEXP)
		return (False);
	switch (type)
	{
		case BOOLEXP_AND:
		case BOOLEXP_OR:
			return ((sub1->contains (object)) || (sub2->contains (object)));
		case BOOLEXP_NOT:
			return (sub1->contains (object));
		case BOOLEXP_CONST:
			return (thing == object);
		case BOOLEXP_FLAG:
			return (False);
	}

	/* Should always be one of the above types */
	Trace( "BUG: boolexp::contains: Invalid node type %d.\n", type);
	return (False);
}


static	char	boolexp_buf[BUFFER_LEN];
static	char	*buftop;


void
boolexp::unparse_internal (
	context		&c,
const	boolexp_type	outer_type)
const

{
	int i;

	if (this == TRUE_BOOLEXP)
	{
		strcpy(buftop, "*UNLOCKED*");
		buftop += strlen(buftop);
	}
	else
	{
		switch(type)
		{
			case BOOLEXP_AND:
				if(outer_type == BOOLEXP_NOT)
					*buftop++ = '(';
				sub1->unparse_internal (c, type);
				*buftop++ = AND_TOKEN;
				sub2->unparse_internal (c, type);
				if(outer_type == BOOLEXP_NOT)
					*buftop++ = ')';
				break;
			case BOOLEXP_OR:
				if(outer_type == BOOLEXP_NOT || outer_type == BOOLEXP_AND)
					*buftop++ = '(';
				sub1->unparse_internal (c, type);
				*buftop++ = OR_TOKEN;
				sub2->unparse_internal (c, type);
				if(outer_type == BOOLEXP_NOT || outer_type == BOOLEXP_AND)
					*buftop++ = ')';
				break;
			case BOOLEXP_NOT:
				*buftop++ = '!';
				sub1->unparse_internal (c, type);
				break;
			case BOOLEXP_CONST:
				strcpy(buftop, unparse_object(c, thing));
				buftop += strlen(buftop);
				break;
			case BOOLEXP_FLAG:
				*buftop++ = '@';
				for (i=0; flag_list[i].string != NULL; i++)
					if (thing == flag_list[i].flag)
						break;

				if (flag_list[i].string != NULL)
				{
					strcpy (buftop, flag_list[i].string);
					buftop += strlen (buftop);
				}
				else
				{
					Trace("BUG: unparse_internal : bad type %c\n", type);
				}
		}
	}
}


void
boolexp::unparse_for_return_internal (
	context		&c,
const	boolexp_type	outer_type)
const

{
	int i;

	if (this == TRUE_BOOLEXP)
	{
		strcpy(buftop, "*UNLOCKED*");
		buftop += strlen(buftop);
	}
	else
	{
		switch(type)
		{
			case BOOLEXP_AND:
				if(outer_type == BOOLEXP_NOT)
					*buftop++ = '(';
				sub1->unparse_for_return_internal (c, type);
				*buftop++ = AND_TOKEN;
				sub2->unparse_for_return_internal (c, type);
				if(outer_type == BOOLEXP_NOT)
					*buftop++ = ')';
				break;
			case BOOLEXP_OR:
				if(outer_type == BOOLEXP_NOT || outer_type == BOOLEXP_AND)
					*buftop++ = '(';
				sub1->unparse_for_return_internal (c, type);
				*buftop++ = OR_TOKEN;
				sub2->unparse_for_return_internal (c, type);
				if(outer_type == BOOLEXP_NOT || outer_type == BOOLEXP_AND)
					*buftop++ = ')';
				break;
			case BOOLEXP_NOT:
				*buftop++ = '!';
				sub1->unparse_for_return_internal (c, type);
				break;
			case BOOLEXP_CONST:
				strcpy(buftop, ::unparse_for_return (c, thing));
				buftop += strlen(buftop);
				break;
			case BOOLEXP_FLAG:
				*buftop++ = '@';
				for (i=0; flag_list[i].string!=NULL; i++)
					if (flag_list[i].flag == thing)
						break;

				if (flag_list[i].string!=NULL)
				{
					strcpy (buftop, flag_list[i].string);
					buftop += strlen(buftop);
				}
				else
				{
					Trace("BUG: unparse_for_return_internal - unknown type %c\n", type);
					abort();
				}
		}
	}
}


const char *
boolexp::unparse (
context	&c)
const

{
	buftop = boolexp_buf;
	unparse_internal (c, BOOLEXP_CONST);	/* no outer type */
	*buftop++ = '\0';

	return (boolexp_buf);
}


const char *
boolexp::unparse_for_return (
context	&c)
const

{
	buftop = boolexp_buf;
	unparse_for_return_internal (c, BOOLEXP_CONST);	/* no outer type */
	*buftop++ = '\0';

	return (boolexp_buf);
}


boolexp *
boolexp::sanitise ()

{
	boolexp	*copied_base;

	switch (type)
	{
		case BOOLEXP_CONST:
			if (Typeof (thing) == TYPE_FREE)
			{
				delete (this);
				return (TRUE_BOOLEXP);
			}
			return(this);
			/* break; */
		case BOOLEXP_AND:
		case BOOLEXP_OR:
			sub1 = sub1->sanitise ();
			sub2 = sub2->sanitise ();
			if (sub1 == TRUE_BOOLEXP)
			{
				copied_base = sub2;
				sub2 = TRUE_BOOLEXP;
				delete (this);
				return (copied_base);
			}
			else if (sub2 == TRUE_BOOLEXP)
			{
				copied_base = sub1;
				sub1 = TRUE_BOOLEXP;
				delete (this);
				return (copied_base);
			}
			return(this);
			/* break; */
		case BOOLEXP_NOT:
			if ((sub1 = sub1->sanitise ()) == TRUE_BOOLEXP)
			{
				delete (this);
				return (TRUE_BOOLEXP);
			}
			return (this);
			/* break; */
		case BOOLEXP_FLAG:
			return (this);
			break;
		default:
			/* bad type */
			Trace("BUG: recursive_sane_lock: bad type %d. Removing node and all sub-nodes.\n", type);
			return (TRUE_BOOLEXP);
	}

	/* Should never get here */
	return (TRUE_BOOLEXP);
}


void
boolexp::set_references ()
const

{
	switch (type)
	{
		case BOOLEXP_AND:
		case BOOLEXP_OR:
			sub2->set_references ();
			/* FALLTHROUGH */
		case BOOLEXP_NOT:
			sub1->set_references ();
			break;
		case BOOLEXP_CONST:
			db [thing].set_referenced ();
			break;
	}
}

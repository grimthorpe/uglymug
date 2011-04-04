/* static char SCCSid[] = "@(#)match.c	1.48\t7/27/95"; */
#include "copyright.h"

/* Routines for parsing arguments */
#include <ctype.h>

#include "db.h"
#include "config.h"
#include "colour.h"
#include "match.h"
#include "externs.h"
#include "interface.h"
#include "command.h"
#include "context.h"
#include "colour.h"
#include "log.h"

#define	ASSIGN_STRING(d,s)	{ if ((d)) free ((d));  if ((s && *s!=0)) (d)=strdup((s)); else (d)=NULL; }

/* Ok, this is a fix for the match code.
 * DO NOT FREAK OUT PLEASE.
 * The way it works: All the match code assumes a non-null name,
 * so, we give it a non-null name.
 * The reason for using strdup() is cos we free it at the end of matching.
 */
#define	TACKY_ASSIGN(d,s)	{ if ((d)) free ((d));  if ((s)) (d)=strdup((s)); else (d)=strdup(""); }


#define DOWNCASE(x) (isupper(x) ? tolower(x) : (x))


/*
 * matcher_fsm_array: Depending on the path, this lists the sequence of states the matcher has to go through.
 *
 * Check out Matcher_path and Matcher_state definitions in match.h.
 */

Matcher_state matcher_fsm_array [] [11] =
{
/* MATCHER_PATH_NONE */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_COMMAND_FULL */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_COMMAND_PLAYER,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_COMMAND_PCONTENTS,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_COMMAND_LOCATION,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_COMMAND_LCONTENTS,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_COMMAND_AREA,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_COMMAND_LAST,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_FINISHED,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_COMMAND_PLAYER */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_COMMAND_PLAYER,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_FINISHED,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_COMMAND_REMOTE */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_COMMAND_PLAYER,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_FINISHED,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_COMMAND_BOUNDED */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_FINISHED,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_COMMAND_FROM_LOC */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_COMMAND_LAST,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_FINISHED,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_FUSE_THING */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_FUSE_THING,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_FINISHED,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_INVALID
	},
/* MATCHER_PATH_FUSE_AREA */
	{
		/* MATCHER_STATE_INVALID		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FINISHED		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_SETUP			-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PLAYER		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_PCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LOCATION	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LCONTENTS	-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_AREA		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_COMMAND_LAST		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_THING		-> */ MATCHER_STATE_INVALID,
		/* MATCHER_STATE_FUSE_AREA		-> */ MATCHER_STATE_FINISHED
	}
};


Matcher_state	matcher_state_array [] =
{
/* MATCHER_PATH_NONE			*/ MATCHER_STATE_INVALID,
/* MATCHER_PATH_COMMAND_FULL		*/ MATCHER_STATE_SETUP,
/* MATCHER_PATH_COMMAND_PLAYER		*/ MATCHER_STATE_SETUP,
/* MATCHER_PATH_COMMAND_REMOTE		*/ MATCHER_STATE_SETUP,
/* MATCHER_PATH_COMMAND_BOUNDED		*/ MATCHER_STATE_COMMAND_AREA,
/* MATCHER_PATH_COMMAND_FROM_LOC	*/ MATCHER_STATE_COMMAND_AREA,
/* MATCHER_PATH_FUSE_THING		*/ MATCHER_STATE_SETUP,
/* MATCHER_PATH_FUSE_AREA		*/ MATCHER_STATE_FUSE_AREA
};


Matcher::Matcher (
dbref			player,
const	String&	name,
typeof_type	type,
dbref			effective_player)

: exact_match			(NOTHING)
, checking_keys			(false)
, first_match			(NOTHING)
, last_match			(NOTHING)
, match_count			(0)
, effective_who			(effective_player)
, real_who			(player)
, match_name			(name)
, preferred_type		(type)
, index				(NULLSTRING)
, index_attempt			(false)
, absolute			(NOTHING)
, gagged			(false)
, absolute_loc			(NOTHING)
, beginning			(name)
, already_checked_location	(false)
, path				(MATCHER_PATH_NONE)
, current_state			(MATCHER_STATE_INVALID)
, internal_restart		(NOTHING)
, internal_inheritance_restart	(NOTHING)
, location			(NOTHING)
, thing				(NOTHING)
, end_location			(NOTHING)
{
	/* Cache the absolute location, since it's used by so many routines */
	absolute = absolute_name();
	if (!controls_for_read (real_who, absolute, effective_who) &&
	   !controls_for_read (real_who, absolute, db[real_who].get_build_id()))
		absolute = NOTHING;

	/* Find the object's location (if supplied) */
	int colon;
	while((colon=beginning.find(':')) != -1)
	{
		if(colon == 0)
		{
			beginning = beginning.substring(1);
		}
		else
		{
			int openindex = beginning.find('[');
			int closeindex = beginning.find(']');
			if((openindex != -1) && (openindex < colon)
				&& (closeindex != -1) && (closeindex < colon))
			{
				break;
			}

			String locationname = beginning.substring(0, colon);

			Matcher	nesting_matcher (player, locationname, TYPE_NO_TYPE, effective_player);
			nesting_matcher.absolute_loc = absolute_loc;
			nesting_matcher.match_everything ();
			absolute_loc = nesting_matcher.match_result ();
			if((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
			{
				beginning = beginning.substring(colon + 1);
			}
			else
			{
				/* Zap AMBIGUOUS mentions, to ease checking */
				absolute_loc = NOTHING;
				break;
			}
		}
	}

	int openindex = beginning.find('[');
	int closeindex = beginning.find(']');

	if((openindex != -1) && (closeindex != -1) && (openindex < closeindex))
	{
		match_name = beginning.substring(0, openindex);
		index = beginning.substring(openindex + 1, closeindex - openindex - 1);
		index_attempt = true;
		match_absolute();
	}
	else
	{
		match_name = beginning;
	}

#ifdef MATCH_DEBUG
	log_message("abs: %d, p: %s, string: %s, beg: %s", absolute_loc, p, match_name, beginning);
#endif
}


Matcher::Matcher (
dbref			player,
dbref			object,
typeof_type	type,
dbref			effective_player)

: exact_match			(NOTHING)
, checking_keys			(false)
, first_match			(NOTHING)
, last_match			(NOTHING)
, match_count			(0)
, effective_who			(effective_player)
, real_who			(player)
, match_name			("")
, preferred_type		(type)
, index				(NULLSTRING)
, index_attempt			(false)
, absolute			(NOTHING)
, gagged			(false)
, absolute_loc			(object)
, beginning			(NULLSTRING)
, already_checked_location	(false)
, path				(MATCHER_PATH_NONE)
, current_state			(MATCHER_STATE_INVALID)
, internal_restart		(NOTHING)
, internal_inheritance_restart	(NOTHING)
, location			(NOTHING)
, thing				(NOTHING)
, end_location			(NOTHING)

{
}


Matcher::Matcher (
const	Matcher	&src)

: exact_match			(src.exact_match)
, checking_keys			(src.checking_keys)
, first_match			(src.first_match)
, last_match			(src.last_match)
, match_count			(src.match_count)
, effective_who			(src.effective_who)
, real_who			(src.real_who)
, match_name			(src.match_name)
, preferred_type		(src.preferred_type)
, index				(src.index)
, index_attempt			(src.index_attempt)
, absolute			(src.absolute)
, gagged			(src.gagged)
, absolute_loc			(src.absolute_loc)
, beginning			(src.beginning)
, already_checked_location	(src.already_checked_location)
, path				(src.path)
, current_state			(src.current_state)
, internal_restart		(src.internal_restart)
, internal_inheritance_restart	(src.internal_inheritance_restart)
, location			(src.location)
, thing				(src.thing)
, end_location			(src.end_location)

{
}


Matcher::~Matcher ()

{
}


void
Matcher::check_keys ()

{
	checking_keys = true;
}


/**
 * Given two possibilities, return the better one. Checks:
 *	- If either is NOTHING, the other one is better,
 *	- If both are variables, ownership by the effective player is better,
 *	- If one is of type preferred_type and the other isn't, the one
 *		of preferred_type is better,
 *	- If keys are being checked then if one's key is satisfied and the
 *		other isn't, pick the one whose key is satisfied,
 *	- Otherwise pick the one < type.
 */

dbref
Matcher::choose_thing (
dbref	thing1,
dbref	thing2)

{
	int	has1;
	int	has2;

	if(thing1 == NOTHING)
		return thing2;
	else if(thing2 == NOTHING)
		return thing1;

	if ((Typeof (thing1) == TYPE_VARIABLE) && (Typeof (thing2) == TYPE_VARIABLE))
	{
		if ((db [thing1].get_owner() == effective_who) && (db [thing2].get_owner() != effective_who))
			return (thing1);
		else if ((db [thing1].get_owner() != effective_who) && (db [thing2].get_owner() == effective_who))
			return (thing2);
	}

	if(preferred_type != TYPE_NO_TYPE)
	{
		if (Typeof(thing1) == preferred_type)
		{
			if (Typeof(thing2) != preferred_type)
				return thing1;
		}
		else if (Typeof(thing2) == preferred_type)
			return thing2;
	}

	if(checking_keys)
	{
		context	check_context	(real_who, context::DEFAULT_CONTEXT);

		has1 = could_doit (check_context, thing1);
		has2 = could_doit (check_context, thing2);

		if (has1 && !has2)
			return thing1;
		else if (has2 && !has1)
			return thing2;
		/* else fall through */
	}

	if ((thing1 == first_match) || (thing2 == first_match))
		return (first_match);

	if (Typeof (thing1) <= Typeof (thing2))
		return (thing1);
	return (thing2);
}


void
Matcher::match_player ()

{
	dbref		match;
	const	char	*p;

	if (match_name.c_str()[0] == LOOKUP_TOKEN && payfor(effective_who, LOOKUP_COST))
	{
		/* Trim leading white space */
		for(p = match_name.c_str() + 1; isspace(*p); p++)
			;

		if((match = lookup_player(real_who, p)) != NOTHING)
			exact_match = match;
	}
}

/**
 * Attempts to find an index in the string
 *	and then to search for the array or dictionary.  Testing
 *	whether the item has that index is not being done here.
 */

 void
 Matcher::match_array_or_dictionary()
 {
 /*	char	*begin;
  *	char	*end;
  *	dbref	temp;
  */

	if((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
		match_variable_list (absolute_loc);
	else
		match_variable_list (real_who);

}



/**
 * Return nnn if name = #nnn, else NOTHING.
 */

dbref
Matcher::absolute_name ()

{
	dbref	match;

	if(match_name.c_str()[0] == NUMBER_TOKEN)
	{
		match = parse_dbref(match_name.c_str()+1);
		if ((match >= 0) && (match < db.get_top ()) && (db + match != NULL))
			return match;
	}
	return NOTHING;
}


/**
 * Test for an exact match by #id.  Configurable as to whether it limits who can do this.
 */

void
Matcher::match_absolute ()

{
	/* Use the cache if it exists */
	if(absolute != NOTHING)
		exact_match = absolute;
	else
	{
		dbref	match;

		if (((match = absolute_name ()) != NOTHING)
#ifdef	ID_MATCH_NEEDS_CONTROL_FOR_READ
			&& (controls_for_read (real_who, match, effective_who)
				|| Abode (match)
				|| Jump (match)
				|| Link (match))
#endif	/* ID_MATCH_NEEDS_CONTROL_FOR_READ */
			)
			exact_match = match;
	}
}


void
Matcher::match_me ()

{
	if(!string_compare(match_name, "me"))
		exact_match = real_who;
}


void
Matcher::match_here ()

{
	if(!string_compare(match_name, "here") && db[real_who].get_location() != NOTHING)
		exact_match = db[real_who].get_location();
}


void
Matcher::match_list (
dbref	first)

{
	/* Bombproof when matching crap. PJC, 13/5/96 */
	if (first == AMBIGUOUS || first == HOME)
		return;
	DOLIST(first, first)
	{
		if(first == absolute)
		{
			exact_match = first;
			if (first_match == NOTHING)
				first_match = first;
			return;
		}
#ifdef ALIASES
		else if((!string_compare(db[first].get_inherited_name(), match_name)) || (db[first].has_alias(match_name)))
#else
		else if(!string_compare(db[first].get_inherited_name(), match_name))
#endif
		{
			/* if there are multiple exact matches, choose one */
			exact_match = choose_thing(exact_match, first);
		}
#ifdef ALIASES
		else if((string_match(db[first].get_inherited_name(), match_name)) || (db[first].has_alias(match_name)))
#else
		else if(string_match(db[first].get_inherited_name(), match_name))
#endif
		{
			last_match = first;
			if ((first_match == NOTHING) && (match_count == 0))
				first_match = first;
			if ((first_match != NOTHING) && (string_compare(db[first].get_inherited_name(), db[first_match].get_inherited_name())))
				first_match = NOTHING;

			match_count++;
		}

		/* If the first is open or not transparent then recurse */
		if (Container (first) && ((Open (first)) || (!Opaque (first))))
			match_contents_list (first);
	}
}


void
Matcher::match_list_variable (
dbref	first)

{
	DOREALLIST (first, first)
        {
		if (!Dark(first) || controls_for_read (effective_who, first, effective_who));
		{
			if (semicolon_string_match (db [first].get_inherited_name (), match_name))
			{
				exact_match = first;
				return;
			}
		}
        }
}


void
Matcher::match_possession ()

{
	if((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
		match_contents_list (absolute_loc);
	else
		match_contents_list (real_who);
}

void
Matcher::match_neighbor ()

{
	dbref	loc;

	if ((loc = db[real_who].get_location()) == NOTHING)
		return;
	match_contents_list (loc);
}

void
Matcher::match_neighbor_remote (dbref loc)

{

	if (loc == NOTHING)
		return;
	match_contents_list (loc);
}


/*
 * match_fuse_or_alarm: Match:
 *	- on player
 *	- in player's location
 *	- on exits in location
 */

void
Matcher::match_fuse_or_alarm ()
{
	dbref	loc;
	dbref	exit;

	if ((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
		match_fuse_list (absolute_loc);
	else
	{
		if ((loc = db[real_who].get_location()) == NOTHING)
			return;
		match_fuse_list (real_who);
		if (exact_match != NOTHING)
			return;
		match_fuse_list (loc);
		if (exact_match != NOTHING)
			return;
		/** This does NOT match fuses on inherited exits **/
		DOLIST (exit, db[loc].get_exits())
		{
			match_fuse_list (exit);
			if (exact_match != NOTHING)
				return;
		}
	}
}


void
Matcher::match_exit ()

{
	if((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
		match_exit_remote (absolute_loc);
	else
		match_exit_remote (db[real_who].get_location());
}


void
Matcher::match_exit_remote (
dbref	loc)

{
	dbref		exit;

	int type = Typeof(loc);

	if((type != TYPE_ROOM)
		&& (type != TYPE_THING)
		&& (type != TYPE_PLAYER)
		&& (type != TYPE_PUPPET))
	{
		return; // Bug out now before we crash the game
	}
	while (loc != NOTHING)
	{
		DOLIST (exit, db[loc].get_exits())
		{
			if (exit == absolute)
				exact_match = exit;
			else
				if (semicolon_string_match (db [exit].get_inherited_name(), match_name))
					exact_match = choose_thing (exact_match, exit);
		}
		if (exact_match != NOTHING)
			return;
		loc = db [loc].get_parent ();
	}
}


/*
 * match_command_internal: returns true if it found something, false if not.
 */

bool
Matcher::match_command_internal ()

{
	while (internal_inheritance_restart != NOTHING)
	{
		if (internal_restart == NOTHING)
		{
			dbref tmp;
			tmp = db[internal_inheritance_restart].find_command(match_name);
			if(tmp != AMBIGUOUS)
			{
				internal_inheritance_restart = db[internal_inheritance_restart].get_parent();
				if(tmp == NOTHING)
				{
					continue;
				}
				exact_match = tmp;
				return true;
			}
			// If we get here then we've found an AMBIGUOUS entry.

			internal_restart = db [internal_inheritance_restart].get_commands ();
		}
		DOLIST (internal_restart, internal_restart)
		{
			if (semicolon_string_match (db[internal_restart].get_inherited_name(), match_name))
			{
				exact_match = internal_restart;
				return (true);
			}
		}
		internal_inheritance_restart = db [internal_inheritance_restart].get_parent ();
	}

	return (false);
}


/*
 * match_fuse_internal: returns true if it found something, false if not.
 */

bool
Matcher::match_fuse_internal ()

{
	while (internal_inheritance_restart != NOTHING)
	{
		if (internal_restart == NOTHING)
			internal_restart = db [internal_inheritance_restart].get_fuses ();
		DOLIST (internal_restart, internal_restart)
		{
			if (Typeof (internal_restart) == TYPE_FUSE)
			{
				exact_match = internal_restart;
				return (true);
			}
		}
		internal_inheritance_restart = db [internal_inheritance_restart].get_parent ();
	}

	return (false);
}


/*
 * match_player_command: Look only at player's commands.
 */

void
Matcher::match_player_command ()

{
	path = MATCHER_PATH_COMMAND_PLAYER;
	match_set_state ();
	match_continue ();
}


void
Matcher::match_count_down_fuse ()

{
	path = (Typeof (absolute_loc) == TYPE_ROOM) ? MATCHER_PATH_FUSE_AREA : MATCHER_PATH_FUSE_THING;
	match_set_state ();

	/* Strictly, we only need this for MATCHER_PATH_FUSE_AREA. PJC 5/1/95 */
	location = absolute_loc;

	match_continue ();
}


void
Matcher::match_bounded_area_command (
dbref	start_location,
dbref	end_location)

{
	path = MATCHER_PATH_COMMAND_BOUNDED;
	match_set_state ();
	location = start_location;
	end_location = end_location;
	match_continue ();
}

void
Matcher::match_command_from_location (
dbref	start_location)

{
	path = MATCHER_PATH_COMMAND_FROM_LOC;
	match_set_state ();
	location = start_location;
	end_location = NOTHING;
	match_continue ();
}

void
Matcher::match_command ()

{
	path = MATCHER_PATH_COMMAND_FULL;
	match_set_state ();
	match_continue ();
}


void
Matcher::match_command_remote ()

{
	/* Limit the command search */
	path = MATCHER_PATH_COMMAND_REMOTE;
	match_set_state ();
	match_continue ();
}


void
Matcher::match_restart ()

{
	/* Set up other fields */
	match_count = 0;
	exact_match = NOTHING;
	last_match = NOTHING;
	first_match = NOTHING;

	/* Increment past the lowest valid field */
	if (internal_restart != NOTHING)
		internal_restart = db [internal_restart].get_next();
	if (internal_restart == NOTHING && internal_inheritance_restart != NOTHING)
		internal_inheritance_restart = db [internal_inheritance_restart].get_parent ();
	if (internal_restart == NOTHING && internal_inheritance_restart == NOTHING)
	{
		if (thing != NOTHING && ((current_state == MATCHER_STATE_COMMAND_PCONTENTS) || (current_state == MATCHER_STATE_COMMAND_LCONTENTS)))
			thing = db [thing].get_next();
		else if ((location != NOTHING) && ((current_state == MATCHER_STATE_COMMAND_AREA) || (current_state == MATCHER_STATE_FUSE_AREA)))
			location = db [location].get_location();
	}

	/* OK, go for it */
	match_continue ();
}


/*
 * match_continue: Matches for commands. Tries:
 *	- The player's commands,
 *	- The player's possessions' commands,
 *	- The location's commands,
 *	- The location's contents' commands.
 *	- Areas' commands, immediate superior first,
 *	- COMMAND_LAST_RESORT's commands.
 *
 * Yes, it's incredibly messy. It's also fully restartable, which is why it's so messy.
 */

void
Matcher::match_continue ()

{
	while (current_state > MATCHER_STATE_FINISHED)
	{
		switch (current_state)
		{
			case MATCHER_STATE_SETUP:
				if((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
					internal_inheritance_restart = absolute_loc;
				else
					internal_inheritance_restart = real_who;
				internal_restart = NOTHING;
				thing = internal_inheritance_restart;
				break;
			case MATCHER_STATE_COMMAND_PLAYER:
				/* Match on the player */
				if (match_command_internal ())
					return;
				thing = db [real_who].get_contents();
				break;
			case MATCHER_STATE_COMMAND_PCONTENTS:
				/* Match on the player's contents list */
				DOLIST (thing, thing)
				{
					if (internal_restart == NOTHING && internal_inheritance_restart == NOTHING)
						internal_inheritance_restart = thing;
					if (match_command_internal ())
						return;
				}
				location = db[real_who].get_location();

				/*
				 * Bombproofing: Give up if there's no location.
				 */
				if (location == NOTHING)
				{
					current_state = MATCHER_STATE_FINISHED;
					continue;
				}
				else
				{
					thing = location;
					internal_inheritance_restart = location;
					internal_restart = NOTHING;
				}
				break;
			case MATCHER_STATE_COMMAND_LOCATION:
				/* Match on the location */
				if (location != NOTHING)
					if (match_command_internal ())
						return;
				already_checked_location = true;
				thing = db [location].get_contents();
				break;
			case MATCHER_STATE_COMMAND_LCONTENTS:
				/* Match on the location's contents (THINGs only, to stop you executing another player's commands) */
				DOLIST (thing, thing)
					if (Typeof (thing) == TYPE_THING)
					{
						if (internal_restart == NOTHING && internal_inheritance_restart == NOTHING)
							internal_inheritance_restart = thing;
						if (match_command_internal ())
							return;
					}
				thing = location;
				break;
			case MATCHER_STATE_COMMAND_AREA:
				/* match commands in the area */
				while ((location != NOTHING) && (location != end_location))
				{
					if (Typeof (location) == TYPE_ROOM)
					{
						if (internal_restart == NOTHING && internal_inheritance_restart == NOTHING)
						{
							/* Gross hack: If we've done phase 3, don't check the same thing again. */
							/* Remember to reset the location checker so we don't keep skipping levels */
							if (already_checked_location)
							{
								already_checked_location = false;
								internal_inheritance_restart = db [location].get_location ();
							}
							else
								internal_inheritance_restart = location;
						}
						if (match_command_internal ())
								return;
					}
					location = db[location].get_location();
				}
				
				thing = COMMAND_LAST_RESORT;
				internal_inheritance_restart = thing;
				internal_restart = NOTHING;
				break;
			case MATCHER_STATE_COMMAND_LAST:
				/* COMMAND_LAST_RESORT */
				if (match_command_internal ())
					return;
				break;
			case MATCHER_STATE_FUSE_THING:
				/* Match on the thing (and parents) only */
				if (match_fuse_internal ())
					return;
				location = db [absolute_loc].get_location ();
				break;
			case MATCHER_STATE_FUSE_AREA:
				/* match commands in the area */
				while ((location != NOTHING) && (location != end_location))
				{
					/* Remember the thing we're supposed to be answering if someone asks us */
					thing = location;
					if (Typeof (location) == TYPE_ROOM)
					{
						if (internal_restart == NOTHING && internal_inheritance_restart == NOTHING)
						{
							/* Gross hack: If we've checked the location, don't check the same thing again. */
							/* Remember to reset the location checker so we don't keep skipping levels */
							if (already_checked_location)
							{
								already_checked_location = false;
								internal_inheritance_restart = db [location].get_location ();
							}
							else
								internal_inheritance_restart = location;
						}
						if (match_fuse_internal ())
								return;
					}
					location = db[location].get_location();
				}
				break;
			default:
				/* Oops... */
				notify_wizard ("Error in command search - state out of range.");
				log_bug("Error in command search - state %d out of range", current_state);
		}
		current_state = matcher_fsm_array [path] [current_state];
	}
}


void
Matcher::match_variable ()
{
	if((absolute_loc != NOTHING) && (absolute_loc != AMBIGUOUS))
		match_variable_list (absolute_loc);
	else
		match_variable_list (real_who);

}

void
Matcher::match_variable_remote (dbref loc)
{
	match_variable_list(loc);
}

void
Matcher::match_everything ()
{
	if(absolute_loc == NOTHING)
	{
		match_me	();
		match_here	();
		match_absolute	();
		if(exact_match == NOTHING)
		{
			match_exit	();
			match_neighbor	();
			match_possession();
			match_player();
		}
		if (exact_match == NOTHING)
			match_command();
		if (exact_match == NOTHING)
			match_array_or_dictionary();
		if (exact_match == NOTHING)
			match_fuse_or_alarm();
		if ((exact_match == NOTHING) && (match_count == 0))
			match_variable();
	}
	else
	{
		match_exit_remote(absolute_loc);
		match_thing_remote(absolute_loc);
		match_absolute();
		if (exact_match == NOTHING)
			match_command_remote (/* Checks in match_command_internal() */);
		if (exact_match == NOTHING)
			match_array_or_dictionary (/* Already uses absolute_loc */);
		if (exact_match == NOTHING)
			match_fuse_or_alarm (/* Already uses absolute_loc */);
		if ((exact_match == NOTHING) && (match_count == 0))
			match_variable (/* Already uses absolute_loc */);
	}
}


void
Matcher::match_thing_remote(dbref loc)
{
	match_contents_list (loc);
}


const String
Matcher::match_index_result()

{
	return(index);
}

bool
Matcher::match_index_attempt_result()

{
	return(index_attempt);
}


dbref
Matcher::match_result ()

{
	if(exact_match != NOTHING)
		return exact_match;
	else
	{
		switch(match_count)
		{
			case 0:
				return NOTHING;
			case 1:
				if (first_match == NOTHING)
					return last_match;
				return first_match;
			default:
				if (first_match == NOTHING)
					return AMBIGUOUS;
				return first_match;
		}
	}
}
		   

/*
 * use this if you don't care about ambiguity
 */

dbref
Matcher::last_match_result ()

{
	if(exact_match != NOTHING)
		return exact_match;
	else
		return last_match;
}


dbref
Matcher::noisy_match_result ()

{
	dbref	match;

	switch(match = match_result())
	{
		case NOTHING:
			if (gagged == false)
				notify_colour(real_who,real_who, COLOUR_MESSAGES, "I don't see '%s' here.", value_or_empty (match_name.c_str()));
			return NOTHING;
		case AMBIGUOUS:
			if (gagged == false)
				notify_colour(real_who, real_who, COLOUR_MESSAGES, AMBIGUOUS_MESSAGE);
			return NOTHING;
		default:
			return match;
	}
}


/*
 * Helper routines. These are all private to class Matcher.
 */

void
Matcher::match_contents_list (
dbref	base)

{
	while (base != NOTHING)
	{
		match_list (db [base].get_contents ());
		if (exact_match != NOTHING)
			break;
		else
			base = db [base].get_parent ();
	}
}


void
Matcher::match_fuse_list (
dbref	base)

{
	while (base != NOTHING)
	{
		match_list (db [base].get_fuses ());
		if (exact_match != NOTHING)
			break;
		else
			base = db [base].get_parent ();
	}
}


void
Matcher::match_variable_list (
dbref	base)

{
	while (base != NOTHING)
	{
		match_list_variable (db [base].get_info_items ());
		if (exact_match != NOTHING)
			break;
		else
			base = db [base].get_parent ();
	}
}


void
context::do_query_my (
const	String& type,
const	String& arg)

{
	/** For now, assume type == "leaf" **/
	unsigned int		pos;
	Matcher			*matcher = (Matcher *) NULL;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!arg)
		pos = 0;
	else
	{
		int level = atoi (arg.c_str());
		if(level < 0)
			pos = call_stack.size() + level;
		else
			pos = level;
	}

	if (pos >= call_stack.size ())
	{
		if (!gagged_command())
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@?my: Not that many levels on stack");
	}
	else
	{
		// Find the matcher that we're asking.  In the common case, this
		// is the topmost frame's matcher, so there's an optimisation.
		if (pos == 0)
		{
			if ((matcher = call_stack.top ()->get_matcher ()) == NULL)
				return;
		}
		else
		{
			// Copy the stack...
			Call_stack copied_call_stack (call_stack);

			// ... and pop the appropriate number of frames off it so that the top frame is the one we want.
			while (pos > 0)
			{
				copied_call_stack.pop ();
				--pos;
			}
			if ((matcher = copied_call_stack.top ()->get_matcher ()) == NULL)
				return;
		}

		return_status = COMMAND_SUCC;
		if (!string_compare (type, "match"))
			set_return_string (unparse_for_return (*this, matcher->match_result ()));
		else if (!string_compare (type, "leaf"))
			set_return_string (unparse_for_return (*this, matcher->get_leaf ()));
		else if(!string_compare (type, "self"))
			set_return_string (unparse_for_return (*this, get_current_command()));
		else if(!string_compare (type, "name"))
			set_return_string(matcher->get_match_name());
		else
		{
			if (!gagged_command())
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@?my: '%s' is not a valid type.", type.c_str());
			return_status = COMMAND_FAIL;
			set_return_string (error_return_string);
		}
	}
}

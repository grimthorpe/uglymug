/* static char SCCSid[] = "@(#)game_predicates.c	1.34\t9/25/95"; */
#include "copyright.h"

/* Predicates for testing various conditions */

#include <string.h>
#include <ctype.h>
#include <math.h>

#include "match.h"
#include "db.h"
#include "interface.h"
#include "colour.h"
#include "config.h"
#include "context.h"
#include "externs.h"
#include "game_predicates.h"

#define	valid_char(c)	((isalnum(c) || ((c) == '_')) && (c != '\n'))

const	char	*fit_errlist [] =
	{
		"If you see this message, something is very wrong.",		/* SUCCESS */
		"it won't take the extra weight.",				/* NO_MASS */
		"it isn't big enough.",						/* NO_VOLUME */
		"it isn't a container.",					/* NOT_CONTAINER */
		"I have absolutely no idea why - see a wizard!"			/* NO_REASON */
	};


/* match_player_type: just matches those things for who and @?num connected and stuff.
 *
 * Return values are in game_predicates.h
 */

int
match_player_type(
const	CString& text)
{
	if(string_prefix("admin",text))
		return T_ADMIN;
	else if(string_prefix("mortal",text))
		return T_MORTAL;
	else if(string_prefix("builder",text))
		return T_BUILDER;
	else if(string_prefix("xbuilder",text))
		return T_XBUILDER;
	else if(string_prefix("wizard",text))
		return T_WIZARD;
	else if(string_prefix("apprentice",text))
		return T_APPRENTICE;
	else if(string_prefix("welcomer",text))
		return T_WELCOMER;
	else if(string_prefix("god",text))
		return T_GOD;
	else
		return T_NONE;
}


/*
 * could_doit: Checks to see whether or not a player can get an object or go
 *	through an exit.
 *
 * Return value:
 *	1	if it is feasible,
 *	0	if it isn't.
 */

const Boolean
could_doit (
const	context	&c,
const	dbref	thing)

{
	/* If it is a NOTHING then we can't do it. */
	if (thing == NOTHING)
		return (False);

	/* Can't traverse unlinked exits */
	if (Typeof (thing) == TYPE_EXIT && db [thing].get_destination () == NOTHING)
		return (False);

	/* Can't do it if it's locked against you */
	Matcher matcher (c.get_player (), thing, TYPE_NO_TYPE, c.get_player ());
	matcher.set_my_leaf (thing);
	return (db [thing].get_inherited_key ()->eval (c, matcher));
}


/*
 * can_doit: Calls could_doit to find out if the player can {get an object,
 *	go through an exit}. Depending on the answer, prints success or fail
 *	messages.
 *
 * Return value:
 *	0	if couldn't do it,
 *	1	if could do it.
 */

const Boolean
can_doit (
const	context	&c,
const	dbref	thing,
const	char	*default_fail_msg)

{
	dbref	loc;
	char	buf[BUFFER_LEN];

	if ((loc = getloc(c.get_player ())) == NOTHING)
		return 0;

	if(!could_doit(c, thing))
	{
		/* can't do it */

		if(db[thing].get_inherited_fail_message())
			notify_public_colour(c.get_player (), c.get_player(), COLOUR_FAILURE, "%s", db[thing].get_inherited_fail_message().c_str());
		else if(default_fail_msg)
			notify_public_colour(c.get_player (), c.get_player(), COLOUR_FAILURE, default_fail_msg);
		
		if(db[thing].get_inherited_ofail() && !Dark(c.get_player ()))
		{
			pronoun_substitute(buf, BUFFER_LEN, c.get_player (), db[thing].get_inherited_ofail ());
			notify_except(db[loc].get_contents(), c.get_player(), c.get_player (), buf);
		}

		return 0;
	}
	else
	{
		/* can do it */
		if(db[thing].get_inherited_succ_message())
			notify_public_colour(c.get_player (), c.get_player(), COLOUR_SUCCESS, "%s", db[thing].get_inherited_succ_message().c_str());

		if(db[thing].get_inherited_osuccess() && !Dark(c.get_player ()))
		{
			pronoun_substitute(buf, BUFFER_LEN, c.get_player (), db[thing].get_inherited_osuccess());
			notify_except(db[loc].get_contents(), c.get_player(), c.get_player (), buf);
		}

		return 1;
	}
}

Boolean
can_see (
context	&c,
dbref	thing,
Boolean	can_see_loc)

{
	if(c.get_player () == thing || Typeof(thing) == TYPE_EXIT)
		return 0;
	
	else	if(Typeof(thing) == TYPE_THING)
		{
			if (db[c.get_player ()].get_flag(FLAG_NUMBER) && Dark(thing))
				return 0;
			else
				return ((db[db[thing].get_location()].get_owner() == c.get_player ()) || (can_see_loc && (!Dark(thing) || c.controls_for_read(thing) || (db[db[thing].get_location()].get_owner() != db[thing].get_owner()))));
		}
	else if((Typeof(thing) == TYPE_PLAYER) && Dark(thing))
		return 1;
	else if(can_see_loc)
		return(!Dark(thing) || c.controls_for_read(thing) || (db[db[thing].get_location()].get_owner() != db[thing].get_owner()));
	else
		/* can't see loc */
		return(c.controls_for_read(thing));
}


/*
 * payfor: Wizards can pay for anything, others can only pay for
 *	something if they have enough pennies.
 *
 * NOTE: This updates the pennies field to indicate that the player
 *	has paid for it.
 *
 * Ammended 27/1/96 by Cyric:
 *	Now XBuilders, Apprentices, Wizards and God can go negative and
 *	still build.
 *
 * Return value:
 *	1	if paid for,
 *	0	if not.
 */

int
payfor (
dbref	player,
int	cost)

{
	if ((db[player].get_pennies() >= cost) || (player == GOD_ID) || (Wizard (player)) || (Apprentice(player)) || (XBuilder(player)))
	{
		db[player].set_pennies (db [player].get_pennies () - cost);
		return 1;
	}
	else
		return 0;
}

Boolean
ok_name (
const	CString&name)

{
	return (name
		&& *name.c_str()
		&& *name.c_str() != LOOKUP_TOKEN
		&& *name.c_str() != NUMBER_TOKEN
		&& string_compare(name, "me")
		&& string_compare(name, "home")
		&& string_compare(name, "here"));
}


Boolean
ok_puppet_name (
const	CString& name)

{
	const	char	*scan;

	if(!ok_name(name))
		return (False);

	if (name.length() > 20)
		return (False);

	for(scan = name.c_str(); *scan; scan++)
		if(!(valid_char(*scan) || isspace(*scan)))
			return (False);

	return (True);
}


Boolean
ok_player_name (
const	CString& name)

{
	if (!ok_puppet_name (name))
		return (False);

	/* lookup name to avoid conflicts */
	return (lookup_player(NOTHING, name) == NOTHING);
}


int
ok_password (
const	CString& password)

{
	if (!password || !*password.c_str())
		return 0;
	return 1;
}


#ifdef ALIASES
int
ok_alias_string (
	dbref	/* player */,
const	CString& string)
{
	const	char	*scan;
	dbref		test;

	if (!string || (string.length() < MIN_ALIAS_LENGTH) || (string.length() > MAX_ALIAS_LENGTH))
		return 0;

	for (scan = string.c_str(); *scan; scan++)
		if ((!isprint (*scan)) || (*scan == '=') || (*scan == '\n') || (isspace(*scan)))
			return 0;

	/* lookup name to avoid conflicts */
	if ((test = lookup_player(NOTHING, string)) == NOTHING)
		return -1;
	return test;

}
#endif /* ALIASES */


int
ok_who_string (
	dbref	/* player */,
const	CString& string)
{
	const	char	*scan;

	if (string.length() > MAX_WHO_STRING)
		return 0;

	for (scan = string.c_str(); *scan; scan++)
		if ((!isprint (*scan)) || (*scan == '\n') )
			return 0;
	return 1;
}


int
can_reach (
dbref	player,
dbref	thing)

{
	/* Keep going till we get to the player's location, out of the universe, or we hit a snag */
	while (thing != db[player].get_location() && thing != NOTHING)
	{
		if (db[thing].get_location() != NOTHING && Typeof (db[thing].get_location()) == TYPE_THING && !Open (db[thing].get_location()))
			return (0);
		thing = db[thing].get_location();
	}
	return (thing != NOTHING);
}


enum fit_result
will_fit (
dbref victim,
dbref destination)

{
	static	double	victim_volume;
	static	double	victim_mass;
	static	double	destination_volume;
	static	double	destination_mass;
	static	double	contents_of_dest_volume;
	static	double	contents_of_dest_mass;
	static	double	destination_volume_limit;
	static	double	destination_mass_limit;
	static	double	destination_gravity_factor;

	if (destination == HOME)
		destination = db[victim].get_destination ();
	if (destination == NOTHING)
		return SUCCESS;

	/* It'll fit where is already is */
	if (db[victim].get_location () == destination)
		return SUCCESS;

	victim_volume			= find_volume_of_object (victim);
	victim_mass			= find_mass_of_object (victim);
	destination_volume		= find_volume_of_object (destination);
	destination_mass		= find_mass_of_object (destination);
	destination_volume_limit	= find_volume_limit_of_object (destination);
	destination_mass_limit 		= find_mass_limit_of_object (destination);
	destination_gravity_factor	= db[destination].get_inherited_gravity_factor ();

	switch( Typeof (victim))
	{
		case TYPE_THING:
			switch( Typeof (destination))
			{
				case TYPE_THING :
					if (!Container (destination))
						return NOT_CONTAINER;
				case TYPE_ROOM :
				case TYPE_PLAYER :
					contents_of_dest_volume		= find_volume_of_contents (destination);
					contents_of_dest_mass		= find_mass_of_contents_except (destination, NOTHING);
					if ((destination_volume_limit != HUGE_VAL)
					&& (contents_of_dest_volume + victim_volume > destination_volume_limit))
						return NO_VOLUME;
					if ((destination_mass_limit != HUGE_VAL)
					&& (destination_gravity_factor != 0)
					&& (contents_of_dest_mass + (victim_mass * destination_gravity_factor) > destination_mass_limit))
						return NO_MASS;
					if (destination_gravity_factor != 0)
						victim_mass *= destination_gravity_factor;
					while (((destination = getloc (destination)) != NOTHING) && (db[destination].get_inherited_gravity_factor () != 0))
					{
						victim_mass *= db[destination].get_inherited_gravity_factor ();
						if (((destination_mass_limit = find_mass_limit_of_object (destination)) != HUGE_VAL)
						&& (find_mass_of_contents_except (destination, victim) + victim_mass > destination_mass_limit))
						{
							return NO_MASS;
						}
					}
					break;
				default :
					return NO_REASON;
			}
			break;
		case TYPE_ROOM:
			switch(Typeof(destination))
			{
				case TYPE_ROOM:
					break;
				case TYPE_THING:
				case TYPE_PLAYER:
				default:
					return NO_REASON;
			}
			break;
		case TYPE_PLAYER:
			switch(Typeof(destination))
			{
				case TYPE_THING:
					if (!Container(destination))
						return NOT_CONTAINER;
				case TYPE_ROOM:
					contents_of_dest_volume		= find_volume_of_contents (destination);
					contents_of_dest_mass		= find_mass_of_contents_except (destination, NOTHING);
					if ((destination_volume_limit != HUGE_VAL)
					&& (contents_of_dest_volume + victim_volume > destination_volume_limit))
						return NO_VOLUME;
					if ((destination_mass_limit != HUGE_VAL)
					&& (destination_gravity_factor != 0)
					&& (contents_of_dest_mass + (victim_mass * destination_gravity_factor) > destination_mass_limit))
						return NO_MASS;
					victim_mass *= destination_gravity_factor;
					while (((destination = getloc (destination)) != NOTHING) && (db[destination].get_inherited_gravity_factor () != 0))
					{
						victim_mass *= db[destination].get_inherited_gravity_factor ();
						if (((destination_mass_limit = find_mass_limit_of_object (destination)) != HUGE_VAL)
						&& (find_mass_of_contents_except (destination, victim) + victim_mass > destination_mass_limit))
						{
							return NO_MASS;
						}
					}
					break;
				case TYPE_PLAYER:
				default:
					return NO_REASON;
                        }
		default:
			return SUCCESS;
	}
	return SUCCESS;
}

/* These probably shouldn't be here, but they're related to the will_fit bit */
/* and I can't be bothered finding somewhere else to put them */

double
find_volume_limit_of_object(
dbref thing)
{
	switch (Typeof (thing))
	{
		case TYPE_ROOM :
		case TYPE_THING :
		case TYPE_PLAYER :
			return (db[thing].get_inherited_volume_limit ());
			break;
		default:
			return (0);
	}
}

double
find_mass_limit_of_object(
dbref thing)
{
	switch (Typeof (thing))
	{
		case TYPE_ROOM :
		case TYPE_THING :
		case TYPE_PLAYER :
			return (db[thing].get_inherited_mass_limit ());
			break;
		default:
			return (0);
	}
}

double
find_volume_of_object(
dbref thing)
{
	switch(Typeof(thing))
	{
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_PLAYER:
			return(db[thing].get_inherited_volume ());
			break;
		default:
			return(0);
	}
}

double
find_mass_of_object(
dbref thing)
{
	switch(Typeof(thing))
	{
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_PLAYER:
			return(db[thing].get_inherited_mass ());
			break;
		default:
			return(0);
	}
}

double
find_volume_of_contents(
dbref thing)

{
	double	total_volume = 0;
	dbref	looper;

	if (thing == NOTHING)
		return (0);
	DOLIST(looper, db[thing].get_contents())
	{
		switch (Typeof(looper))
		{
			case TYPE_PLAYER :	/* player's take up their volume and what they're carrying */
				total_volume += find_volume_of_contents(looper);
			case TYPE_THING :
			case TYPE_ROOM :
				total_volume += db[looper].get_inherited_volume ();
				break;
			default :
				break;
		}
	}
	return(total_volume);
}

double
find_mass_of_contents_except (
dbref thing,
dbref exception)

{ 
	double	total_mass = 0;
	double	partial_mass = 0;
	dbref	looper;
	
	if ((thing == NOTHING) || (db[thing].get_inherited_gravity_factor () == 0))
		return (0);

	DOLIST(looper, db[thing].get_contents())
	{
		if (exception != looper)
		{
			partial_mass = db[thing].get_inherited_gravity_factor ();
			switch (Typeof(looper))
			{
				case TYPE_THING :
				case TYPE_PLAYER :
				case TYPE_ROOM :
					partial_mass *= db[looper].get_inherited_mass () + find_mass_of_contents_except (looper, exception); 
				default:
					break;
			}
			total_mass += partial_mass;
		}
	}
	return(total_mass); 
}

Boolean
abortable_command (
const	char	*command)
{
	while (*command && isspace (*command))
		command++;
	switch (*command)
	{
		case	'|':
			return (False);
		case	'h':
		case	'H':
			/* We want to check that the command is 'home<NULL>'	*/
			/*	 else, we allow it to be aborted.		*/
			return (!((*(++command) == 'o' || *command == 'O')
				&& ((*(++command) == 'm' || *command == 'M'))
				&& ((*(++command) == 'e' || *command == 'E'))
				&& ((*(++command) == '\0'))));
		default:
			return (True);
	}
}

Boolean
is_guest (
dbref player)
{
        int guest_match;
	char element[BUFFER_LEN];
       	sprintf(element, "#%d", player);
	Matcher matcher (GOD_ID, "guest_dictionary", TYPE_DICTIONARY, GOD_ID);
	matcher.match_variable_remote(COMMAND_LAST_RESORT);
	matcher.match_array_or_dictionary();
	if ((guest_match = matcher.match_result()) == NOTHING)
        {
                Trace( "BUG: Cannot find guest_dictionary in #4\n");
                return (False);
        }
        else
        {
                if (db[guest_match].exist_element(element))
                        return (True);
                else
                        return (False);
        }
}


/*
 * do_query_can: Can the current player do a 'read' or 'write'
 * on the named object?
 */

void
context::do_query_can(const CString& object, const CString& type)
{
	Matcher matcher(player, object, TYPE_NO_TYPE, get_effective_id());
	matcher.match_everything();
	dbref target = matcher.match_result();

	if(NOTHING == target)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I don't see '%s' here.", object.c_str());
		RETURN_FAIL;
	}
	else if(AMBIGUOUS == target)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Ambiguous");
		RETURN_FAIL;
	}

	if(string_compare(type, "read") == 0)
	{
		if(controls_for_read(target))
		{
			RETURN_SUCC;
		}
	}
	else if(string_compare(type, "write") == 0)
	{
		if(controls_for_write(target))
		{
			RETURN_SUCC;
		}
	}
	else if(string_compare(type, "link") == 0)
	{
		if(can_link_to(*this, target) ||
			((Typeof(target) == TYPE_ROOM)
			 && (Link(target) || Abode(target))))
		{
			RETURN_SUCC;
		}
	}
	else if(string_compare(type, "jump") == 0)
	{
		if(((Typeof(target) == TYPE_ROOM)
				&& (controls_for_read(target) 
					|| Abode(target)
					|| Jump(target)))
		 || ((Typeof(target) == TYPE_THING)
				&& controls_for_write(target)))
		{
			RETURN_SUCC;
		}
	}
	else
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unknown @?can type: '%s'. Should be read, write, link or jump", type.c_str());
	}
	RETURN_FAIL;
}


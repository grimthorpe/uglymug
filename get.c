/* static char SCCSid[] = "@(#)get.c	1.50\t7/27/95"; */
#include "copyright.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "colour.h"
#include "command.h"
#include "context.h"
#include "game_predicates.h"

#include "config.h"
#include "log.h"


extern	"C"	long	lrand48 ();


/** Not the neatest solution, but quick.  PJC 1/2/97 **/
static	char	scratch_return_string [BUFFER_LEN];


void
context::do_query_numconnected(
const	String& argument,
const	String& )
{
	int	what;

	if(argument)
	{
		what = match_player_type(argument);
		if(what == 0)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That is not a valid player type.");
			RETURN_FAIL;
		}
	}
	else
		what = 0;

	sprintf(scratch_return_string, "%d", count_connect_types(what));

	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}


static dbref
find_for_query (
	context	&c,
const	String& name,
	int	need_control)

{
	dbref	result;

	if (!name)
		return (c.get_player ());
	Matcher matcher (c.get_player (), name, TYPE_NO_TYPE, c.get_effective_id ());
	if (c.gagged_command())
		matcher.work_silent ();
	matcher.match_everything ();
	result = matcher.match_result ();
	if ((result != NOTHING) && (result != AMBIGUOUS) && need_control && !c.controls_for_read (result))
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return (NOTHING);
	}
	else
		return (matcher.noisy_match_result ());
}


static dbref
find_executor_for_query (
context		&c,
const	String& name,
int		need_control)

{
	dbref	thing;

	if (!name)
		thing = c.get_player ();
	else
		thing = find_for_query (c, name, need_control);

	if ((thing != NOTHING) && (Typeof (thing) != TYPE_PLAYER) && (Typeof (thing) != TYPE_PUPPET))
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That's not a Player.");
		return (NOTHING);
	}
	return (thing);
}


static dbref
find_player_for_query (
	context	&c,
const	String& name,
	int	need_control)

{
	dbref	thing;

	if (!name)
		thing = c.get_player ();
	else
		thing = find_for_query (c, name, need_control);

	if ((thing != NOTHING) && (Typeof (thing) != TYPE_PLAYER))
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,  "That's not a Player.");
		return (NOTHING);
	}
	return (thing);
}


#if 0
static dbref
find_thing_for_query (
	context	&c,
const	String& name,
	int	need_control)

{
	dbref	thing = find_for_query (c, name, need_control);
	if ((thing != NOTHING) && (Typeof (thing) != TYPE_THING))
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES,  "That's not a Thing.");
		return (NOTHING);
	}
	return (thing);
}
#endif

static dbref
find_anything_with_csucc_link_for_query (
	context	&c,
const	String&  name,
	int	need_control)

{
	dbref	thing = find_for_query (c, name, need_control);
	if ((thing != NOTHING) && (Typeof (thing) != TYPE_COMMAND) && (Typeof (thing) != TYPE_ALARM) && (Typeof (thing) != TYPE_FUSE))
	{
		notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That cannot be linked to commands..");
		return (NOTHING);
	}
	return (thing);
}


void
context::do_query_address (
const	String& name,
const	String& )

{
	dbref loc;
	dbref thing;
	int already_had_one = 0;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if ((thing = find_for_query (*this, name, 0)) != NOTHING)
	{
		if (Typeof(thing) != TYPE_ROOM)
			notify_colour(player, player, COLOUR_MESSAGES, "You can only find the address on rooms");
		else
		{
			*scratch_return_string = '\0';
			loc = db[thing].get_location();
			while ((loc != NOTHING) && (strlen (scratch_return_string) < BUFFER_LEN))
			{
				if (already_had_one)
					strcat (scratch_return_string, ", ");
				strcat (scratch_return_string, unparse_object (*this, loc));
				loc = db[loc].get_location();
				already_had_one = 1;
			}

			Accessed(loc);
			set_return_string (scratch_return_string);
			return_status = COMMAND_SUCC;
		}
	}
}


void
context::do_query_area (const String& name, const String& the_area)
{
	dbref area;
	dbref thing;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if ((area = find_for_query (*this, the_area, 0)) == NOTHING)
		return;
	if ((thing = find_for_query (*this, name, 0)) == NOTHING)
		return;

	Accessed(thing);

	if (in_area (thing, area))
	{
		set_return_string (ok_return_string);
		return_status = COMMAND_SUCC;
	}
}


void
context::do_query_article (const String& name, const String& type)
{
	dbref   thing = find_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

        if (thing == NOTHING)
		return;

	if (!((Typeof (thing) == TYPE_ROOM)
	|| (Typeof (thing) == TYPE_EXIT)
	|| (Typeof (thing) == TYPE_THING)
	|| (Typeof (thing) == TYPE_PLAYER)
	|| (Typeof (thing) == TYPE_PUPPET)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Things, Exits, Rooms, Players and NPCs can have articles.");
		return;
	}

	if (!type)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You must specify an article type.");
		return;
	}

	if (! ( string_compare(type, "lower indefinite") && string_compare(type, "li")))
		set_return_string (getarticle (thing, ARTICLE_LOWER_INDEFINITE));
	else if (! (string_compare(type, "lower definite") && string_compare(type, "ld")))
		set_return_string (getarticle (thing, ARTICLE_LOWER_DEFINITE));
	else if (! (string_compare(type, "upper indefinite") && string_compare(type, "ui")))
		set_return_string (getarticle (thing, ARTICLE_UPPER_INDEFINITE));
	else if (! (string_compare(type, "upper definite") && string_compare(type, "ud")))
	        set_return_string (getarticle (thing, ARTICLE_UPPER_DEFINITE));
	else
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@?article: '%s' is not a valid option.", type.c_str());
		return;
	}

	Accessed(thing);
	return_status=COMMAND_SUCC;
	return;
}


void
context::do_query_cfail (const String& name, const String&)
{
	dbref	thing = find_anything_with_csucc_link_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	Accessed(thing);
	set_return_string (unparse_for_return (*this, db[thing].get_exits()));
	return_status = COMMAND_SUCC;
}

void
context::do_query_commands (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	else
	{
		switch(Typeof(thing))
		{
			case TYPE_ROOM:
			case TYPE_THING:
			case TYPE_PLAYER:
			case TYPE_PUPPET:
				Accessed(thing);
				set_return_string (unparse_for_return (*this, db[thing].get_commands()));
				return_status = COMMAND_SUCC;
				break;

			default:
				notify_colour(player, player, COLOUR_MESSAGES, "Only Rooms, Things and Players can have commands.");
				set_return_string (error_return_string);
				return_status = COMMAND_FAIL;
				break;
		}
	}
}


void
context::do_query_connected (
const	String& name,
const	String&)

{
	dbref victim = find_executor_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (victim == NOTHING)
		return;

	Accessed (victim);

	if (Connected (victim))
	{
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_query_contents (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	else
	{
		switch(Typeof(thing))
		{
			case TYPE_ROOM:
			case TYPE_THING:
			case TYPE_PLAYER:
				thing = db[thing].get_contents();
				Accessed (thing);
				set_return_string (unparse_for_return (*this, thing));
				if(thing != NOTHING)
					return_status = COMMAND_SUCC;
				break;
			default:
				notify_colour(player, player, COLOUR_MESSAGES, "Only Rooms, Things and Players have a contents list.");
				set_return_string (error_return_string);
				return_status = COMMAND_FAIL;
				break;
		}
	}
}


void
context::do_query_controller (const String& name, const String&)
{
	dbref thing = find_executor_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (unparse_for_return (*this, db [thing].get_controller ()));
}

void
context::do_query_cstring (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);

	if ((thing == NOTHING) || (Typeof(thing) != TYPE_ROOM) && (Typeof(thing) != TYPE_THING))
	{
		if (thing != NOTHING)
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Rooms and Things have contents strings.");
		set_return_string (error_return_string);
		return_status = COMMAND_FAIL;
		return;
	}
	Accessed (thing);
	set_return_string (db[thing].get_contents_string ().c_str());
	return_status = COMMAND_SUCC;
}

void
context::do_query_csucc (const String& name, const String&)
{
	dbref	thing = find_anything_with_csucc_link_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	set_return_string (unparse_for_return(*this, db[thing].get_contents()));
	return_status = COMMAND_SUCC;
}


void
context::do_query_descendantfrom (const String& first, const String& parent_str)
{
	dbref	thing, parent, temp;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	Matcher	thing_matcher (player, first, TYPE_NO_TYPE, get_effective_id ());
	Matcher	parent_matcher (player, parent_str, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
	{
		thing_matcher.work_silent ();
		parent_matcher.work_silent ();
	}
	thing_matcher.match_everything ();
	parent_matcher.match_everything ();
	if ((thing = thing_matcher.noisy_match_result ()) == NOTHING)
		return;
	if ((parent = parent_matcher.noisy_match_result ()) == NOTHING)
		return;

	return_status = COMMAND_SUCC;
	do {
		if ((temp = db[thing].get_parent ()) != NOTHING)
		{
			do {
				if (temp == parent)
				{
					set_return_string (unparse_for_return (*this, thing));
					return;
				}
			} while ((temp = db[temp].get_parent ()) != NOTHING);
		}
	} while ((thing = db[thing].get_next ()) != NOTHING);

	set_return_string (unparse_for_return (*this, NOTHING));
}


void
context::do_query_description (const String& name, const String&)
{
	int	value;
	dbref	thing;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything ();
	if ((thing = matcher.noisy_match_result ()) == NOTHING)
		return;

	switch(Typeof(thing))
	{
		case TYPE_COMMAND:
			/* Kludge to stop people looking at command descriptions */
			if((!controls_for_read (thing)) && Opaque(thing))
				return;
			if(matcher.match_index_attempt_result())
			{
				if((value = db[thing].exist_element (atoi (matcher.match_index_result().c_str()))) > 0)
				{
					set_return_string (db[thing].get_element(value).c_str());
					return_status = COMMAND_SUCC;
				}
				else
				{
					if (!gagged_command())
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Command only has %d lines", db[thing].get_number_of_elements());
					set_return_string (error_return_string);
					return_status = COMMAND_FAIL;
				}
			}
			else
			{
				set_return_string (db[thing].get_description().c_str());
				return_status = COMMAND_SUCC;
			}
			break;
		case TYPE_DICTIONARY:
			if(matcher.match_index_attempt_result())
				if((value = db[thing].exist_element(matcher.match_index_result())))
				{
					set_return_string (db[thing].get_element(value).c_str());
					return_status = COMMAND_SUCC;
				}
				else
				{
					if (!gagged_command())
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Dictionary doesn't contain element \"%s\"", matcher.match_index_result().c_str());
					set_return_string (error_return_string);
					return_status = COMMAND_FAIL;
				}
			else
			{
				if (!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Dictionary doesn't contain element \"%s\"", matcher.match_index_result().c_str());
				set_return_string (error_return_string);
				return_status = COMMAND_FAIL;
			}
			break;

		case TYPE_ARRAY:
			if(matcher.match_index_attempt_result())
			{
				if((value = db[thing].exist_element (atoi (matcher.match_index_result().c_str()))) > 0)
				{
					set_return_string (db[thing].get_element(value).c_str());
					return_status = COMMAND_SUCC;
				}
				else
				{
					if (!gagged_command())
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Array only has %d elements", db[thing].get_number_of_elements());
					set_return_string (error_return_string);
					return_status = COMMAND_FAIL;
				}
			}
			else
			{
				if (!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Array only has %d elements", db[thing].get_number_of_elements());
				set_return_string (error_return_string);
				return_status = COMMAND_FAIL;
			}
			break;
		default:
			set_return_string (db[thing].get_description().c_str());
			return_status = COMMAND_SUCC;
			break;
	}

	Accessed (thing);
}

void
context::do_query_destination(const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing != NOTHING)
	{
		Accessed (thing);
		set_return_string (unparse_for_return (*this, db[thing].get_destination()));
		return_status = COMMAND_SUCC;
	}
	else
	{
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
	}
}


void
context::do_query_drop (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof(thing) == TYPE_VARIABLE)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning: @?drop used with a variable. This is a deprecated command");
		if(in_command()) {
			log_bug("@?drop used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
		}
		else {
			log_bug("@?drop used with variable %s(#%d) by player %s(#%d) on the command line",
					getname(thing), thing,
					getname(player), player);
		}
	}
	Accessed (thing);
	set_return_string (db[thing].get_drop_message().c_str());
	return_status = COMMAND_SUCC;
}


void
context::do_query_elements (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	else
	{
		switch(Typeof(thing))
		{
			case TYPE_DICTIONARY:
				if(db[thing].get_number_of_elements() > 0)
					set_return_string (db[thing].get_index(1).c_str());
				else
					set_return_string ("NOTHING");

				Accessed (thing);
				return_status = COMMAND_SUCC;
				break;
			case TYPE_ARRAY:
				if(db[thing].get_number_of_elements() > 0)
					set_return_string ("1");
				else
					set_return_string ("NOTHING");

				Accessed (thing);
				return_status = COMMAND_SUCC;
				break;
			default:
				notify_colour(player, player, COLOUR_MESSAGES, "Only Arrays and Dictionaries have element lists.");
				break;
		}
	}
}


void
context::do_query_email (const String& name, const String&)
{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if ((victim=find_executor_for_query (*this, name, 1)) == NOTHING)
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That player does not exist.");
	else
	{
		if (!Wizard (player))
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		else
		{
			Accessed (victim);
			return_status = COMMAND_SUCC;
			set_return_string (db[victim].get_email_addr ().c_str());
		}
	}
}

			
void
context::do_query_exist (const String& name, const String& owner_string)
{
	dbref	owner;
	dbref	result;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if (!owner_string)
	{
		Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
		if (gagged_command ())
			matcher.work_silent ();
		matcher.match_everything ();
		if (((result = matcher.match_result ()) != NOTHING) && (result != AMBIGUOUS))
		{
			switch(Typeof(result))
			{
				case TYPE_DICTIONARY:
					if(matcher.match_index_attempt_result())
					{
						if(db[result].exist_element(matcher.match_index_result()))
						{
							set_return_string (ok_return_string);
							return_status = COMMAND_SUCC;
						}
					}
					else
					{
						set_return_string (ok_return_string);
						return_status = COMMAND_SUCC;
					}
					break;

				case TYPE_ARRAY:
					if(matcher.match_index_attempt_result())
					{
						if(db[result].exist_element (atoi (matcher.match_index_result().c_str())) > 0)
						{
							set_return_string (ok_return_string);
							return_status = COMMAND_SUCC;
						}
					}
					else
					{
						set_return_string (ok_return_string);
						return_status = COMMAND_SUCC;
					}
					break;
				
				default:
					set_return_string (ok_return_string);
					return_status = COMMAND_SUCC;
					break;
			}

			Accessed (result);
		}
		if (result == AMBIGUOUS)
			set_return_string ("Ambiguous");
	}
	else
	{
		if ((owner = lookup_player (player, owner_string)) == NOTHING)
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That player doesn't exist.");
		else
		{
			Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
			if (gagged_command ())
				matcher.work_silent ();
			matcher.match_everything ();
			result = matcher.match_result ();
			if ((result != NOTHING) && (result != AMBIGUOUS) && (db[result].get_owner() == owner))
			{
				switch(Typeof(result))
				{
					case TYPE_DICTIONARY:
						if(matcher.match_index_attempt_result())
						{
							if(db[result].exist_element(matcher.match_index_result()))
							{
								set_return_string (ok_return_string);
								return_status = COMMAND_SUCC;
							}
						}
						else
						{
							set_return_string (ok_return_string);
							return_status = COMMAND_SUCC;
						}
						break;

					case TYPE_ARRAY:
						if(matcher.match_index_attempt_result())
						{
							if(db[result].exist_element (atoi (matcher.match_index_result().c_str())) > 0)
							{
								set_return_string (ok_return_string);
								return_status = COMMAND_SUCC;
							}
						}
						else
						{
							set_return_string (ok_return_string);
							return_status = COMMAND_SUCC;
						}
						break;
					
					default:
						set_return_string (ok_return_string);
						return_status = COMMAND_SUCC;
						break;
				}

				Accessed (result);
			}
			if (result == AMBIGUOUS)
				set_return_string ("Ambiguous");
		}
	}
}

void
context::do_query_exits (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	set_return_string (unparse_for_return (*this, db[thing].get_exits()));
	return_status = COMMAND_SUCC;
}


void
context::do_query_fail (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof(thing) == TYPE_VARIABLE)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning: @?fail used with a variable. This is a deprecated command");
		if (in_command()) {
			log_bug("@?fail used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
		}
		else {
			log_bug("@?fail used with variable %s(#%d) by player %s(#%d) on the command line",
					getname(thing), thing,
					getname(player), player);
		}
	}
	Accessed (thing);
	set_return_string (db[thing].get_fail_message().c_str());
	return_status = COMMAND_SUCC;
}

void
context::do_query_first_name (const String& name, const String&)
{
	char	*name_end;

	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if ((Typeof (thing) != TYPE_EXIT) && (Typeof (thing) != TYPE_COMMAND))
		set_return_string (getname (thing));
	else
	{
		strcpy (scratch_return_string, getname (thing));
		if ((name_end = strchr (scratch_return_string, ';')) != NULL)
			*name_end = '\0';
		set_return_string (scratch_return_string);
	}
	Accessed (thing);
	return_status = COMMAND_SUCC;
}

void
context::do_query_fuses (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	thing = db[thing].get_fuses();

	set_return_string (unparse_for_return (*this, thing));
	return_status = COMMAND_SUCC;
}

void
context::do_query_gravity_factor (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	switch (Typeof (thing))
	{
		case TYPE_PLAYER:
		case TYPE_ROOM:
		case TYPE_THING:
			sprintf (scratch_return_string, "%.9g", db[thing].get_inherited_gravity_factor ());
			break;
		default:
			return;
	}
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void
context::do_query_id (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	set_return_string (unparse_for_return (*this, thing));
	return_status = COMMAND_SUCC;
}


void
context::do_query_idletime (const String& name, const String&)
{
	time_t	interval;
	dbref   thing = find_player_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if ((interval = get_idle_time (thing)) == -1)
	{
		return;
	}
	sprintf (scratch_return_string, "%ld", (long int)interval);
	Accessed (thing);
	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}


void
context::do_query_interval (const String& name, const String&)
{
	time_t	interval;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (!name)
		return;
	interval = atol (name.c_str());
	set_return_string (time_string (interval));
	return_status = COMMAND_SUCC;
}


void
context::do_query_key (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof (thing) != TYPE_THING)
	{
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
	}
	else
	{
		Accessed (thing);
		set_return_string (db[thing].get_lock_key ()->unparse_for_return (*this));
		return_status = COMMAND_SUCC;
	}
}

void
context::do_query_last_entry (const String& name, const String&)
{
	dbref	thing;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if ((thing = find_for_query (*this, name, 0)) != NOTHING)
	{
		if (Typeof(thing) != TYPE_ROOM)
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only find the last entry time of a room.");
		else
		{
			sprintf (scratch_buffer, "%ld", (long int)db[thing].get_last_entry_time());
			Accessed (thing);
			set_return_string (scratch_buffer);
			return_status = COMMAND_SUCC;
		}
	}
}

void
context::do_query_location (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if(thing == NOTHING)
		return;
	if (!controls_for_read(db[thing].get_location()) && !controls_for_read(thing))
		return;
	Accessed (thing);
	set_return_string (unparse_for_return (*this, db[thing].get_location()));
	return_status = COMMAND_SUCC;
}

void
context::do_query_lock (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	set_return_string (db[thing].get_key ()->unparse_for_return (*this));
	return_status = COMMAND_SUCC;
}

void
context::do_query_mass (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	switch (Typeof (thing))
	{
		case TYPE_PLAYER:
		case TYPE_ROOM:
		case TYPE_THING:
			sprintf (scratch_return_string, "%.9g", db[thing].get_inherited_mass ());
			break;
		default:
			return;
	}
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void
context::do_query_mass_limit (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	switch (Typeof (thing))
	{
		case TYPE_PLAYER:
		case TYPE_ROOM:
		case TYPE_THING:
			sprintf (scratch_return_string, "%.9g", db[thing].get_inherited_mass_limit ());
			break;
		default:
			return;
	}
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void 
context::do_query_money (const String& name, const String&)
{
	dbref victim = find_for_query(*this, name, 1);

	set_return_string (error_return_string);
        return_status = COMMAND_FAIL;
	if (victim == NOTHING)
		return;
	switch(Typeof (victim))
	{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			sprintf(scratch_return_string, "%d", db[victim].get_money());
			break;
		default:
			return;
	}
	Accessed (victim);
	return_status = COMMAND_SUCC;
        set_return_string (scratch_return_string);
}

void
context::do_query_myself (
const	String&,
const	String&)

{
	if (in_command())
	{
		return_status = COMMAND_SUCC;
		set_return_string (unparse_for_return (*this, get_current_command ()));
	}
	else
	{
		return_status = COMMAND_FAIL;
		set_return_string (error_return_string);
	}
}


void
context::do_query_name (const String& name, const String& type)
{
	dbref	thing = find_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if (thing == NOTHING)
		return;

	Accessed (thing);

	if (!type)	/* Do they want an article too? */
	{
		set_return_string (getname (thing));
		return_status = COMMAND_SUCC;
		return;
	}

	if (!((Typeof (thing) == TYPE_ROOM)
	|| (Typeof (thing) == TYPE_EXIT)
	|| (Typeof (thing) == TYPE_THING)
	|| (Typeof (thing) == TYPE_PLAYER)
	|| (Typeof (thing) == TYPE_PUPPET)))
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Only Things, Exits, Rooms, Players and NPCs can have articles.");
		return;
	}
	

	if (! ( string_compare(type, "lower indefinite") && string_compare(type, "li")))
		strcpy(scratch_return_string, getarticle (thing, ARTICLE_LOWER_INDEFINITE));
	else if (! (string_compare(type, "lower definite") && string_compare(type, "ld")))
		strcpy(scratch_return_string, getarticle (thing, ARTICLE_LOWER_DEFINITE));
	else if (! (string_compare(type, "upper indefinite") && string_compare(type, "ui")))
		strcpy(scratch_return_string, getarticle (thing, ARTICLE_UPPER_INDEFINITE));
	else if (! (string_compare(type, "upper definite") && string_compare(type, "ud")))
	        strcpy(scratch_return_string, getarticle (thing, ARTICLE_UPPER_DEFINITE));
	else
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "@?name: '%s' is not a valid option.", type.c_str());
		return;
	}

	strcat (scratch_return_string, getname (thing));

	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
	return;
}

void
context::do_query_next (const String& name, const String&)
{
	int	value;
	dbref	result;
	dbref	thing;

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;

	if (!name)
		thing = get_player ();
	else
	{
		Matcher matcher (get_player (), name, TYPE_NO_TYPE, get_effective_id ());
		if (gagged_command ())
			matcher.work_silent ();
		matcher.match_everything ();
		result = matcher.match_result ();
		if ((result != NOTHING) && (result != AMBIGUOUS) && matcher.match_index_attempt_result() && !controls_for_read (result))
		{
			notify_colour (get_player (), get_player(), COLOUR_ERROR_MESSAGES, permission_denied.c_str());
			thing = NOTHING;
		}
		else
			thing = matcher.noisy_match_result ();

		if(thing == NOTHING)
			return;

		switch(Typeof(thing))
		{
			case TYPE_DICTIONARY:
				if(matcher.match_index_attempt_result())
				{
					if((value = db[thing].exist_element(matcher.match_index_result())))
					{
						if((int)db[thing].get_number_of_elements() >= value + 1)
						{
							return_status = COMMAND_SUCC;
							set_return_string (db[thing].get_index (value + 1).c_str());
						}
						else
						{
							return_status = COMMAND_FAIL;
							set_return_string ("NOTHING");
						}
					}
					else
					{
						notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Dictionary doesn't contain element \"%s\".", matcher.match_index_result().c_str());
					}
					return;
				}
				else // Not an index, get the 'right' next.
				{
					do
					{
						thing = db[thing].get_next();
					}
					while(thing != NOTHING && Typeof(thing) != TYPE_DICTIONARY);
					set_return_string (unparse_for_return (*this, thing));
					if(thing != NOTHING)
						return_status = COMMAND_SUCC;
					return;
				}
				break;

			case TYPE_ARRAY:
				if(matcher.match_index_attempt_result())
				{
					if((value = db[thing].exist_element(atoi(matcher.match_index_result().c_str()))) > 0)
					{
						if((int)db[thing].get_number_of_elements() >= value + 1)
						{
							return_status = COMMAND_SUCC;
							sprintf(scratch_return_string, "%d", value + 1);
							set_return_string (scratch_return_string);
						}
						else
						{
							return_status = COMMAND_FAIL;
							set_return_string ("NOTHING");
						}
					}
					else
					{
						notify_colour(get_player(), get_player(), COLOUR_ERROR_MESSAGES, "Array only has %d elements.", db[thing].get_number_of_elements());
					}
					return;
				}
				else
				{
					do
					{
						thing = db[thing].get_next();
					}
					while(thing != NOTHING && Typeof(thing) != TYPE_ARRAY);
					set_return_string (unparse_for_return (*this, thing));
					if(thing != NOTHING)
					{
						return_status = COMMAND_SUCC;
					}
					return;
				}
				break;

			case TYPE_VARIABLE:
				do
				{
					thing = db[thing].get_next();
				}
				while(thing != NOTHING && Typeof(thing) != TYPE_VARIABLE);
				set_return_string (unparse_for_return (*this, thing));
				if(thing != NOTHING)
					return_status = COMMAND_SUCC;
				return;

			case TYPE_PROPERTY:
				do
				{
					thing = db[thing].get_next();
				}
				while(thing != NOTHING && Typeof(thing) != TYPE_PROPERTY);
				set_return_string (unparse_for_return (*this, thing));
				if(thing != NOTHING)
					return_status = COMMAND_SUCC;
				return;
			default:
				break;
		}
	}

	thing = db[thing].get_next();
	set_return_string (unparse_for_return (*this, thing));
	if(thing != NOTHING)
		return_status = COMMAND_SUCC;
}


void
context::do_query_odrop (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof(thing) == TYPE_VARIABLE)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning: @?odrop used with a variable. This is a deprecated command");
		if(in_command()) {
			log_bug("@?odrop used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
		}
		else {
			log_bug("@?odrop used with variable %s(#%d) by player %s(#%d) on the command line",
					getname(thing), thing,
					getname(player), player);
		}
	}
	Accessed (thing);
	set_return_string (db[thing].get_odrop().c_str());
	return_status = COMMAND_SUCC;
}


void
context::do_query_ofail (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof(thing) == TYPE_VARIABLE)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning: @?ofail used with a variable. This is a deprecated command");
		if(in_command()) {
			log_bug("@?ofail used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
		}
		else {
			log_bug("@?ofail used with variable %s(#%d) by player %s(#%d) on the command line",
					getname(thing), thing,
					getname(player), player);
		}
	}
	Accessed (thing);
	set_return_string (db[thing].get_ofail().c_str());
	return_status = COMMAND_SUCC;
}


void
context::do_query_osuccess (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof(thing) == TYPE_VARIABLE)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning: @?osuccess used with a variable. This is a deprecated command");
		if(in_command()) {
			log_bug("@?osuccess used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
		}
		else {
			log_bug("@?osuccess used with variable %s(#%d) by player %s(#%d) on the command line",
					getname(thing), thing,
					getname(player), player);
		}
	}
	Accessed (thing);
	set_return_string (db[thing].get_osuccess().c_str());
	return_status = COMMAND_SUCC;
}


void
context::do_query_owner (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	set_return_string (unparse_for_return (*this, db[thing].get_owner()));
	return_status = COMMAND_SUCC;
}


void
context::do_query_parent (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	set_return_string (unparse_for_return (*this, db[thing].get_parent ()));
	return_status = COMMAND_SUCC;
}

void
context::do_query_bps (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	sprintf (scratch_return_string, "%d", db[thing].get_pennies());
	Accessed (thing);
	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}


#ifdef ALIASES
String
format_alias_string (
dbref	thing)

{
	static char	alias_buffer[(MAX_ALIASES * MAX_ALIAS_LENGTH)];
	int		i;
	int		gotone = 0;

	alias_buffer[0] = '\0';
	for (i = 0; i < MAX_ALIASES; i++)
	{
		if(db[thing].get_alias(i))
		{
			if (gotone != 0)
				strcat(alias_buffer, ";");
			strcat(alias_buffer, db[thing].get_alias(i).c_str());
			gotone = 1;
		}
	}
	return alias_buffer;
}


void
context::do_query_aliases (const String& name, const String&)
{
	dbref	thing = find_player_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (format_alias_string(thing));
}
#endif /* ALIASES */


void
context::do_query_race (const String& name, const String&)
{
	dbref	thing = find_executor_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (db[thing].get_race ().c_str());
}


void
context::do_query_rand (const String& value, const String&)
{
	int	top;
	
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	top = atoi (value.c_str());
	if (top <= 0)
		top = db.get_top ();

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "%ld", lrand48 () % top);
	set_return_string (scratch_return_string);
}


void
context::do_query_realtime (const String& name, const String&)
{
	time_t	now;

	if (!name)
		time (&now);
	else
		now = atol (name.c_str());
	strcpy (scratch_return_string, asctime (localtime (&now)));
	*(scratch_return_string + strlen (scratch_return_string) - 1) = '\0';
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}


void
context::do_query_score (const String& name, const String&)
{
	dbref	thing = find_executor_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	Accessed (thing);
	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "%ld", db[thing].get_score ());
	set_return_string (scratch_return_string);
}


void
context::do_query_set (const String& name, const String& flag)
{
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	dbref   thing = find_for_query (*this, name, 1);
	int	i;

	if (thing == NOTHING)
		return;

	for (i = 0; flag_list [i].string != NULL; i ++)
		if (string_prefix (flag_list[i].string, flag))
		{
			Accessed (thing);

			if (db[thing].get_flag(flag_list [i].flag))
			{
				return_status = COMMAND_SUCC;
				set_return_string (ok_return_string);
			}
			return;
		}
}

void
context::do_query_size (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 0);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	switch(Typeof(thing))
	{
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_COMMAND:
			sprintf (scratch_return_string, "%d", db[thing].get_number_of_elements());
			Accessed (thing);
			set_return_string (scratch_return_string);
			return_status = COMMAND_SUCC;
			break;

		default:
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't get the number of elements for that type of object.");
			return_status = COMMAND_FAIL;
			set_return_string (error_return_string);
			break;
	}

}


void
context::do_query_success (const String& name, const String&)
{
	dbref	thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (Typeof(thing) == TYPE_VARIABLE)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning: @?success used with a variable. This is a deprecated command");
		if(in_command()) {
			log_bug("@?success used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
		}
		else {
			log_bug("@?success used with variable %s(#%d) by player %s(#%d) on the command line",
					getname(thing), thing,
					getname(player), player);
		}
	}
	Accessed (thing);
	set_return_string (db[thing].get_succ_message().c_str());
	return_status = COMMAND_SUCC;
}


void
context::do_query_time (
const	String&,
const	String&)
{
	time_t	time_now;

	time (&time_now);
	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "%ld", (long int)time_now);
	set_return_string (scratch_return_string);
}


void
context::do_query_typeof (const String& name, const String& type)
{
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	thing = find_for_query (*this, name, 0);
	if (thing == NOTHING)
		return;

	Accessed (thing);

	if (!type)
	{
		return_status = COMMAND_SUCC;
		switch (Typeof (thing))
		{
			case TYPE_ROOM:
				set_return_string ("Room");
				break;
			case TYPE_THING:
				set_return_string ("Thing");
				break;
			case TYPE_EXIT:
				set_return_string ("Exit");
				break;
			case TYPE_PLAYER:
				set_return_string ("Player");
				break;
			case TYPE_COMMAND:
				set_return_string ("Command");
				break;
			case TYPE_FUSE:
				set_return_string ("Fuse");
				break;
			case TYPE_ALARM:
				set_return_string ("Alarm");
				break;
			case TYPE_VARIABLE:
				set_return_string ("Variable");
				break;
			case TYPE_PROPERTY:
				set_return_string ("Property");
				break;
			case TYPE_ARRAY:
				set_return_string ("Array");
				break;
			case TYPE_DICTIONARY:
				set_return_string ("Dictionary");
				break;
			case TYPE_PUPPET:
				set_return_string ("Puppet");
				break;
			default:
				return_status = COMMAND_FAIL;
				break;
		}
		return;
	}	
	else
	{
		bool	truebool = false;

		switch (Typeof (thing))
		{
			case TYPE_ROOM:
				truebool = string_prefix ("room", type);
				break;
			case TYPE_THING:
				truebool = string_prefix ("thing", type);
				break;
			case TYPE_EXIT:
				truebool = string_prefix ("exit", type);
				break;
			case TYPE_PLAYER:
				truebool = string_prefix ("player", type) || string_prefix("executor", type);
				break;
			case TYPE_PUPPET:
				truebool = string_prefix ("puppet", type) || string_prefix("executor", type);
				break;
			case TYPE_COMMAND:
				truebool = string_prefix ("command", type);
				break;
			case TYPE_FUSE:
				truebool = string_prefix ("fuse", type);
				break;
			case TYPE_ALARM:
				truebool = string_prefix ("alarm", type);
				break;
			case TYPE_VARIABLE:
				truebool = string_prefix ("variable", type);
				break;
			case TYPE_PROPERTY:
				truebool = string_prefix ("property", type);
				break;
			case TYPE_ARRAY:
				truebool = string_prefix ("array", type);
				break;
			case TYPE_DICTIONARY:
				truebool = string_prefix ("dictionary", type);
				break;
		}
		if (truebool)
		{
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
		}
	}
}


void
context::do_query_variables (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	dbref var = db[thing].get_variables();
	Accessed (thing);
	set_return_string (unparse_for_return (*this, var));
	if(var != NOTHING)
		return_status = COMMAND_SUCC;
}

void
context::do_query_properties (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	dbref var = db[thing].get_properties();
	Accessed (thing);
	set_return_string (unparse_for_return (*this, var));
	if(var != NOTHING)
		return_status = COMMAND_SUCC;
}

void
context::do_query_arrays (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	dbref var = db[thing].get_arrays();
	Accessed (thing);
	set_return_string (unparse_for_return (*this, var));
	if(var != NOTHING)
		return_status = COMMAND_SUCC;
}

void
context::do_query_dictionaries (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);
	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;

	dbref var = db[thing].get_dictionaries();
	Accessed (thing);
	set_return_string (unparse_for_return (*this, var));
	if(var != NOTHING)
		return_status = COMMAND_SUCC;
}


void
context::do_query_volume (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	switch (Typeof (thing))
	{
		case TYPE_PLAYER:
		case TYPE_ROOM:
		case TYPE_THING:
			sprintf (scratch_return_string, "%.9g", db[thing].get_inherited_volume ());
			break;
		default:
			return;
	}
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void
context::do_query_volume_limit (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	switch (Typeof (thing))
	{
		case TYPE_PLAYER:
		case TYPE_ROOM:
		case TYPE_THING:
			sprintf (scratch_return_string, "%.9g", db[thing].get_inherited_volume_limit ());
			break;
		default:
			return;
	}
	Accessed (thing);
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void
context::do_query_weight (const String& name, const String&)
{
	dbref thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	switch (Typeof (thing))
	{
		case TYPE_PLAYER :
		case TYPE_ROOM :
		case TYPE_THING :
			sprintf (scratch_return_string, "%.9g", (db[thing].get_inherited_mass () + find_mass_of_contents_except (thing, NOTHING)));
			break;
		default :
			return;
	}
	Accessed (thing);
	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}

void
context::do_query_who (const String& name, const String&)
{
	dbref victim = find_executor_for_query (*this, name, 0);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (victim == NOTHING)
		return;

	Accessed (victim);
	return_status = COMMAND_SUCC;
	set_return_string (db[victim].get_who_string ().c_str());
}

void
context::do_query_ctime (const String& name, const String&)
{
	time_t 	t;
	dbref	thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (!(t = db[thing].get_ctime ()))
		return;
	sprintf (scratch_return_string, "%ld", t);
	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}
	
void
context::do_query_mtime (const String& name, const String&)
{
	time_t 	t;
	dbref	thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (!(t = db[thing].get_mtime ()))
		return;
	sprintf (scratch_return_string, "%ld", t);
	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}
	
void
context::do_query_atime (const String& name, const String&)
{
	time_t 	t;
	dbref	thing = find_for_query (*this, name, 1);

	set_return_string (error_return_string);
	return_status = COMMAND_FAIL;
	if (thing == NOTHING)
		return;
	if (!(t = db[thing].get_atime ()))
		return;
	sprintf (scratch_return_string, "%ld", t);
	set_return_string (scratch_return_string);
	return_status = COMMAND_SUCC;
}

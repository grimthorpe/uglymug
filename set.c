/* static char SCCSid[] = "@(#)set.c 1.108 99/03/02@(#)"; */
#include "copyright.h"

/* commands which set parameters */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "db.h"
#include "config.h"
#include "match.h"
#include "interface.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "colour.h"
#include "log.h"


static dbref
match_controlled (
context		&c,
const	String&name)
{
	dbref match;

	Matcher matcher (c.get_player (), name, TYPE_NO_TYPE, c.get_effective_id ());
	if (c.gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();

	match = matcher.noisy_match_result();

	if(match != NOTHING && !c.controls_for_write (match))
	{
		if(Readonly(match))
			notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, "You cannot modify an object with the ReadOnly flag set.");
		else
			notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return NOTHING;
	}

	return match;
}

void
context::do_at_insert (
const	String& what,
const	String& element)
{
	/* I'm not catering for Dictionaries because it would need an extra
	   argument and Ian said 'Just put the code in, please, pretty please
	   with sugar on top' */

	dbref	thing;
	int	typething;
	unsigned int	index;

	/* First find what we are looking for */
	Matcher what_matcher (player, what, TYPE_NO_TYPE, get_effective_id());
	what_matcher.match_everything();
	if(what_matcher.match_result() ==  NOTHING)
			what_matcher.match_command();

	thing = what_matcher.match_result();
	if(thing == NOTHING)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Object not found");
		return;
	}
	typething = Typeof(thing);

	switch(typething)
	{
		case TYPE_DICTIONARY:
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@insert cannot be used on Dictionaries");
			RETURN_FAIL;
			break;
		default:
			break;
	}


	/*
	 * A more meaningful message when trying to describe a read-only object
	 */
	if (Readonly(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the description of a read-only object.");
		return;
	}

	/* Do we need to check if we own it */
	if(!controls_for_write(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		RETURN_FAIL;
	}


/* Not using this bit until someone puts extra argument in */
#if 0
	/* Check the index is valid */
	switch(typething)
	{
		case TYPE_DICTIONARY:
			if(!(index = db[thing].exist_element(what_matcher.match_index_result())))
			{
				notify_colour(player, COLOUR_ERROR_MESSAGES, "That element does not exist.");
				RETURN_FAIL;
			} 

		case TYPE_COMMAND:
		case TYPE_ARRAY:
	}
#endif

	const String& indexString = what_matcher.match_index_result();
	if (!indexString)
	{
		notify_colour ( player, player, COLOUR_ERROR_MESSAGES, "@insert requires an index." );
		RETURN_FAIL;
	}

	index = atoi( indexString.c_str() );
	if(index > (db[thing].get_number_of_elements() + 1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The object does not have that many elements.");
		RETURN_FAIL;
	}
	else if(index <= 0)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The index must be greater than 0.");
		RETURN_FAIL;
	}

	/* Check that the array/dict/command can take the extra entry */
	switch(typething)
	{
		unsigned int limit;
		
		case TYPE_ARRAY:
			if(Wizard(thing))
				limit = MAX_WIZARD_ARRAY_ELEMENTS;
			else
				limit = MAX_MORTAL_ARRAY_ELEMENTS;

			if((db[thing].get_number_of_elements() + 1) > limit)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Too many elements. Maximum is %d");
				RETURN_FAIL;
			}
			break;

		default:
			if(Wizard(thing))
				limit = MAX_WIZARD_DESC_SIZE;
			else
				limit = MAX_MORTAL_DESC_SIZE;

			if((element.length() + db[thing].get_size()) > limit)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, that line makes the description too big.");
				RETURN_FAIL;
			}
			break;
	}

	/* Do it */
	db[thing].insert_element(index, element);
	Modified (thing);

	if (!gagged_command() && !in_command())
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Inserted.");

	if(typething == TYPE_COMMAND)
		db [thing].flush_parse_helper();

	RETURN_SUCC;

}

enum directions
{
	ASCEND,
	DESCEND,
	ELASCEND,
	ELDESCEND
};

/* Things we have to do with sort:
	Sort a desc/array in ascending/descending order
	Sort a dictionart in ascending/descending order on indexes or elements */

	/* I will do it with options in the direction field:

		ascend		desc/array - elements, dict - index
		descend		
		elascend	dict - elements
		eldescend
	*/

void
context::do_at_sort (
const	String& array,
const	String& direction)
{
	dbref	thing;
	directions	direct = ASCEND;

	/* First find what we are looking for */
	Matcher what_matcher (player, array, TYPE_NO_TYPE, get_effective_id());
	what_matcher.match_array_or_dictionary();
	what_matcher.match_everything();
	if(what_matcher.match_result() == NOTHING)
			what_matcher.match_command();

	thing = what_matcher.match_result();
	if(thing == NOTHING)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Object not found");
		return;
	}
	else if(thing == AMBIGUOUS)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I don't know which one you mean");
		return;
	}
	else if(thing < 0)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Thats not a valid thing to sort");
		return;
	}

	/*
	 * A more meaningful message when trying to describe a read-only object
	 */
	if (Readonly(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't sort a read-only object.");
		return;
	}

	/* Do we need to check if we own it */
	if(!controls_for_write(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		RETURN_FAIL;
	}

	if(string_prefix("ascend", direction))
		direct= ASCEND;
	else if(string_prefix("descend", direction))
		direct= DESCEND;
	else if(string_prefix("elascend", direction))
		direct= ELASCEND;
	else if(string_prefix("eldescend", direction))
		direct= ELDESCEND;


	/* Do it */
	db[thing].sort_elements(direct);
	Modified (thing);
	RETURN_SUCC;
}

void
context::do_at_peak
(const	String& number,
const	String& )
{
char buf[100]; // '@peak <maxint>\0'

	if(!Wizard(player))
	{
		notify(player, "Don't be silly.");
		return;
	}
	if(number)
	{
		peak_users = atoi(number.c_str());
		notify(player, "Peak set to %d.", peak_users);
// Do some logging, cos someone is abusing @peak
sprintf(buf,"@peak %d",peak_users);
log_command(    player,
                getname(player),
                get_effective_id(),
                getname(get_effective_id()),
                db[player].get_location(),
                getname(db[player].get_location()),
                buf
                );
	}

	RETURN_SUCC;
}

void
context::do_at_race (
const	String& name,
const	String& newrace)

{
	dbref	thing;
	ssize_t	newlinecheck;
	String	checkrace = newrace;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) == NOTHING)
		return;
	if ((Typeof(thing) != TYPE_PLAYER) && (Typeof(thing) != TYPE_PUPPET))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Only Players have races.");
		return;
	}

	if ((newlinecheck = checkrace.find('\n')) != -1)
	{
		checkrace = checkrace.substring(newlinecheck);
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "WARNING: Race truncated at newline.");
	}

	if(!ok_name(checkrace))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That is not a reasonable race.");
		return;
	}	

	/* Assuming all OK, changing race... */
	db[thing].set_race (checkrace);
	Modified (thing);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES,"Race Set.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_name (
const	String& name,
const	String& newname)

{
	dbref	thing;
	int	value;
	time_t	now;
	time_t	temp = 0;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();

	if ((thing = matcher.noisy_match_result()) == NOTHING)
		return;

	/*
	 * A more meaningful message when trying to describe a read-only object
	 */
	if (Readonly(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the name of a read-only object.");
		return;
	}

	if(!controls_for_write (thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the name of something you don't own or control.");
		return;
	}

	switch(Typeof(thing))
	{
		case TYPE_PLAYER:
			now = time(NULL);
			if (is_guest(player) && !Wizard(get_effective_id ()))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry but guests are not allowed to change their names.");
				return;
			}
			else
			{
				// Allow the same name, for re-caps, or for using the same name as an alias.
				if(db.lookup_player(newname) == thing)
				{
					// This refers to us already...
					// This is here so we've got a fall-through for the other tests
				}
				else if (!ok_player_name(newname)) // Allow the same name, for re-caps
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't give a player that name.");
					return;
				} 
				else if(!Wizard(get_effective_id ()) && !Wizard(player) && !Apprentice(player) && ((temp = now - db[thing].get_last_name_change()) < NAME_TIME))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you can't change your name for another %d minute%s.", (NAME_TIME - temp)/60 + 1, PLURAL((NAME_TIME - temp)/60 + 1));
					return;
				}

				db[thing].set_last_name_change(now);
#ifdef LOG_NAME_CHANGES
				log_message("NAME CHANGE: #%d: %s to %s", thing, db[thing].get_name(), newname);
#endif
				db.remove_player_from_cache(db[thing].get_name());
			}
			break;

		case TYPE_PUPPET:
			/* Stop Puppet/Player spoofing with puppets having same name as player */
			if (lookup_player(player, newname)!=NOTHING)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't give a puppet the same name as an existing player!");
				return;
			}
			break;

		case TYPE_DICTIONARY:
			if(matcher.match_index_result())
				if((value = db[thing].exist_element(matcher.match_index_result())) > 0)
				{
					db[thing].set_index (value, newname);
					if (!in_command())
						notify_colour(player, player, COLOUR_MESSAGES, "Element name set.");

					return_status = COMMAND_SUCC;
					set_return_string (ok_return_string);
					return;
				}
				else
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Dictionary \"%s\" does not contain element \"%s\".", db[thing].get_name().c_str(), matcher.match_index_result().c_str());
					return;
				}
			else
			{
				if(!ok_name(newname))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES,"That is not a reasonable name.");
					return;
				}
			}
			break;

		case TYPE_ARRAY:
			if(matcher.match_index_result())
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "You can't rename an element of an array.");
				return;
			}
			else
			{
				if(!ok_name(newname))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That is not a reasonable name.");
					return;
				}
			}

		default:
			if (!newname)
			{
				if (db [thing].get_parent () == NOTHING)
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only set a null name on an object with a parent.");

					return;
				}
			}
			else if(!ok_name(newname))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That is not a reasonable name.");
				return;
			}
	}

	db[thing].set_name(newname);
	Modified (thing);

	if(Typeof(thing)==TYPE_PLAYER)
		db.add_player_to_cache(thing, newname);

	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Name set.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_describe (
const	String& name,
const	String& description)

{
	unsigned int	value;
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "What do you want to describe?");
		return;
	}
		
	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();

	if ((thing = matcher.noisy_match_result()) == NOTHING)
		return;

	/*
	 * A more meaningful message when trying to describe a read-only object
	 */
	if (Readonly(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the description of a read-only object.");
		return;
	}

	if (!controls_for_write (thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't describe something you don't own or control.");
		return;
	}

	if(Eric(thing))
	{
		// Inheritable thing?
		dbref matchloc = matcher.get_location();
		if((matchloc != NOTHING) && (matchloc != db[thing].get_location()))
		{
			// The location of the found object is not the same as the location we started searching on.
			// So as long as this is a variable, we should be OK.
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Need to create new variable here (%d).", matcher.get_location());
			return;
		}
	}

	switch (Typeof (thing))
	{
		case TYPE_FUSE:
			value = atoi (description.c_str());
			if (value < 1)
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Fuses must tick at least once.");
				return;
			}
			db[thing].set_description(description);
			break;
		case TYPE_ALARM:
			db.unpend (thing);
			db[thing].set_description(description);
			if (description)
				db.pend (thing);
			break;
		case TYPE_DICTIONARY:
			if(matcher.match_index_result())
				if((value = db[thing].exist_element(matcher.match_index_result())) > 0)
					db[thing].set_element(value, matcher.match_index_result(), description);
				else
				{
					if((int)(db[thing].get_number_of_elements() + 1) <= ((Wizard(thing)) ? MAX_WIZARD_DICTIONARY_ELEMENTS : MAX_MORTAL_DICTIONARY_ELEMENTS))
						db[thing].set_element(value, matcher.match_index_result(), description);
					else
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "A dictionary can't have more than %d elements", (Wizard(thing))? MAX_WIZARD_DICTIONARY_ELEMENTS : MAX_MORTAL_DICTIONARY_ELEMENTS);
						return;
					}
				}
			else
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "A dictionary can't have null indices.");
				return;
			}
			break;

		case TYPE_ARRAY:
			if(matcher.match_index_attempt_result())
			{
				if(matcher.match_index_result())
				{
					if((value = atoi (matcher.match_index_result().c_str())) > 0)
						if(value <= ((Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS))
							db[thing].set_element(value, description);
						else
						{
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, "An array can't have more than %d elements", (Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS);
							return;
						}
					else
					{
						notify_colour(player,player, COLOUR_ERROR_MESSAGES, "An array can't have negative, zero or non-numeric indices.");
						return;
					}
				}
				else
				{
					if((db[thing].get_number_of_elements() + 1) <= ((Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS))
						db[thing].set_element(db[thing].get_number_of_elements() + 1, description);
					else
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "An array can't have more than %d elements", (Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS);
						return;
					}
				}
			}
			else
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Arrays do not have descriptions.");
				return;
			}

			break;
		case TYPE_COMMAND:
			if(matcher.match_index_attempt_result())
			{
				if(matcher.match_index_result())
				{
					if((value = atoi (matcher.match_index_result().c_str())) > 0)
						if(value <= ((Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS))
							db[thing].set_element(value, description);
						else
						{
							notify_colour(player, player, COLOUR_ERROR_MESSAGES, "A command can't have more than %d lines", (Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS);
							return;
						}
					else
					{
						notify_colour(player,player, COLOUR_ERROR_MESSAGES, "You must specify a positive line number greater than zero.");
						return;
					}
				}
				else
				{
					if((db[thing].get_number_of_elements() + 1) <= ((Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS))
						db[thing].set_element(db[thing].get_number_of_elements() + 1, description);
					else
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "A command can't have more than %d lines.", (Wizard(thing))? MAX_WIZARD_ARRAY_ELEMENTS : MAX_MORTAL_ARRAY_ELEMENTS);
						return;
					}
				}
			}
			else
			{
				db[thing].set_description(description);
			}
			db[thing].flush_parse_helper();
			break;

		default:
			db[thing].set_description(description);
			break;
	}

	Modified (thing);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Description set.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_fail (
const	String& name,
const	String& message)

{
	dbref thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_PLAYER:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't set the fail field of a player.");
				return;

			case TYPE_DICTIONARY:
			case TYPE_ARRAY:
			case TYPE_PROPERTY:
				notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "That object type cannot have a fail message.");
				return;
			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @fail used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@fail used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@fail used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}

			default:
				break;
		}

		db[thing].set_fail_message(message);
		Modified (thing);

		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Message set.");

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_at_success (
const	String& name,
const	String& message)

{
	dbref thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_PLAYER:
				if(!Wizard(get_effective_id ()))
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the success field of a player.");
					return;
				}
				break;

			case TYPE_ARRAY:
			case TYPE_PROPERTY:
			case TYPE_DICTIONARY:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That object type does not have a success field.");
				return;
				
			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @success used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@success used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@success used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:
				break;
			
		}

		db[thing].set_succ_message(message);
		Modified (thing);

		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Message set.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);

	}
}

void
context::do_at_drop (
const	String& name,
const	String& message)

{
	int	value;
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_FUSE:
				if (message)
				{
					if (Typeof (thing) == TYPE_FUSE)
					{
						value = atoi (message.c_str());
						if (value < 1)
						{
							notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That's too short a fuse length.");
							return;
						}
					}
				}
				break;

			case TYPE_PROPERTY:
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That object does not have a drop message.");
				return;

			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @drop used with a variable. This is a deprecated command.");
				if (in_command()) {
					log_bug("@drop used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@drop used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:
				break;
		}

		db[thing].set_drop_message(message);
		Modified (thing);

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Set.");
	}
}

void
context::do_at_osuccess (
const	String& name,
const	String& message)

{
	dbref thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_PLAYER:
				if (!Wizard(get_effective_id ()))
				{
					notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the osuccess field of a player.");
					return;
				}

			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
				notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "That object does not have an osuccess message.");
				return;

			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @osuccess used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@osuccess used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@osuccess used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:
				break;
		}

		db[thing].set_osuccess(message);
		Modified (thing);

		if (!in_command())
			notify_colour(player,  player, COLOUR_MESSAGES, "Message set.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_at_ofail (
const	String& name,
const	String& message)

{
	dbref thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_PLAYER:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't set the ofail field of a player.");
				return;

			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That object does not have an ofail message.");
				return;

			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @ofail used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@ofail used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@ofail used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:
				break;
		}

		db[thing].set_ofail(message);
		Modified (thing);

		if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Message set.");

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_at_odrop (
const	String& name,
const	String& message)

{
	dbref thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_FUSE:
				notify_colour (player,  player, COLOUR_ERROR_MESSAGES,"You cannot set the odrop of a fuse");
				return;

			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES,  "That object does not have an odrop message.");
				return;

			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @odrop used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@odrop used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@odrop used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:
				break;
		}
			
		db[thing].set_odrop(message);
		Modified (thing);

		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Message set.");

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_at_lock (
const	String& name,
const	String& keyname)

{
	dbref	thing;
	boolexp	*key;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher name_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		name_matcher.work_silent ();
	name_matcher.match_everything();

	switch (thing = name_matcher.match_result())
	{
		case NOTHING:
			notify_colour(player,  player, COLOUR_MESSAGES, "I don't see what you want to lock!");
			return;
		case AMBIGUOUS:
			notify_colour(player, player, COLOUR_MESSAGES,  "I don't know which one you want to lock!");
			return;
		default:
			if (Readonly(thing))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the lock of a read-only object.");
				return;
			}
			
			if(!controls_for_write (thing))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't lock something you don't own or control.");
				return;
			}
			break;
	}

	switch(Typeof(thing))
	{
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That object does not have a lock.");
				return;

			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @lock used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@lock used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@lock used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:

				if (!keyname || (key = parse_boolexp (player, keyname)) == TRUE_BOOLEXP)
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I don't understand that key.");
				else
				{
					/* Check for commands locking to themselves */
					if (Typeof (thing) == TYPE_COMMAND && key->contains (thing))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot give a command a lock that includes itself.");
						return;
					}

					/* everything ok, do it */
					db[thing].set_key(key);
					Modified (thing);

					if (!in_command())
						notify_colour(player, player, COLOUR_MESSAGES, "Locked.");

					return_status = COMMAND_SUCC;
					set_return_string (ok_return_string);
				}
	}
}


void
context::do_at_unlock (
const	String& name,
const	String& )

{
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) != NOTHING)
	{
		switch(Typeof(thing))
		{
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
			case TYPE_PROPERTY:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That object does not have a lock.");
				return;

			case TYPE_VARIABLE:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning: @unlock used with a variable. This is a deprecated command.");
				if(in_command()) {
					log_bug("@unlock used with variable %s(#%d) by player %s(#%d) in command %s(#%d)",
							getname(thing), thing,
							getname(player), player,
							getname(get_current_command()), get_current_command());
				}
				else {
					log_bug("@unlock used with variable %s(#%d) by player %s(#%d) on the command line",
							getname(thing), thing,
							getname(player), player);
				}
			default:
				db[thing].set_key(TRUE_BOOLEXP);
				Modified (thing);

				if (!in_command())
					notify_colour(player,  player, COLOUR_MESSAGES, "Unlocked.");

				return_status = COMMAND_SUCC;
				set_return_string (ok_return_string);
		}
	}
}

void
context::do_at_unlink (
const	String& name,
const	String& )

{
	dbref exit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, name, TYPE_EXIT, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_exit();
	matcher.match_here();
	matcher.match_absolute();

	if ((exit = matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(exit))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change unlink a read-only object.");
		return;
	}

	if(!controls_for_write (exit))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't own that exit.");
		return;
	}

	switch(Typeof(exit))
	{
		case TYPE_EXIT:
			if (db[exit].get_destination() != NOTHING)
				db[db[exit].get_owner()].set_pennies(db[db[exit].get_owner()].get_pennies() + LINK_COST);
			db[exit].set_destination(NOTHING);
			Modified (exit);
			if (!in_command())
				notify_colour(player, player, COLOUR_MESSAGES, "Unlinked.");
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			break;
		case TYPE_ROOM:
			db[exit].set_destination(NOTHING);
			Modified (exit);
			if (!in_command())
				notify_colour(player, player, COLOUR_MESSAGES, "Dropto removed.");

			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			break;
		default:
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't unlink that!");
			break;
	}
}


void
context::do_at_owner (
const	String& name,
const	String& newowner)

{
	dbref	thing;
	dbref	object;
	dbref	owner;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!name)
	{
		notify_colour(player,  player, COLOUR_MESSAGES, "Change ownership of what?");
		return;
	}

	Matcher name_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		name_matcher.work_silent ();
	name_matcher.match_everything();
	if((thing = name_matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(thing))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the owner of a read-only object.");
		return;
	}

	/* Work out who the owner will be */
	if(newowner)
	{
		Matcher owner_matcher (player, newowner, TYPE_PLAYER, get_effective_id ());
		if (gagged_command ())
			owner_matcher.work_silent ();
		owner_matcher.match_neighbor ();
		owner_matcher.match_player ();
		owner_matcher.match_me ();
		owner_matcher.match_absolute ();

		if((owner = owner_matcher.noisy_match_result()) == NOTHING)

		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I couldn't find that player.");
			return;
		}
		if((Typeof(owner) != TYPE_PLAYER) && (Typeof(owner) != TYPE_PUPPET))
		{
		        notify_colour(player, player, COLOUR_MESSAGES, "You can only change ownership to a player");
			return;
		}
	}
	else
		owner = get_effective_id ();

	/* If they are ownering to the group it is okay */
/*	if(owner != db[player].get_build_id()) */
	if(1)
	{
		/* Check permissions for mortals */
		if (!Wizard (get_effective_id ()))
		{
			if(!((db[owner].get_owner() == player) &&
			    (db[thing].get_owner() == player)))
			{
				/* Check a non-wizard isn't trying a Wizard-style chown */
				if ((thing != get_effective_id ()) && (owner != get_effective_id ()))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES,  permission_denied.c_str());
					return;
				}
				/* Check CHOWN_OK for non-Wizards */
				if ((thing != get_effective_id ()) && !(db[thing].get_flag(FLAG_CHOWN_OK)))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You cannot change ownership of an object that's not yours, unless it has its CHOWN_OK flag set.");
					return;
				}
			}
		}

		/* Check permissions for Wizards */
		else if ((!ChownAll(player))
			&& ((Wizard (owner) && owner != player)
				|| (Wizard (thing) && (thing != player))))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
			return;
		}
	} /* end of group if */

	switch (Typeof(thing))
	{
		case TYPE_ROOM:
		case TYPE_EXIT:
			if(owner != db[player].get_build_id())
				if (!Wizard (get_effective_id ()) && !Wizard (player))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can change ownership of rooms and exits.");
					return;
				}
			db[thing].set_owner(owner);
			break ;
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_FUSE:
			/* Deny if someone's changing command permissions - hack trap. 6/10/94 PJC */
			if (owner == player && in_command ())
			{	
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Tried to change ownership of command %s(#%d) to you.", getname_inherited (thing), thing);
				return;
			}
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_PROPERTY:
		case TYPE_THING:
		case TYPE_VARIABLE:
			db[thing].set_owner(owner);
			break;
		case TYPE_PUPPET:
			if (Typeof(owner) != TYPE_PLAYER)
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "NPCs can only be owned by Players.");
				return;
			}
			db[thing].set_owner(owner);
			break;
		case TYPE_PLAYER:
			/* Way around command owning to own all a players objects - no longer within a command 7/12/94 */
		if (owner == player && in_command ())
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Tried to change ownership of player %s(#%d) to you.", getname (thing), thing);
			return;
		}

		else
		{
			if (db[owner].get_flag(FLAG_CHOWN_OK))
			{
				for (object = 0; object < db.get_top (); object++)
					if (Typeof (object) != TYPE_FREE)
					{
						if ((db[object].get_owner() == thing) && (Typeof (object) != TYPE_PLAYER))
							db[object].set_owner(owner);
					}
			}
			else
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES,  "You can only give all your objects to a CHOWN_OK player");
				return;
			}
		}
		break;
		default:
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "I don't know how to change ownership of that type of object - SEE A WIZARD!");
			return;
	}
	Modified (thing);
	if (!in_command())
		notify_colour(player,  player, COLOUR_MESSAGES, "Owner changed.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_set (
const	String& name,
const	String& flag)

{
	dbref			thing;
	const	char		*p;
	object_flag_type	f;
	int			i;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	/* find thing - don't use match_controlled so we can unset an (r) flag! */
	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything ();

	if ((thing = matcher.noisy_match_result ()) == NOTHING)
		return;

	/* move p past NOT_TOKEN if present */
	for(p = flag.c_str(); p && *p && (*p == NOT_TOKEN || isspace(*p)); p++)
		;

	/* identify flag */
	if ((p == NULL) || (*p == '\0'))
	{
		notify_colour (player, player, COLOUR_MESSAGES, "You must specify a flag name.");
		return;
	}

	for (i = 0; flag_list [i].string != NULL; i++)
		if(string_prefix(flag_list [i].string, p))
		{
			if ((flag_list [i].flag == FLAG_CONNECTED) ||
			    (flag_list [i].flag == FLAG_REFERENCED))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That flag can't be set by a user.");
				return;
			}
			if (flag_list[i].flag == FLAG_OFFICER)
			{
				notify_colour(player,player,COLOUR_ERROR_MESSAGES, "The Officer flag is obsolete. Please don't try to set it.");
				return;
			}
			else
			{
				f = flag_list [i].flag;
				break;
			}
		}
	if (flag_list [i].string == NULL)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I don't recognize that flag.");
		return;
	}

	if (*flag.c_str() != NOT_TOKEN)
	{
		if (!is_flag_allowed(Typeof(thing), f))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't apply that flag to that type of object.");
			return;
		}
	}

	/* because of the way controls_for_write() works, we unconditionally unset
	 * the ReadOnly flag, then we can check controls_for_write,
	 * if this fails, the player didn't have permission to unset the ReadOnly flag,
	 * so reset it - Chisel 2002/02/04
	 * */
	if ((f == FLAG_READONLY) && (*flag.c_str() == NOT_TOKEN))
	{
		db[thing].clear_flag(f);	/* unconditially remove ReadOnly */

		/* check permissions */
		if (!controls_for_write (thing))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
			db[thing].set_flag(f);	/* reset the flag */
			return;
		}
	}

        /* Give a nicer message when we try to teleport a read-only object */
        if (Readonly(thing))
        {
                notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't modify a read-only object.");
                return;
	}

	if ((f==FLAG_SILENT) && (Typeof(thing)==TYPE_PLAYER))
	{
		if (!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only wizards can set this on a player!");
			return;
		}
	}
	if ((f==FLAG_NATTER) && (Typeof(thing)==TYPE_PLAYER))
	{
		if (in_command())
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Tried to set Natter flag on player %s(#%d).", getname (thing), thing);
			return;
		}

		if (
		    (!Wizard (get_effective_id ()) && (thing != player))
		    ||
		    (!Wizard (get_effective_id ()) && (thing == player) && (*flag.c_str() != NOT_TOKEN)))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only wizards can set the Natter flag on players.");
			return;
		}
		if(!controls_for_write(thing))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
			return;
		}
		if(Wizard(thing) || Apprentice(thing))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Wizards and Apprentices can't leave or join Natter.");
			return;
		}
		if ((thing != player) && (*flag.c_str() != NOT_TOKEN))
		{
			notify_colour (thing, player, COLOUR_ERROR_MESSAGES, "WARNING: %s has set you Natter, in order to escape type '@set me = !natter'", getname(player));
		}
		if (*flag.c_str() != NOT_TOKEN)
		{
			notify_wizard_natter ("  %s has joined Natter", getname_inherited (thing));
		}
		else
		{
			notify_wizard_natter ("  %s has left Natter", getname_inherited (thing));
		}
	}
	switch(f)
	{
	case FLAG_GOD_SET_GOD:
	case FLAG_GOD_WRITE_ALL:
	case FLAG_GOD_BOOT_ALL:
	case FLAG_GOD_CHOWN_ALL:
	case FLAG_GOD_MAKE_WIZARD:
	case FLAG_GOD_PASSWORD_RESET:
	case FLAG_GOD_DEEP_POCKETS:
		if(in_command())
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change that flag in a command.");
			return;
		}
		if(Typeof(thing) == TYPE_PLAYER)
		{
			if(!SetGodPower(player))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have permission to change that god-like power.");
				return;
			}
		}
		break;
	default:
		break;
	}

	if ((f==FLAG_DONTANNOUNCE) && (*flag.c_str() != NOT_TOKEN))
	{
		if(!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only wizards can set DontAnnounce flag on players.");
			return;
		}
	}
	if ((f==FLAG_NOHUHLOGS) && (*flag.c_str() != NOT_TOKEN))
	{
		if (!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set the NoHuhLogs flag on players.");
			return;
		}
	}
	if ((f==FLAG_CENSORED) && (Typeof(thing)==TYPE_PLAYER))
	{
		if (!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set or unset the censored flag on players.");
			return;
		}
	}
	if((f==FLAG_HAVEN) && (Typeof(thing)==TYPE_COMMAND))
	{
		if (!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set or unset the haven flag on commands.");
			return;
		}
	}

	if (!controls_for_write (thing))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if((f==FLAG_MALE) || (f==FLAG_FEMALE) || (f==FLAG_NEUTER))
	{
		if(*flag.c_str()==NOT_TOKEN)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't unassign gender!");
			return;
		}
		else
		{
			db[thing].clear_flag(FLAG_MALE);
			db[thing].clear_flag(FLAG_FEMALE);
			db[thing].clear_flag(FLAG_NEUTER);
		}
	}

	/* check for restricted flag */
	if(!MakeWizard(player) && !Wizard(get_effective_id ())
		&& ((f == FLAG_WIZARD)
			|| (f == FLAG_RETIRED)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set that flag.");
		return;
	}

#ifdef RESTRICTED_BUILDING
	if (f == FLAG_BUILDER)
	{
		if(!Apprentice(player) && !Wizard(player))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards or Apprentices can set the Builder flag.");
			return;
		}
	}
#endif /* RESTRICTED_BUILDING */

	if (f == FLAG_APPRENTICE)
	{
		if(!Wizard(player))
		{
			notify_colour (player,player, COLOUR_ERROR_MESSAGES, "Only Wizards may create Apprentices.");
			return;
		}
	}

	if (f == FLAG_ASHCAN)
	{
//		if (!Wizard (get_effective_id ()))
//		{
//			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the 'ashcan' flag.");
//			return;
//		}
//		if (!Wizard (get_effective_id ()) && !(player == db[thing].get_owner()) )
//		{
//			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the 'ashcan' flag on other player's possesions!");
//			return;
//		}
//		if (player == thing)
//		{
//			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "We don't allow players to set the AshCan flag on themselves!");
//			return;
//		}

		/*
		 * non-wizard, and they aren't unsetting themselves
		 * _OR_
		 * non-wizard, and they _ARE_ trying to set themselves ashcan
		 */
		if (
		    (!Wizard (get_effective_id ()) && (thing != player))
		    ||
		    (!Wizard (get_effective_id ()) && (thing == player) && (*flag.c_str() != NOT_TOKEN)))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the 'ashcan' flag.");
			return;
		}

		if ((thing == COMMAND_LAST_RESORT) || (thing == LIMBO) || (thing == PLAYER_START))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "Now that would REALLY mess the database up!");
			return;
		}
		if (Wizard (thing))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That item is protected with the Wizard flag.");
			return;
		}
		if (Readonly (thing))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES,"That object is protected by the READONLY flag.");
			return;
		}
	}

	if (f == FLAG_OFFICER)
	{
		if(!Wizard(player))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the officer flag.");
			return;
		}
	}

	if (f == FLAG_WELCOMER)
	{
		if(!Wizard(player))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the welcomer flag.");
			return;
		}
	}
	
	if (f == FLAG_XBUILDER)
	{
		if(!Wizard(player))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the XBuilder flag.");
			return;
		}
	}
	
	if (f == FLAG_HOST)
	{
		if(!Wizard(player))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the host flag.");
			return;
		}
	}	

	if (f == FLAG_FIGHTING)
	{
		if (*flag.c_str() == NOT_TOKEN)
		{
			if (!Wizard (get_effective_id ()))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may change the Fighting flag.");
				return;
			}
		}
	}

	if ((f == FLAG_DARK) && (!Wizard (get_effective_id ())))
	{
		switch (Typeof (thing))
		{
			case TYPE_PLAYER:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Dark players are not allowed.");
				return;
			default:
				break;
		}
	}
	if((f==FLAG_CHOWN_OK) && (Typeof (thing) == TYPE_PLAYER))
	{
		if (in_command())
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Cannot set CHOWN_OK on yourself inside a command.");
			return;
		}
		if (*flag.c_str() != NOT_TOKEN)
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Other players can now change ownership of objects to you without your permission!");
	}

	if(f == FLAG_WIZARD && in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You just tried to execute an @set WIZARD in a command.");
		return;
	}

	if(f == FLAG_APPRENTICE && in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You just tried to execute an @set APPRENTICE in a command.");
		return;
	}

	if((f == FLAG_WIZARD) && (!MakeWizard(player)) && (Typeof (thing) == TYPE_PLAYER))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "You may not make Wizards.");
		return;
	}
	if (f == FLAG_WIZARD && (Typeof(thing) == TYPE_PLAYER) && Puppet(thing))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot set a puppet to be a wizard.");
		return;
	}
	/* Check you can't trash a container with things in it */
	if ((*flag.c_str() == NOT_TOKEN)
		&& (db[thing].get_contents() != NOTHING)
		&& (Container (thing))
		&& ((f == FLAG_OPEN) || (f == FLAG_OPENABLE)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Remove the contents, then try again.");
		return;
	}


#ifdef ABORT_FUSES
	/* Only Wizards can set abort fuses, and we log it. */

	if ((f==FLAG_ABORT) && (*flag.c_str() != NOT_TOKEN))
	{
		if (!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only wizards may create Abort fuses.");
			return;
		}
		if ( (get_current_command() != NOTHING) && (!Wizard(get_current_command())))
		{
			log_hack(NOTHING, "Attempt to set ABORT flag on %s(#%d) by %s(%d) in command %s(%d)",
					getname(thing), thing,
					getname(player), player,
					getname(get_current_command()), get_current_command());
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Abort fuses may only be created in commands if the command is set Wizard.");
			return;
		}
	}
#endif

	/* Log every WIZARD flag set */
	if ((f == FLAG_WIZARD) && (*flag.c_str() != NOT_TOKEN))
	{
		if (Typeof(thing) == TYPE_COMMAND)
			log_hack(NOTHING, "<<<WIZARD>>> flag set on %s(#%d) by %s(#%d)",getname(thing),thing,getname(player),player);
		else
			log_hack(NOTHING, "PROTECT: WIZARD flag set on %s(#%d) by %s(#%d)",getname(thing),thing,getname(player),player);
	}
	if (f == FLAG_READONLY)
	{
		if (!Wizard (get_effective_id ()))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set the Readonly flag.");
			return;
		}
		if (in_command ())
		{
			dbref cur_cmd = get_current_command ();
			if (!Wizard(cur_cmd) || !Wizard(db[cur_cmd].get_owner()) || !Readonly(cur_cmd))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You just tried to execute an @set READONLY in a command which was not wizard owned, set, or readonly.");
				return;
			}
			if (!Silent(get_current_command()))
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning:  @set READONLY in command.");
		}
	}
	if ((f==FLAG_BACKWARDS) && (*flag.c_str() != NOT_TOKEN))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot set the Backwards flag. It is only intended for commands which pre-date multiple-line @if statements. See help Backwards for more information");
		return;
	}
	/* Everything is ok, do the set */
	if(*flag.c_str() == NOT_TOKEN)
	{
		/* reset the flag */
		db[thing].clear_flag(f);
		Modified (thing);
		if (!in_command())
			notify_colour(player,  player, COLOUR_MESSAGES, "Flag reset.");
	}
	else
	{
		/* set the flag */
		db[thing].set_flag(f);
		Modified (thing);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Flag set.");
	}
	if(f==FLAG_NO_EMAIL_FORWARD)
	{
		dump_email_addresses();
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_score (
const	String& victim_string,
const	String& value)

{
	dbref	victim;
	int	score;
	char	scratch_return_string [11];

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if (!Wizard (get_effective_id ()))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set a player's score.");
		return;
	}

	Matcher matcher (player, victim_string, TYPE_PLAYER, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_neighbor ();
	matcher.match_player ();
	matcher.match_me ();
	matcher.match_absolute ();
	if ((victim = matcher.noisy_match_result ()) == NOTHING)
		return;
	if ((Typeof(victim) != TYPE_PLAYER) && (Typeof(victim) != TYPE_PUPPET))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That is not a player.");
		return;
	}


	if (Wizard (victim))
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Everyone knows wizards don't care about scores.");
		return;
	}
	if (value && isdigit (*value.c_str()))
		score = atoi (value.c_str());
	else
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That is not a valid score.");
		return;
	}

	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "%s had %d points, and now has %d points.", getname (victim), db[victim].get_score (), score);
	}

	sprintf (scratch_return_string, "%ld",db[player].get_score ());
	db[victim].set_score (score);
	Modified (victim);
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void
context::do_at_volume (
const	String& object,
const	String& volume)
{
	dbref	victim;
	double	new_volume;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!object)
	{
		notify_colour (player,player, COLOUR_MESSAGES, "Set the volume of what?");
		return;
	}

	if (!volume)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the volume to what?");
		return;
	}
	
	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();
	if ((victim = matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the volume of a read-only object.");
		return;
	}

        if ((!controls_for_write (victim))
		|| ((Typeof(victim) == TYPE_PLAYER)
			&& (db[victim].get_inherited_volume () != STANDARD_PLAYER_VOLUME)
			&& (!Wizard (player))))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can do that.");
		return;
	}
	if ((Typeof(victim) != TYPE_PUPPET) && (Typeof(victim) != TYPE_THING) && (Typeof(victim) !=TYPE_PLAYER) && (Typeof(victim) != TYPE_ROOM))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That object has no volume attribute.");
		return;
	}

	if (string_prefix ("inherit", volume))
		new_volume = NUM_INHERIT;
	else
		new_volume = strtod (volume.c_str(), (char **)NULL);

	if ((new_volume <= 0) && (!Wizard(player)))
	{
		notify_colour(player,player, COLOUR_ERROR_MESSAGES, "Only Wizards can set a volume to that.");
		return;
	}

	if ((getloc (victim) != NOTHING) && (db[getloc (victim)].get_inherited_volume_limit () != HUGE_VAL) &&
	   ((new_volume - find_volume_of_object(victim)
		+ find_volume_of_contents(getloc(victim))) > find_volume_limit_of_object(getloc(victim))))
	{
		if (gagged_command ())
		{
			sprintf (scratch_buffer, "You can't make %s that large in", unparse_object (*this, victim).c_str());
			notify_censor_colour (player,player, COLOUR_ERROR_MESSAGES, "%s %s.", scratch_buffer, unparse_object (*this, getloc (victim)).c_str());
		}
		return;
	}

	db[victim].set_volume (new_volume);
	Modified (victim);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_volume_limit (
const	String& object,
const	String& volume_limit)
{
	dbref	victim;
	double	new_volume_limit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!object)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the volume limit of what?");
		return;
	}
	if (!volume_limit)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the volume limit to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();
	if ((victim = matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the volumelimit of a read-only object.");
		return;
	}

        if ((!controls_for_write (victim))
		|| ((Typeof(victim) == TYPE_PLAYER)
			&& (db[victim].get_inherited_volume_limit () != STANDARD_PLAYER_VOLUME_LIMIT)
			&& (!Wizard (player))))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set a volumelimit to that.");
		return;
	}

	if ((Typeof(victim) != TYPE_PUPPET) && (Typeof(victim) != TYPE_THING) && (Typeof(victim) != TYPE_PLAYER) && (Typeof(victim) != TYPE_ROOM))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That object has no volume limit attribute.");
		return;
	}

	// Allows mortals to set room values to infinity.
	if (string_prefix ("infinity", volume_limit)
	|| string_prefix ("infinite", volume_limit)
	|| string_prefix ("none", volume_limit)
	|| (atoi (volume_limit.c_str()) >= HUGE_VAL))
	{
		if (Wizard(player) || (Typeof(victim) == TYPE_ROOM))
			new_volume_limit = HUGE_VAL;
		else
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards have power over the infinite.");
			return;
		}
	}
	else if (string_prefix ("inherit", volume_limit))
		new_volume_limit = NUM_INHERIT;
	else
		new_volume_limit = strtod (volume_limit.c_str(), (char **)NULL);

	if ((Typeof (victim) == TYPE_PLAYER)
		&& (!Wizard (player))
		&& (new_volume_limit > STANDARD_PLAYER_VOLUME_LIMIT))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set a volumelimit that high.");
			return;
		}

	if ((new_volume_limit != HUGE_VAL) && (new_volume_limit < find_volume_of_contents (victim)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't have such a low volume limit with that object's current contents.");
		return;
	}

	db[victim].set_volume_limit (new_volume_limit);
	Modified (victim);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_mass (
const	String& object,
const	String& mass)
{
	dbref	victim;
	double	new_mass;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!object)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the mass of what?");
		return;
	}
	if (!mass)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the mass to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();
	if ((victim = matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the mass of a read-only object.");
		return;
	}

        if ((!controls_for_write (victim))
		|| ((Typeof(victim) == TYPE_PLAYER)
			&& (db[victim].get_inherited_mass () != STANDARD_PLAYER_MASS)
			&& (!Wizard (player))))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set a mass to that.");
		return;
	}

	if ((Typeof(victim) != TYPE_PUPPET) && (Typeof(victim) != TYPE_THING) && (Typeof(victim) != TYPE_PLAYER) && (Typeof(victim) != TYPE_ROOM))
	{
		notify_colour(player,  player, COLOUR_ERROR_MESSAGES, "That object has no mass attribute.");
		return;
	}

	if (string_prefix ("inherit", mass))
		new_mass = NUM_INHERIT;
	else
		new_mass = strtod (mass.c_str(), (char **)NULL);

	if ((new_mass <= 0) && (!Wizard(player)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if ((getloc (victim) != NOTHING)
	&& ((new_mass + find_mass_of_contents_except (getloc (victim), victim)) > find_mass_limit_of_object (getloc (victim))))
	{
		if (!gagged_command ())
		{
			sprintf (scratch_buffer, "You can't make %s that heavy in", unparse_object (*this, victim).c_str());
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "%s %s.", scratch_buffer, unparse_object (*this, getloc (victim)).c_str());
		}
		return;
	}

	db[victim].set_mass (new_mass);
	Modified (victim);

	if (!in_command())
		notify_colour (player,  player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_mass_limit (
const	String& object,
const	String& mass_limit)
{
	dbref	victim;
	double	new_mass_limit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!object)
	{
		notify_colour (player,  player, COLOUR_MESSAGES, "Set the mass limit of what?");
		return;
	}
	if (!mass_limit)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the mass limit to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();
	if ((victim = matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the masslimit of a read-only object.");
		return;
	}

        if ((!controls_for_write (victim))
	|| ((Typeof(victim) == TYPE_PLAYER)
		&& (db[victim].get_inherited_mass_limit () != STANDARD_PLAYER_MASS_LIMIT)
		&& (!Wizard (player))))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if ((Typeof(victim) != TYPE_PUPPET) && (Typeof(victim) != TYPE_THING) && (Typeof(victim) != TYPE_PLAYER) && (Typeof(victim) != TYPE_ROOM))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That object has no mass limit attribute.");
		return;
	}

	// Allows mortals to set room values to infinity.
	if (string_prefix ("infinity", mass_limit)
	|| string_prefix ("infinite", mass_limit)
	|| string_prefix ("none", mass_limit)
	|| (atoi (mass_limit.c_str()) >= HUGE_VAL))
	{
		if (Wizard(player) || (Typeof(victim) == TYPE_ROOM))
			new_mass_limit = HUGE_VAL;
		else
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards have power over the infinite.");
			return;
		}
	}
	else if (string_prefix ("inherit", mass_limit))
		new_mass_limit = NUM_INHERIT;
	else
		new_mass_limit = strtod (mass_limit.c_str(), (char **)NULL);

	if ((Typeof (victim) == TYPE_PLAYER)
		&& (!Wizard (player))
		&& (new_mass_limit > STANDARD_PLAYER_MASS_LIMIT))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
			return;
		}

	if ((new_mass_limit != HUGE_VAL) && (new_mass_limit < find_mass_of_contents_except (victim, NOTHING)))
	{
		notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "You can't have such a low mass limit with that object's current contents.");
		return;
	}

	db[victim].set_mass_limit (new_mass_limit);
	Modified (victim);

	if (!in_command())
		notify_colour (player,  player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_gravity_factor (const String& object, const String& gravity_factor)
{
	dbref	victim;
	double	new_gravity_factor;
	double	old_gravity_factor;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!object)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the gravity factor of what?");
		return;
	}
	if (!gravity_factor)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the gravity factor to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		matcher.work_silent ();
	matcher.match_everything();
	if ((victim = matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the gravityfactor of a read-only object.");
		return;
	}

        if ((!controls_for_write (victim))
		|| ((Typeof(victim) == TYPE_PLAYER)
			&& (db[victim].get_inherited_gravity_factor () != STANDARD_PLAYER_GRAVITY)
			&& (!Wizard (player))))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if ((Typeof(victim) != TYPE_PUPPET) && (Typeof(victim) != TYPE_THING) && (Typeof(victim) != TYPE_PLAYER) && (Typeof(victim) != TYPE_ROOM))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That object has no gravity factor attribute.");
		return;
	}

	if (string_prefix ("inherit", gravity_factor))
		new_gravity_factor = NUM_INHERIT;
	else
		new_gravity_factor = strtod (gravity_factor.c_str(), (char **)NULL);

	if ((new_gravity_factor < 1) && (!Wizard (player)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	old_gravity_factor = db[victim].get_gravity_factor (); /* NOT inherited! */
	db[victim].set_gravity_factor (new_gravity_factor);

	if ((db[victim].get_inherited_mass_limit () != HUGE_VAL)
	&& (db[victim].get_inherited_mass_limit () < find_mass_of_contents_except (victim, NOTHING)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't set the gravity factor such that the object can no longer hold its contents.");
		db[victim].set_gravity_factor (old_gravity_factor);
		return;
	}

	Modified (victim);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_location (
const	String& victim_string,
const	String& new_location_string)

{
	dbref			victim;
	dbref			new_location;
	enum	fit_result	fit_error;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher victim_matcher (player, victim_string, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		victim_matcher.work_silent ();
	victim_matcher.match_everything ();
	if ((victim = victim_matcher.noisy_match_result ()) == NOTHING)
		return;

	if (!new_location_string)
	{
		new_location=victim;
		victim = player;
	}
	else
	{
		Matcher loc_matcher (player, new_location_string, TYPE_NO_TYPE, get_effective_id ());
		if (gagged_command ())
			loc_matcher.work_silent ();
		loc_matcher.match_everything ();
		if ((new_location = loc_matcher.noisy_match_result ()) == NOTHING)
			return;
	}

	/* Frig player teleport */
	if (((Typeof(victim) == TYPE_PUPPET) || (Typeof (victim) == TYPE_PLAYER)) && ((Typeof(new_location) == TYPE_PUPPET) || (Typeof (new_location) == TYPE_PLAYER)))
		new_location = db[new_location].get_location();

	if (((fit_error=will_fit(victim,new_location)) != SUCCESS && (new_location != db[victim].get_location())) && (Typeof(victim) != TYPE_ROOM))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, the teleport failed - %s", fit_errlist [fit_error]);
		return;
	}

	/* Give a nicer message when we try to teleport a read-only object */
	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the location of a read-only object.");
		return;
	}

	/* Check controllership */
	switch (Typeof (victim))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_ROOM:
			if ((!controls_for_write (new_location)
			||   !controls_for_write (victim)) && !Link(new_location) && !Apprentice(get_effective_id()))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You must either control the destination, or it must have its Link_OK flag set.");
				return;
			}
			break;
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if ((victim==player) && (Welcomer(victim)))
				break;
			if ((victim!=player) && !Wizard(get_effective_id()) && !Apprentice(get_effective_id()) && !controls_for_write (victim))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
				return;
			}
			if ((!controls_for_read (new_location)
				|| ((Typeof (new_location) == TYPE_THING) && !(controls_for_write (db[new_location].get_location()))))
				&& !(Abode(new_location) || Jump (new_location)))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only @teleport to a room that you own, or a room with its 'jump' or 'abode' flag set");
				return;
			}
			break;
		case TYPE_THING:
			if (
				/* if they don't have controls-for-write AND (it's not a wiz-chpid where the new location is a room) */
				(!controls_for_write(new_location) && !(Wizard(get_effective_id()) && Typeof(new_location) == TYPE_ROOM))
				||
				!(controls_for_write (victim) || db[victim].get_location() == NOTHING || controls_for_read (db [victim].get_location()))
				||
				((Typeof (new_location) == TYPE_THING) && !(controls_for_write (db[new_location].get_location()))))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
				return;
			}
			break;
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_PROPERTY:
		case TYPE_VARIABLE:
			/* I hope this is right: You have to 'own' the var and the destination */
			if (!controls_for_write (new_location)
				|| !controls_for_write (victim))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
				return;
			}
			break;
		default:
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "What on earth is that type of object?");
			return;
	}

	/* Check types */
	switch (Typeof (victim))
	{
		case TYPE_ALARM:
			switch (Typeof (new_location))
			{
				case TYPE_PLAYER:
				case TYPE_PUPPET:
				case TYPE_ROOM:
				case TYPE_THING:
					break;
				default:
					notify_colour (player, player, COLOUR_MESSAGES, "Only Players, Rooms and Things can have Alarms attached to them.");
					return;
			}
			break;
		case TYPE_COMMAND:
			switch (Typeof (new_location))
			{
				case TYPE_PLAYER:
				case TYPE_PUPPET:
				case TYPE_ROOM:
				case TYPE_THING:
					break;
				default:
					notify_colour (player, player, COLOUR_MESSAGES, "Only Players, Rooms and Things can have Commands attached them.");
					return;
			}
			break;
		case TYPE_EXIT:
			switch (Typeof (new_location))
			{
				case TYPE_THING:
				case TYPE_ROOM:
				case TYPE_PLAYER:
					break;
				default:
					notify_colour (player,  player, COLOUR_MESSAGES, "Only Things, Rooms and Players can have Exits inside them.");
					return;
			}
			break;
		case TYPE_FUSE:
			switch (Typeof (new_location))
			{
				case TYPE_THING:
				case TYPE_ROOM:
				case TYPE_EXIT:
				case TYPE_PLAYER:
				case TYPE_PUPPET:
					break;
				default:
					notify_colour (player, player, COLOUR_MESSAGES, "Only Exits, Players, Rooms and Things can have Fuses attached to them.");
					return;
			}
			break;
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			switch (Typeof (new_location))
			{
				case TYPE_ROOM:
				case TYPE_THING:
					if (contains (new_location, victim))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That's a physical impossibility.");
						return;
					}
					break;
				default:
					notify_colour (player, player, COLOUR_MESSAGES, "Only Rooms and Things can have Players inside them.");
					return;
			}
			break;
		case TYPE_ROOM:
			switch (Typeof (new_location))
			{
				case TYPE_ROOM:
					if (in_area (new_location, victim)
					   && (new_location != victim))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That's a physical impossibility.");
						return;
					}
					break;
				default:
					notify_colour (player,  player, COLOUR_MESSAGES,"Only Rooms can have Rooms inside them.");
					return;
			}
			break;
		case TYPE_THING:
			switch (Typeof (new_location))
			{
				case TYPE_THING:
				case TYPE_ROOM:
				case TYPE_PLAYER:
				case TYPE_PUPPET:
					if (contains (new_location, victim))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That's a physical impossibility.");
						return;
					}
					break;
				default:
					notify_colour (player, player, COLOUR_MESSAGES, "Only Things, Rooms and Players can have Things inside them.");
					return;
			}
			break;
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_PROPERTY:
		case TYPE_VARIABLE:
			switch (Typeof (new_location))
			{
				case TYPE_EXIT:
				case TYPE_COMMAND:
				case TYPE_PLAYER:
				case TYPE_PUPPET:
				case TYPE_ROOM:
				case TYPE_THING:
					break;
				default:
					notify_colour (player,  player, COLOUR_MESSAGES, "Only Players, Rooms and Things can have Information Objects attached to them.");
					return;
			}
			break;
		default:
			notify_colour (player,  player, COLOUR_MESSAGES, "That object can't be moved.");
			log_bug("do_at_location tried to move an invalid object type (#%d)", victim);
			return;
	}

	/* If we've got this far, it's legal. (but they might not fit in the destination) */

	/* Frig the destination if we're teleporting a room to itself */

	if (new_location == victim)
		new_location = NOTHING;

	/* Move victim to new_location */
	if ((Typeof (victim) == TYPE_PLAYER) || (Typeof (victim) == TYPE_PUPPET))
	{
		if (will_fit (victim, new_location)==SUCCESS)
		{
			context	*victim_context = new context (victim, *this);
			victim_context->enter_room (new_location);
			delete mud_scheduler.push_new_express_job (victim_context);
		}
		else
		{
			if (player == victim)
			{
				notify_censor_colour(player,  player, COLOUR_MESSAGES, "To teleport you need enough free space to arrive in. There isn't enough in %s", getname(new_location));
				return;
			}
			else
			{
				notify_censor_colour(player, player, COLOUR_ERROR_MESSAGES,  "There isn't enough room for %s in %s", getname(victim), getname(new_location));
				return;
			}
		}
	}
	else
	{
		if (will_fit(victim, new_location)==SUCCESS)
			moveto (victim, new_location);
		else
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That won't go in there - %s", fit_errlist [fit_error]);
			return;
		}
	}

	Accessed (victim);
	Accessed (new_location);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Located.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_key (
const	String& object,
const	String& keyname)

{
	dbref	container;
	boolexp	*key;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher container_matcher (player, object, TYPE_THING, get_effective_id ());
	if (gagged_command ())
		container_matcher.work_silent ();
	container_matcher.match_possession ();
	container_matcher.match_neighbor ();
	container_matcher.match_here ();
	container_matcher.match_absolute ();

	if ((container = container_matcher.noisy_match_result()) == NOTHING)
		return;

	if (Readonly(container))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change the key of a read-only object.");
		return;
	}

	if (!controls_for_write (container))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot give a key to an object you don't own.");
	else if (!Container (container))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot give that thing a key.");
	else
	{
		if (!keyname)
			key = TRUE_BOOLEXP;
		else if ((key = parse_boolexp (player, keyname)) == TRUE_BOOLEXP)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "I don't understand that key.");
			return;
		}

		db[container].set_lock_key (key);
		Modified (container);

		if (!in_command())
		{
			if (!keyname)
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s can no longer be locked.", unparse_object (*this, container).c_str());
			else
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s can now be locked.", unparse_object (*this, container).c_str());
		}
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_at_parent (
const	String& name,
const	String& parent_name)

{
	dbref	thing;
	dbref	parent;
	dbref	test_parent;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) == NOTHING)
		return;

	if ((Typeof(thing) == TYPE_PROPERTY) || (Typeof(thing) == TYPE_VARIABLE))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Properties cannot be inherited.");
		return;
	}

	if ((Typeof(thing) == TYPE_PLAYER) && is_guest(thing))
	{
		notify_colour(player,  player, COLOUR_ERROR_MESSAGES, "Sorry, but guests cannot be reparented.");
		return;
	}
	/* If the second arg is blank, unparent */
	if (!parent_name)
	{
		db [thing].set_parent (NOTHING);
		Modified (thing);
		if (!in_command ())
			notify_colour (player, player, COLOUR_MESSAGES, "Unlinked.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		return;
	}

	Matcher	parent_matcher (player, parent_name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		parent_matcher.work_silent ();
	parent_matcher.match_everything ();
	if ((parent = parent_matcher.noisy_match_result ()) == NOTHING)
		return;

	/* Can only inherit from an object of the same type */
	if (Typeof (parent) != Typeof (thing))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only inherit from a parent of the same type as the child.");
		return;
	}

	/* If we don't control the parent, it had better be Inheritable */
	if (!(Inheritable (parent) || controls_for_read (parent)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You don't control that object and its Inheritable flag is not set.");
		return;
	}

	/* Finally, we'd better check for whether we're about to create a loop of parents */
	/* (this is one that *doesn't* happen in Real Life (TM)) */
	for (test_parent = parent; test_parent != NOTHING; test_parent = db [test_parent].get_parent ())
		if (thing == test_parent)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Can't parent an object to something with itself as an ancestor.");
			return;
		}

	/* Looks like it's all OK */
	db [thing].set_parent (parent);
	db [parent].set_referenced();
	Modified (thing);
	Accessed (parent);
	if (!in_command ())
		notify_colour (player, player, COLOUR_MESSAGES, "Linked.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void context::do_at_credit(const String& arg1, const String& arg2)
{
	dbref victim;
	int amount;
	char *where;
	char scanned_name[BUFFER_LEN];
	const char *currency_name=CURRENCY_NAME;

	if(!arg1 || !arg2)
	{
		notify(player, "Usage:  @credit <player> = <currency type>;<amount to give>");
		RETURN_FAIL;
	}

	if(in_command() && !Wizard(get_current_command()))
	{
		notify(player, permission_denied.c_str());
		RETURN_FAIL;
	}

        strcpy(scanned_name, arg2.c_str());
        if ((where=strchr(scanned_name, ';')) == 0)
	{
		notify(player, "Usage:  @credit <player> = <currency type>;<amount to give>");
		RETURN_FAIL;
	}
	*where = '\0';
	if((amount=atoi(++where)) == 0)
	{
		notify(player, "You must specify a non-zero amount to transfer.");
		RETURN_FAIL;
	}

	if (string_compare(scanned_name, currency_name) != 0)
	{
		notify(player, "That is not a valid currency to credit.");
		RETURN_FAIL;
	}

	if(amount<0)
	{
		notify(player, "You can't give negative money - use @debit instead");
		RETURN_FAIL;
	}

	if(!MoneyMaker(get_effective_id()) && (amount>db[get_effective_id()].get_money()))
	{
		notify(player, "You don't have that much to give away.");
		RETURN_FAIL;
	}

	Matcher matcher(player, arg1, TYPE_PLAYER, get_effective_id());
	matcher.match_neighbor();
	matcher.match_me();
	matcher.match_player();
	matcher.match_absolute();

	if((victim=matcher.noisy_match_result())==NOTHING)
		RETURN_FAIL;
	
	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change exchange credits with a read-only player.");
		return;
	}

	if ((Typeof(victim)!=TYPE_PLAYER) && (Typeof(victim)!=TYPE_PUPPET))
	{
		notify(player, "You can only give money to players and puppets.");
		RETURN_FAIL;
	}

	if(amount<0 && (db[victim].get_money()+amount)<0)
	{
		notify(player, "You can't make someone go into the red.");
		RETURN_FAIL;
	}

	if(get_effective_id()==victim)
	{
		notify(player, "You would only be stealing from yourself.");
		RETURN_FAIL;
	}

	db[get_effective_id()].set_money(db[get_effective_id()].get_money()-amount);
	db[victim].set_money(db[victim].get_money()+amount);
	Accessed (get_effective_id());
	Accessed (victim);

	if(!in_command())
	{
		notify(player, "You give %d %s to %s.", amount, currency_name, db[victim].get_name().c_str());
		notify(victim, "%s gives you %d %s.", db[player].get_name().c_str(), amount, currency_name);
	}
	log_credit(get_effective_id(), db[get_effective_id()].get_name().c_str(), victim, db[victim].get_name().c_str(), amount, currency_name);

	RETURN_SUCC;
}

void context::do_at_debit(const String& arg1, const String& arg2)
{
	dbref victim;
	int amount;
	char *where;
	char scanned_name[BUFFER_LEN];
	const char *currency_name=CURRENCY_NAME;

	if(!arg1 || !arg2)
	{
		notify(player, "Usage:  @debit <player> = <currency type>;<amount to give>");
		RETURN_FAIL;
	}


	if(!Apprentice(player) && !Wizard(get_effective_id()))
	{
		notify(player, permission_denied.c_str());
               RETURN_FAIL;
	}

	if(in_command())
	{
		notify(player, permission_denied.c_str());
		RETURN_FAIL;
	}

        strcpy(scanned_name, arg2.c_str());
        if ((where=strchr(scanned_name, ';')) == 0)
	{
		notify(player, "Usage:  @debit <player> = <currency type>;<amount to give>");
		RETURN_FAIL;
	}
	*where = '\0';
	if((amount=atoi(++where)) == 0)
	{
		notify(player, "You must specify a non-zero amount to transfer.");
		RETURN_FAIL;
	}

	if (string_compare(scanned_name, currency_name) != 0)
	{
		notify(player, "That is not a valid currency to debit.");
		RETURN_FAIL;
	}

	if(amount<0)
	{
		notify(player, "So you want me to take away negative money? Use @credit instead.");
		RETURN_FAIL;
	}

	Matcher matcher(player, arg1, TYPE_PLAYER, get_effective_id());
	matcher.match_neighbor();
	matcher.match_me();
	if(Wizard(player) || Apprentice(player))
	{
		matcher.match_player();
		matcher.match_absolute();
	}

	if((victim=matcher.noisy_match_result())==NOTHING)
		RETURN_FAIL;
	
	if (Readonly(victim))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't change exchange credits with a read-only player.");
		return;
	}

	if ((Typeof(victim)!=TYPE_PLAYER) && (Typeof(victim)!=TYPE_PUPPET))
	{
		notify(player, "You can only take money from players and puppets.");
		RETURN_FAIL;
	}

	if(!controls_for_write(victim))
	{
		notify(player, "Permission denied.");
		RETURN_FAIL;
	}

	if((db[victim].get_money()-amount)<0)
	{
		notify(player, "You can't make someone go into the red.");
		RETURN_FAIL;
	}

	if(get_effective_id()==victim)
	{
		notify(player, "You would only be stealing from yourself.");
		RETURN_FAIL;
	}

	db[get_effective_id()].set_money(db[get_effective_id()].get_money()+amount);
	db[victim].set_money(db[victim].get_money()-amount);
	Accessed (get_effective_id());
	Accessed (victim);

	if(!in_command())
	{
		notify(player, "You take %d %s from %s.", amount, currency_name, db[victim].get_name().c_str());
		notify(victim, "%s takes %d %s from you.", db[player].get_name().c_str(), amount, currency_name);
	}
	log_debit(victim, db[victim].get_name().c_str(), get_effective_id(), db[get_effective_id()].get_name().c_str(), amount, currency_name);

	RETURN_SUCC;
}

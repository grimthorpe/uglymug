static char SCCSid[] = "@(#)set.c 1.108 99/03/02@(#)";
#include "copyright.h"

/* commands which set parameters */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include "db.h"
#include "config.h"
#include "match.h"
#include "interface.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "colour.h"


static dbref
match_controlled (
context		&c,
const	char	*name)
{
	dbref match;

	Matcher matcher (c.get_player (), name, TYPE_NO_TYPE, c.get_effective_id ());
	if (c.gagged_command () == True)
		matcher.work_silent ();
	matcher.match_everything();

	match = matcher.noisy_match_result();

	if(match != NOTHING && !c.controls_for_write (match))
	{
		notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, "You cannot modify an object with the ReadOnly flag set.");
		return NOTHING;
	}

	if(match != NOTHING && !c.controls_for_write (match))
	{
		notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, permission_denied);
		return NOTHING;
	}

	return match;
}

void
context::do_at_insert (
const	char	*what,
const	char	*element)
{
	/* I'm not catering for Dictionaries because it would need an extra
	   argument and Ian said 'Just put the code in, please, pretty please
	   with sugar on top' */

	dbref	thing;
	int	typething;
	int	index;

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
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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

	char *indexString = what_matcher.match_index_result();
	if ( indexString == NULL )
	{
		notify_colour ( player, player, COLOUR_ERROR_MESSAGES, "@insert requires an index." );
		RETURN_FAIL;
	}

	index = atoi( indexString );
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
		int limit;
		
		case TYPE_ARRAY:
			if(Wizard(thing))
				limit = MAX_WIZARD_ARRAY_ELEMENTS;
			else
				limit = MAX_MORTAL_ARRAY_ELEMENTS;
			break;

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
			break;

			if((int)(strlen(element) + db[thing].get_size()) > limit)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, that line makes the description too big.");
				RETURN_FAIL;
			}
			break;
	}


	/* Do it */
	db[thing].insert_element(index, element);
	if (!gagged_command() && !in_command())
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Inserted.");
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
const	char	*array,
const	char	*direction)
{
	dbref	thing;
	int	direct;

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
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
	RETURN_SUCC;
}

void
context::do_peak
(const	char	*number,
const	char	*)
{
	if(!Wizard(player))
	{
		notify(player, "Don't be silly.");
		return;
	}
	if(!blank(number))
	{
		peak_users = atoi(number);
		notify(player, "Peak set to %d.", peak_users);
	}

	RETURN_SUCC;
}

void
context::do_race (
const	char	*name,
const	char	*newrace)

{
	dbref	thing;
	char	*check;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, name)) == NOTHING)
		return;
	if ((Typeof(thing) != TYPE_PLAYER) && (Typeof(thing) != TYPE_PUPPET))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Only Players have races.");
		return;
	}
	if(!ok_name(newrace))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That is not a reasonable race.");
		return;
	}	

	if ((check = strchr(newrace, '\n')))
	{	
		*check = '\0';
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "WARNING: Race truncated at newline.");
	}

	/* Assuming all OK, changing race... */
	db[thing].set_race (newrace);
	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES,"Race Set.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_name (
const	char	*name,
const	char	*newname)

{
	dbref	thing;
	int	value;
	time_t	now;
	int	temp = 0;

	newname = value_or_empty(newname);

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
				if(!Wizard(get_effective_id ()) && !Wizard(player) && !Apprentice(player) && ((temp = now - db[thing].get_last_name_change()) < NAME_TIME))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you can't change your name for another %d minute%s.", (NAME_TIME - temp)/60 + 1, PLURAL((NAME_TIME - temp)/60 + 1));
					return;
				}

					
				if (!ok_player_name(newname) && (strcasecmp(newname, db[thing].get_name()))) // Allow the same name, for re-caps
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't give a player that name.");
					return;
				}
				db[thing].set_last_name_change(now);
#ifdef LOG_NAME_CHANGES
				Trace( "NAME CHANGE: %s to %s\n", db[thing].get_name(), newname);
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
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Dictionary \"%s\" does not contain element \"%s\".", db[thing].get_name(), matcher.match_index_result());
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
			if (newname == NULL || *newname == '\0')
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

	if(Typeof(thing)==TYPE_PLAYER)
		db.add_player_to_cache(thing, newname);

	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Name set.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_describe (
const	char	*name,
const	char	*description)

{
	int	value;
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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

	switch (Typeof (thing))
	{
		case TYPE_FUSE:
			value = atoi (value_or_empty (description));
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
			if(!blank(matcher.match_index_result()))
				if((value = db[thing].exist_element(matcher.match_index_result())) > 0)
					db[thing].set_element(value, matcher.match_index_result(), description);
				else
				{
					if((db[thing].get_number_of_elements() + 1) <= ((Wizard(thing)) ? MAX_WIZARD_DICTIONARY_ELEMENTS : MAX_MORTAL_DICTIONARY_ELEMENTS))
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
				if(!blank(matcher.match_index_result()))
				{
					if((value = atoi (value_or_empty (matcher.match_index_result()))) > 0)
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
				if(!blank(matcher.match_index_result()))
				{
					if((value = atoi (value_or_empty (matcher.match_index_result()))) > 0)
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
				db[thing].set_description(description);
			db[thing].flush_parse_helper();
			break;

		default:
			db[thing].set_description(description);
			break;
	}


	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Description set.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_fail (
const	char	*name,
const	char	*message)

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
				Trace("BUG: @fail used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());

			default:
				break;
		}

		db[thing].set_fail_message(message);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Message set.");

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_success (
const	char	*name,
const	char	*message)

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
				Trace("BUG: @success used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:
				break;
			
		}

		db[thing].set_succ_message(message);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Message set.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);

	}
}

void
context::do_at_drop (
const	char	*name,
const	char	*message)

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
				if ((message != NULL) && (*message != '\0'))
				{
					if (Typeof (thing) == TYPE_FUSE)
					{
						value = atoi (message);
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
				Trace("BUG: @drop used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:
				break;
		}

		db[thing].set_drop_message(message);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Set.");
	}
}

void
context::do_osuccess (
const	char	*name,
const	char	*message)

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
				Trace("BUG: @osuccess used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:
				break;
		}

		db[thing].set_osuccess(message);
		if (!in_command())
			notify_colour(player,  player, COLOUR_MESSAGES, "Message set.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_ofail (
const	char	*name,
const	char	*message)

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
				Trace("BUG: @ofail used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:
				break;
		}

		db[thing].set_ofail(message);
		if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Message set.");

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_odrop (
const	char	*name,
const	char	*message)

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
				Trace("BUG: @odrop used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:
				break;
		}
			
		db[thing].set_odrop(message);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Message set.");

		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_at_lock (
const	char	*name,
const	char	*keyname)

{
	dbref	thing;
	boolexp	*key;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher name_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
				Trace("BUG: @lock used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:

				if (keyname == NULL || (key = parse_boolexp (player, keyname)) == TRUE_BOOLEXP)
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
					if (!in_command())
						notify_colour(player, player, COLOUR_MESSAGES, "Locked.");

					return_status = COMMAND_SUCC;
					set_return_string (ok_return_string);
				}
	}
}


void
context::do_at_unlock (
const	char	*name,
const	char	*)

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
				Trace("BUG: @unlock used with variable #%d by player #%d in command #%d\n", thing, player, get_current_command());
			default:
				db[thing].set_key(TRUE_BOOLEXP);
				if (!in_command())
					notify_colour(player,  player, COLOUR_MESSAGES, "Unlocked.");

				return_status = COMMAND_SUCC;
				set_return_string (ok_return_string);
		}
	}
}

void
context::do_unlink (
const	char	*name,
const	char	*)

{
	dbref exit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, name, TYPE_EXIT, get_effective_id ());
	if (gagged_command () == True)
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
			if (!in_command())
				notify_colour(player, player, COLOUR_MESSAGES, "Unlinked.");
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			break;
		case TYPE_ROOM:
			db[exit].set_destination(NOTHING);
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
context::do_owner (
const	char	*name,
const	char	*newowner)

{
	dbref	thing;
	dbref	object;
	dbref	owner;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (name == NULL || !*name)
	{
		notify_colour(player,  player, COLOUR_MESSAGES, "Change ownership of what?");
		return;
	}

	Matcher name_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
	if(newowner != NULL && *newowner)
	{
		Matcher owner_matcher (player, newowner, TYPE_PLAYER, get_effective_id ());
		if (gagged_command () == True)
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
					notify_colour(player, player, COLOUR_ERROR_MESSAGES,  permission_denied);
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
		else if ((player != GOD_ID)
			&& ((Wizard (owner) && owner != player)
				|| (Wizard (thing) && (thing != player))))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, permission_denied);
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
	if (!in_command())
		notify_colour(player,  player, COLOUR_MESSAGES, "Owner changed.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_set (
const	char	*name,
const	char	*flag)

{
	dbref			thing;
	const	char		*p;
	object_flag_type	f;
	int			i;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	/* find thing - don't use match_controlled so we can unset an (r) flag! */
	Matcher matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
		matcher.work_silent ();
	matcher.match_everything ();

	if ((thing = matcher.noisy_match_result ()) == NOTHING)
		return;

	/* move p past NOT_TOKEN if present */
	for(p = flag; p && *p && (*p == NOT_TOKEN || isspace(*p)); p++)
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
			if ((flag_list[i].flag == FLAG_ERIC) || (flag_list[i].flag == FLAG_OFFICER))
			{
				notify_colour(player,player,COLOUR_ERROR_MESSAGES, "The Eric flag is obsolete. Please don't try to set it.");
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

	if (*flag != NOT_TOKEN)
		if (!is_flag_allowed(Typeof(thing), f))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't apply that flag to that type of object.");
			return;
		}
#if 0

	if ((!(type_to_flag_map [Linear_typeof (thing)] & f)) & (*flag != NOT_TOKEN))
#endif

	if ((f == FLAG_READONLY) && (*flag == NOT_TOKEN))
	{
		db[thing].clear_flag(f);
		if (!controls_for_write (thing))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, permission_denied);
			db[thing].set_flag(f);
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

	if ((f==FLAG_NOHUHLOGS) && (*flag != NOT_TOKEN))
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

	if (!controls_for_write (thing))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	if((f==FLAG_MALE) || (f==FLAG_FEMALE) || (f==FLAG_NEUTER))
		if(*flag==NOT_TOKEN)
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
			
		
	/* check for restricted flag */
	if(!Wizard(get_effective_id ())
		&& (f == FLAG_WIZARD))
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
		    (!Wizard (get_effective_id ()) && (thing == player) && (*flag != NOT_TOKEN)))
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "Only Wizards may set the 'ashcan' flag.");
			return;
		}

		if ((thing == GOD_ID) || (thing == COMMAND_LAST_RESORT) || (thing == LIMBO) || (thing == PLAYER_START))
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
		if (*flag == NOT_TOKEN)
		{
			if (!Wizard (get_effective_id ()))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may change the Fighting flag.");
				return;
			}
		}
		else if (Typeof(thing) == TYPE_PLAYER)
		{
			db[thing].set_dexterity(10+(lrand48() % 10));	
			db[thing].set_strength(10+(lrand48() % 10));	
			db[thing].set_constitution(10+(lrand48() % 10));	
			db[thing].set_max_hit_points(10+(lrand48() % 10)+db[thing].get_constitution()/5);
			db[thing].set_hit_points(db[thing].get_max_hit_points());
		}	
				
#if 0	/* For soft-coded fighting... */
		if (in_command())
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot set the FIGHTING flag in a command");
			return;
		}
#endif
	}

	if ((f == FLAG_DARK) && (!Wizard (get_effective_id ())))
	{
		switch (Typeof (thing))
		{
			case TYPE_THING:
				if (Builder(player))
					break;
				/* FALLTHROUGH */
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
		if (*flag != NOT_TOKEN)
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "WARNING: Other players can now change ownership of objects to you without your permission!");
	}

	/* check for stupid wizard */
	if(f == FLAG_WIZARD && *flag == NOT_TOKEN && thing == player)
	{
		notify_colour(player,  player, COLOUR_ERROR_MESSAGES, "You cannot make yourself mortal.");
		return;
	}

	if(f == FLAG_WIZARD && in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You just tried to execute an @set WIZARD in a command.");
		return;
	}

#ifndef	NO_GOD
	if((f == FLAG_WIZARD) && (player != GOD_ID) && (Typeof (thing) == TYPE_PLAYER))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "Only God may make Wizards.");
		return;
	}
#endif	/* NO_GOD */
	if (f == FLAG_WIZARD && (Typeof(thing) == TYPE_PLAYER) && Puppet(thing))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot set a puppet to be a wizard.");
		return;
	}
	if (f == FLAG_WIZARD && !Wizard(player))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards can set that flag.");
		return;
	}
	/* Check you can't trash a container with things in it */
	if ((*flag == NOT_TOKEN)
		&& (db[thing].get_contents() != NOTHING)
		&& (Container (thing))
		&& ((f == FLAG_OPEN) || (f == FLAG_OPENABLE)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Remove the contents, then try again.");
		return;
	}


#ifdef ABORT_FUSES
	/* Only Wizards can set abort fuses, and we log it. */

	if ((f==FLAG_ABORT) && (*flag != NOT_TOKEN))
	{
		if (!Wizard(get_effective_id()))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only wizards may create Abort fuses.");
			return;
		}
		if ( (get_current_command() != NOTHING) && (!Wizard(get_current_command())))
		{
			Trace("HACK - Attempt to set ABORT flag on %d by %d in command %d\n", thing, player, get_current_command());
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Abort fuses may only be created in commands if the command is set Wizard.");
			return;
		}
	}
#endif

	/* Log every WIZARD flag set */
	if ((f == FLAG_WIZARD) && (*flag != NOT_TOKEN))
	{
		if (Typeof(thing) == TYPE_COMMAND)
			Trace("HACK - <<<WIZARD>>> flag set on %s(%d) by %s(%d)\n",getname(thing),thing,getname(player),player);
		else
			Trace("PROTECT - WIZARD flag set on %s(%d) by %s(%d)\n",getname(thing),thing,getname(player),player);
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
	if ((f==FLAG_BACKWARDS) && (*flag != NOT_TOKEN))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot set the Backwards flag. It is only intended for commands which pre-date multiple-line @if statements. See help Backwards for more information");
		return;
	}
	/* Everything is ok, do the set */
	if(*flag == NOT_TOKEN)
	{
		/* reset the flag */
		db[thing].clear_flag(f);
		if (!in_command())
			notify_colour(player,  player, COLOUR_MESSAGES, "Flag reset.");
	}
	else
	{
		/* set the flag */
		db[thing].set_flag(f);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Flag set.");
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_score (
const	char	*victim_string,
const	char	*value)

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
	if (gagged_command () == True)
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
	if (value && isdigit (*value))
		score = atoi (value);
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
	return_status = COMMAND_SUCC;
	set_return_string (scratch_return_string);
}

void
context::do_volume (
const	char	*object,
const	char	*volume)
{
	dbref	victim;
	double	new_volume;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (object == NULL)
	{
		notify_colour (player,player, COLOUR_MESSAGES, "Set the volume of what?");
		return;
	}

	if (volume == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the volume to what?");
		return;
	}
	
	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
		new_volume = strtod (volume, (char **)NULL);

	if ((new_volume <= 0) && (!Wizard(player)))
	{
		notify_colour(player,player, COLOUR_ERROR_MESSAGES, "Only Wizards can set a volume to that.");
		return;
	}

	if ((getloc (victim) != NOTHING) && (db[getloc (victim)].get_inherited_volume_limit () != HUGE_VAL) &&
	   ((new_volume - find_volume_of_object(victim)
		+ find_volume_of_contents(getloc(victim))) > find_volume_limit_of_object(getloc(victim))))
	{
		if (gagged_command () == False)
		{
			sprintf (scratch_buffer, "You can't make %s that large in", unparse_object (*this, victim));
			notify_censor_colour (player,player, COLOUR_ERROR_MESSAGES, "%s %s.", scratch_buffer, unparse_object (*this, getloc (victim)));
		}
		return;
	}

	db[victim].set_volume (new_volume);

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_volume_limit (
const	char	*object,
const	char	*volume_limit)
{
	dbref	victim;
	double	new_volume_limit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (object == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the volume limit of what?");
		return;
	}
	if (volume_limit == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the volume limit to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
	|| (atoi (value_or_empty (volume_limit)) >= HUGE_VAL))
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
		new_volume_limit = strtod (volume_limit, (char **)NULL);

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

	if (!in_command())
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_mass (
const	char	*object,
const	char	*mass)
{
	dbref	victim;
	double	new_mass;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (object == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the mass of what?");
		return;
	}
	if (mass == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the mass to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
		new_mass = strtod (mass, (char **)NULL);

	if ((new_mass <= 0) && (!Wizard(player)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	if ((getloc (victim) != NOTHING)
	&& ((new_mass + find_mass_of_contents_except (getloc (victim), victim)) > find_mass_limit_of_object (getloc (victim))))
	{
		if (gagged_command () == False)
		{
			sprintf (scratch_buffer, "You can't make %s that heavy in", unparse_object (*this, victim));
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "%s %s.", scratch_buffer, unparse_object (*this, getloc (victim)));
		}
		return;
	}

	db[victim].set_mass (new_mass);

	if (!in_command())
		notify_colour (player,  player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_mass_limit (
const	char	*object,
const	char	*mass_limit)
{
	dbref	victim;
	double	new_mass_limit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (object == NULL)
	{
		notify_colour (player,  player, COLOUR_MESSAGES, "Set the mass limit of what?");
		return;
	}
	if (mass_limit == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the mass limit to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
	|| (atoi (value_or_empty (mass_limit)) >= HUGE_VAL))
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
		new_mass_limit = strtod (mass_limit, (char **)NULL);

	if ((Typeof (victim) == TYPE_PLAYER)
		&& (!Wizard (player))
		&& (new_mass_limit > STANDARD_PLAYER_MASS_LIMIT))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
			return;
		}

	if ((new_mass_limit != HUGE_VAL) && (new_mass_limit < find_mass_of_contents_except (victim, NOTHING)))
	{
		notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "You can't have such a low mass limit with that object's current contents.");
		return;
	}

	db[victim].set_mass_limit (new_mass_limit);

	if (!in_command())
		notify_colour (player,  player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_gravity_factor (const char *object, const char *gravity_factor)
{
	dbref	victim;
	double	new_gravity_factor;
	double	old_gravity_factor;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (object == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the gravity factor of what?");
		return;
	}
	if (gravity_factor == NULL)
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Set the gravity factor to what?");
		return;
	}

	Matcher	matcher (player, object, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
		new_gravity_factor = strtod (gravity_factor, (char **)NULL);

	if ((new_gravity_factor < 1) && (!Wizard (player)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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

	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_location (
const	char	*victim_string,
const	char	*new_location_string)

{
	dbref			victim;
	dbref			new_location;
	enum	fit_result	fit_error;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher victim_matcher (player, victim_string, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
		victim_matcher.work_silent ();
	victim_matcher.match_everything ();
	if ((victim = victim_matcher.noisy_match_result ()) == NOTHING)
		return;

	if (new_location_string == NULL || *new_location_string == '\0')
	{
		new_location=victim;
		victim = player;
	}
	else
	{
		Matcher loc_matcher (player, new_location_string, TYPE_NO_TYPE, get_effective_id ());
		if (gagged_command () == True)
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
			if ((victim!=player) && !Wizard(get_effective_id()) && !Apprentice(get_effective_id()))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
		case TYPE_WEAPON:
		case TYPE_ARMOUR:
		case TYPE_AMMUNITION:
			if (!controls_for_write (new_location)
				|| !(controls_for_write (victim) || db[victim].get_location() == NOTHING || controls_for_read (db [victim].get_location()))

				|| ((Typeof (new_location) == TYPE_THING) && !(controls_for_write (db[new_location].get_location()))))
			{
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
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
			context	*victim_context = new context (victim);
			victim_context->enter_room (new_location);
			delete mud_scheduler.push_express_job (victim_context);
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
	if (!in_command())
		notify_colour (player, player, COLOUR_MESSAGES, "Located.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_key (
const	char	*object,
const	char	*keyname)

{
	dbref	container;
	boolexp	*key;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher container_matcher (player, object, TYPE_THING, get_effective_id ());
	if (gagged_command () == True)
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
		if (keyname == NULL || *keyname == '\0')
			key = TRUE_BOOLEXP;
		else if ((key = parse_boolexp (player, keyname)) == TRUE_BOOLEXP)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "I don't understand that key.");
			return;
		}
		db[container].set_lock_key (key);
		if (!in_command())
		{
			if (keyname == NULL || *keyname == '\0')
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s can no longer be locked.", unparse_object (*this, container));
			else
				notify_censor_colour(player, player, COLOUR_MESSAGES, "%s can now be locked.", unparse_object (*this, container));
		}
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_ammo_type (
const	char	*weapon_name,
const	char	*parent_name)
{
	dbref	thing;
	dbref	parent;
	dbref	test_parent;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((thing = match_controlled (*this, weapon_name)) == NOTHING)
		return;

	if (Typeof(thing) == TYPE_AMMUNITION)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The Ammo Type must be of type Ammunition.");
		return;
	}

	/* If the second arg is blank, unparent */
	if (!(parent_name && *parent_name))
	{
		db [thing].set_ammo_parent (NOTHING);
		if (!in_command ())
			notify_colour (player, player, COLOUR_MESSAGES, "Unlinked.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		return;
	}

	Matcher	parent_matcher (player, parent_name, TYPE_NO_TYPE, get_effective_id ());
	parent_matcher.match_everything ();
	if ((parent = parent_matcher.noisy_match_result ()) == NOTHING)
		return;

	/* Finally, we'd better check for whether we're about to create a loop of parents */
	/* (this is one that *doesn't* happen in Real Life (TM)) */
	for (test_parent = parent; test_parent != NOTHING; test_parent = db [test_parent].get_parent ())
		if (thing == test_parent)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES,  "Can't parent an object to something with itself as an ancestor.");
			return;
		}

	/* Looks like it's all OK */
	db [thing].set_ammo_parent (parent);
	if (!in_command ())
		notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "Ammo Type set.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_parent (
const	char	*name,
const	char	*parent_name)

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
	if (!(parent_name && *parent_name))
	{
		db [thing].set_parent (NOTHING);
		if (!in_command ())
			notify_colour (player, player, COLOUR_MESSAGES, "Unlinked.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		return;
	}

	Matcher	parent_matcher (player, parent_name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command () == True)
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
	if (!in_command ())
		notify_colour (player, player, COLOUR_MESSAGES, "Linked.");
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void context::do_credit(const char *arg1, const char *arg2)
{
	dbref victim;
	int amount;
	char *where;
	char scanned_name[BUFFER_LEN];
	const char *currency_name=CURRENCY_NAME;

	if(blank(arg1) || blank(arg2))
	{
		notify(player, "Usage:  @credit <player> = <currency type>;<amount to give>");
		RETURN_FAIL;
	}

	if(in_command() && !Wizard(get_current_command()))
	{
		notify(player, permission_denied);
		RETURN_FAIL;
	}

        strcpy(scanned_name, arg2);
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

	if (strcasecmp(scanned_name, currency_name) != 0)
	{
		notify(player, "That is not a valid currency to credit.");
		RETURN_FAIL;
	}

	if(amount<0)
	{
		notify(player, "You can't give negative money - use @debit instead");
		RETURN_FAIL;
	}

	if(!Wizard(get_effective_id()) && !Apprentice(get_effective_id()) && amount>db[get_effective_id()].get_money())
	{
		notify(player, "You don't have that much to give away.");
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

	if(!in_command())
	{
		notify(player, "You give %d %s to %s.", amount, currency_name, db[victim].get_name());
		notify(victim, "%s gives you %d %s.", db[player].get_name(), amount, currency_name);
	}
	Trace( "CREDIT: %s(%d) gives %d to %s(%d)\n", db[player].get_name(), player, amount, db[victim].get_name(), victim);

	RETURN_SUCC;
}

void context::do_debit(const char *arg1, const char *arg2)
{
	dbref victim;
	int amount;
	char *where;
	char scanned_name[BUFFER_LEN];
	const char *currency_name=CURRENCY_NAME;

	if(blank(arg1) || blank(arg2))
	{
		notify(player, "Usage:  @credit <player> = <currency type>;<amount to give>");
		RETURN_FAIL;
	}

	if(!Apprentice(player) && !Wizard(get_effective_id()))
	{
		notify(player, permission_denied);
                RETURN_FAIL;
	}

	if(in_command() && !Wizard(get_current_command()))
	{
		notify(player, permission_denied);
		RETURN_FAIL;
	}

        strcpy(scanned_name, arg2);
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

	if (strcasecmp(scanned_name, currency_name) != 0)
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

	if(!in_command())
	{
		notify(player, "You take %d %s from %s.", amount, currency_name, db[victim].get_name());
		notify(victim, "%s takes %d %s from you.", db[player].get_name(), amount, currency_name);
	}
	Trace( "CREDIT: %s(%d) gives -%d to %s(%d)\n", db[player].get_name(), player, amount, db[victim].get_name(), victim);

	RETURN_SUCC;
}

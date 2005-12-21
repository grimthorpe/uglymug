/* static char SCCSid[] = "@(#)create.c	1.26\t12/2/94"; */
#include "copyright.h"

/** \file
 * Commands that create new objects in the database.
 */

#include <math.h>
#include <time.h>

#include "db.h"
#include "objects.h"
#include "config.h"
#include "interface.h"
#include "externs.h"
#include "command.h"
#include "match.h"
#include "context.h"
#include "colour.h"
#include "log.h"

/** Macro to choose between using the effective id and using the build_id */
#define ID (player!=get_effective_id())?(get_effective_id()):(db[player].get_build_id())


/** The only scratch_return_string we use is for #id, so it can be short */
static	char	scratch_return_string [11];


/**
 * Given a context and a source for the link, find a location for the destination
 *	link name and check that it's possible to link to it.
 */

extern dbref
parse_linkable_room (
context			&c,
dbref			link_src,
const	String&	link_dest_name)

{
	dbref	link_dest;

	/* Check for HOME first */
	if (!string_compare(link_dest_name, "home"))
		return HOME;

	/* The full monty */
	Matcher link_matcher (c.get_player (), link_dest_name, TYPE_NO_TYPE, c.get_effective_id ());
	if (c.gagged_command ())
		link_matcher.work_silent ();
	link_matcher.match_everything ();
	if ((link_dest = link_matcher.noisy_match_result()) == NOTHING)
		return (NOTHING);

	/* Well, we got something... is it linkable? */
	if ((link_dest >= 0) && (link_dest < db.get_top ()))
	{
		switch (Typeof (link_dest))
		{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if (c.controls_for_write (link_dest))
				return (link_dest);
			else
			{
				notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "You don't control the Player you're trying to link to.");
				return (NOTHING);
			}
		case TYPE_THING:
			if (!Container (link_dest))
			{
				notify_colour (c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "You can't link objects to a Thing - it has to be a container.");
				return (NOTHING);
			}
			/* FALLTHROUGH */
		case TYPE_ROOM:
			if (can_link_to (c, link_dest) || (Typeof (link_src)==TYPE_PLAYER && Abode (link_dest))
					|| (Typeof (link_src)==TYPE_EXIT && Link (link_dest)))
				return link_dest;

			/* In the database, right type, but no link control */
			notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "You can't link it to that!");
			return NOTHING;

		default:
			notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, "You can't link it to that!");
			return NOTHING;
		}
	}

	/* It's out of the database */
	notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That doesn't exist!");
	return NOTHING;
}


/**
 * Check that this context is able to link between room1 and room2.  Rules:
 * - The two locations must share a common parent;
 * - The linker must control_for_write that common parent and everything below it.
 *
 * The intention is to allow players to link freely within their own areas, but not between
 * areas.
 */

static int
check_hierarchy (
context	&c,
dbref	room1,
dbref	room2)

{
	dbref	room1_parents [100];
	dbref	room2_parents [100];
	int	room1_index;
	int	room2_index;
	int	common_parent_offset;
	int	i;

	/* Wizards can do anything */
	if (Wizard (c.get_effective_id ()))
		return (1);

	/* Can always link to HOME */
	if (room2 == HOME)
		return (1);

	/* Chase hierarchies */
	room1_parents [0] = room1;
	for (room1_index = 1; (room1_index < 100) && (room1_parents [room1_index - 1] != NOTHING); room1_index++)
		room1_parents [room1_index] = db [room1_parents [room1_index - 1]].get_location ();
	room1_index--;
	room2_parents [0] = room2;
	for (room2_index = 1; (room2_index < 100) && (room2_parents [room2_index - 1] != NOTHING); room2_index++)
		room2_parents [room2_index] = db [room2_parents [room2_index - 1]].get_location ();
	room2_index--;

	/* If the rooms don't share a common area, scream */
	if (room1_parents [room1_index] != room2_parents [room2_index])
	{
		notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Permission denied: The two locations must have a common parent area.");
		return (0);
	}

	/* Find the common root */
	for (common_parent_offset = 0;
		(common_parent_offset <= MIN (room1_index, room2_index))
			&& (room1_parents [room1_index - common_parent_offset] == room2_parents [room2_index - common_parent_offset]);
		common_parent_offset++)
		;
	common_parent_offset--;

	/* From now on, you gotta control_for_write() everything down both sides. */
	for (i = 0; i <= MAX (room1_index, room2_index) - common_parent_offset; i++)
	{
		if ((i <= room1_index - common_parent_offset) && (!c.controls_for_write (room1_parents [i])))
		{
			if(room1_parents [i] == NOTHING)
				notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, "Permission denied: Not in an area (see 'help areas')");
			else
				notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Permission denied: You don't control %s. (see 'help areas')", unparse_object (c, room1_parents [i]));
			return (0);
		}
		if ((i < room2_index - common_parent_offset) && (!c.controls_for_write (room2_parents [i])))
		{
			if(room2_parents[i] == NOTHING)
				notify_colour(c.get_player(), c.get_player(), COLOUR_ERROR_MESSAGES, "Permission denied: Not in an area (see 'help areas')");
			else
				notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "Permission denied: You don't control %s. (see 'help areas')", unparse_object (c, room2_parents [i]));
			return (0);
		}
	}

	/* Well, it worked */
	return (1);
}


/**
 * Create an exit in the location of player, called direction.
 *	If linkto is non-NULL, try to parse it into a room to link to.
 */
void
context::do_at_open (
const	String& direction,
const	String& linkto)

{
	dbref loc;
	dbref exit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#ifdef RESTRICTED_BUILDING
	if(!Builder(ID)) {
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */

	if ((Typeof (db[player].get_location ()) != TYPE_ROOM) && (!Container (db[player].get_location ())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,"You cannot open an exit from here.");
		return;
	}

	if((loc = getloc(player)) == NOTHING)
		return;
	if (!direction)
	{
		notify_colour(player, player, COLOUR_MESSAGES,  "Open an exit named what?");
		return;
	}
	if(!ok_name(direction))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's not a valid name for an exit!");
		return;
	}
	if(!controls_for_write (loc))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}
	if(!payfor(ID, EXIT_COST))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Sorry, you don't have enough building points to open an exit.");
		return;
	}
	else
	{
		/* create the exit */
		exit = db.new_object(*new (Exit));

		/* initialize everything */
		db[exit].set_name (direction);
		db[exit].set_owner (ID);
		Settypeof(exit, TYPE_EXIT);
		db[exit].set_location (loc);
		Created (exit);

		/* link it in */
		PUSH (exit, loc, exits);

		/* and we're done */
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES,"Exit #%d opened.", exit);

		return_status = COMMAND_SUCC;
		sprintf (scratch_return_string, "#%d", (int)exit);
		set_return_string (scratch_return_string);

		/* check second arg to see if we should do a link */
		if(linkto && *linkto.c_str() != '\0')
		{
			if (!in_command())
				notify_colour(player, player, COLOUR_MESSAGES,  "Trying to link...");

			if((loc = parse_linkable_room(*this, exit, linkto)) != NOTHING)
			{
				if (!Link (loc) && !check_hierarchy (*this, db [exit].get_location (), loc))
				{
					/* Error message already printed */
				}
				else if(!payfor(get_effective_id (), LINK_COST))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have enough building points to link.");
					return_status = COMMAND_FAIL;
					set_return_string (error_return_string);
				}
				else
				{
					/* it's ok, link it */
					db[exit].set_destination (loc);
					if (loc != HOME)
						Accessed (loc);
					if (!in_command())
						notify_colour(player, player, COLOUR_MESSAGES, "Linked.");

					/* already asserted success */
				}
			}
		}
	}
}


/**
 * Link a player or thing to a home; link a room to its dropto; link
 *	an exit to its destination.
 */

void
context::do_at_link (
const	String& name,
const	String& room_name)

{
	dbref loc;
	dbref thing;
	dbref room;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if((loc = getloc(player)) == NOTHING)
		return;

	Matcher thing_matcher (player, name, TYPE_EXIT, get_effective_id ());
	if (gagged_command ())
		thing_matcher.work_silent ();
	thing_matcher.match_everything ();
	if((thing = thing_matcher.noisy_match_result()) != NOTHING)
	{
		if((room = parse_linkable_room (*this, thing, room_name)) == NOTHING)
			return;

		/* we're ok, check the usual stuff */
		if (thing == room)
		{
			notify_colour (player, player, COLOUR_MESSAGES, "There seems little point in linking that to itself.");
			return;
		}
		switch(Typeof(thing))
		{
			case TYPE_EXIT:
				if(db[thing].get_destination () != NOTHING)
				{
					if(controls_for_read (thing))
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That exit is already linked.");
					else
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}

				if(room != HOME)
				{
					if ((Typeof (room) != TYPE_ROOM) && (Typeof (room) != TYPE_THING))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can only link an exit to a Room or a Thing.");
						return;
					}

					if (!Link (room) && !can_link_to (*this, room))
					{
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't link to that location.");
						return;
					}

					if (!Link (room) && !check_hierarchy (*this, loc, room))
					{
						/* Error message already printed */
						return;
					}
				}
#ifdef RESTRICTED_BUILDING
				if(!Builder(ID))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only authorized builders may seize exits.");
					return;
				}
#endif /* RESTRICTED_BUILDING */

				/* handle costs */
				if(db[thing].get_owner () == get_effective_id ())
				{
					if(!payfor(get_effective_id (), LINK_COST))
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "It costs a building point to link this exit.");
						return;
					}
				}
				else
				{
					if(!payfor(get_effective_id (), LINK_COST + EXIT_COST))
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "It costs two building points to link this exit.");
						return;
					}
					else
						/* pay the owner for his loss */
						db[db[thing].get_owner ()].add_pennies (EXIT_COST);
				}

				/* link has been validated and paid for; do it */
				db[thing].set_owner (get_effective_id ());
				db[thing].set_destination (room);
				Modified (thing);
				if (room != HOME)
					Accessed (room);
				return_status = COMMAND_SUCC;

				/* notify the player */
				if (!in_command())
					notify_colour(player, player, COLOUR_MESSAGES, "Linked.");

				break;
			case TYPE_PLAYER:
			case TYPE_PUPPET:
				if (room != HOME)
					if (Typeof(room)!=TYPE_ROOM)
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only set a player's home to be a room.");
						return;
					}
			case TYPE_THING:
				if(!controls_for_write(thing))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
				if(room == HOME)
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Can't set home to home.");
					return;
				}
				if(!(controls_for_write (room) || Abode (room)))
					/*|| ((Typeof (room) != TYPE_ROOM) && (Typeof (thing) == TYPE_PLAYER))))*/
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't link to #%d!", room);
					return;
				}
				/* do the link */
				db[thing].set_destination (room); /* home */
				Modified (thing);
				Accessed (room);
				if (!in_command())
					notify_colour(player, player, COLOUR_MESSAGES, "Home set.");

				break;
			case TYPE_ROOM:
				if(!controls_for_write (thing))
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
					return;
				}
				if(room != HOME)
					if (Typeof (room) == TYPE_PLAYER)
					{
						notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Can't set a dropto to that.");
						return;
					}
				/* do the link, in location */
				db[thing].set_destination (room); /* dropto */
				Modified (thing);
				if (room != HOME)
					Accessed (room);
				if (!in_command())
					notify_colour(player, player, COLOUR_MESSAGES, "Dropto set.");

				break;
			case TYPE_VARIABLE:
			case TYPE_COMMAND:
			case TYPE_FUSE:
			case TYPE_ALARM:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't link that to anything.");
				return;
				break;
			default:
				notify(player, "Internal error: weird object type.");
				log_bug("wierd object in @linkl: Typeof(#%d) = 0x%x", thing, Typeof(thing));
				return;
		}
		return_status = COMMAND_SUCC;
		set_return_string (unparse_for_return (*this, room));
	}
}


/**
 * Create a new room.  Note that it is not, at this point, part of the area hierarchy.
 */

void
context::do_at_dig (
const	String& name,
const	String& desc)

{
	dbref room;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#ifdef RESTRICTED_BUILDING
	if(!Builder(ID))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */
	if(!name)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Dig what?");
		return;
	}
	if(!ok_name(name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a room!");
		return;
	}
	if(!payfor(ID, ROOM_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points to dig a room.");
		return;
	}
	room = db.new_object(*new (Room));

	/* Initialize everything */
	db[room].set_name (name);
	db[room].set_description (desc);
	db[room].set_owner (ID);
	Settypeof(room, TYPE_ROOM);
	db[room].set_location (NOTHING);
	db[room].set_gravity_factor (STANDARD_ROOM_GRAVITY);
	db[room].set_mass (STANDARD_ROOM_MASS);
	db[room].set_volume (STANDARD_ROOM_VOLUME);
	db[room].set_mass_limit (STANDARD_ROOM_MASS_LIMIT);
	db[room].set_volume_limit (STANDARD_ROOM_VOLUME_LIMIT);
	db[room].set_destination (NOTHING);
	Created (room);

	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "%s created with room number #%d", name.c_str(), room);
	}
	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)room);
	set_return_string (scratch_return_string);
}


/**
 * Create a new Thing with the given name and description, and add it to the player's
 * inventory.  All other properties are defaulted.
 */

void
context::do_at_create (
const	String& name,
const	String& desc)

{
	dbref			loc;
	dbref			thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#ifdef RESTRICTED_BUILDING
	if(!Builder(ID))
		notify_colour(player, player, COLOUR_MESSAGES,  "That command is restricted to authorized builders.");

	else
#endif /* RESTRICTED_BUILDING */
	if(!name || *name.c_str() == '\0')
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create what?");
	else if(!ok_name(name))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a thing!");
	else if ((db[player].get_inherited_volume_limit () != HUGE_VAL)
	&& ((find_volume_of_contents(player) + STANDARD_THING_VOLUME) > db[player].get_inherited_volume_limit ()))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't enough room to create that.");
	else if ((db[player].get_inherited_mass_limit () != HUGE_VAL)
	&& (find_mass_of_contents_except (player, NOTHING) + STANDARD_THING_MASS) > (db[player].get_inherited_mass_limit ()))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You or your location wouldn't be able to take the extra weight.");
	else if(!payfor(ID, THING_COST))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points.");
	else
	{
		/* create the object */
		thing = db.new_object(*new (Thing));

		/* initialize everything */
		db[thing].set_name (name);
		db[thing].set_description (desc);
		db[thing].set_location (player);
		db[thing].set_destination (NOTHING);
		db[thing].set_owner (ID);
		Settypeof(thing, TYPE_THING);
		db[thing].set_contents_string (NULLSTRING);
		db[thing].set_gravity_factor (STANDARD_THING_GRAVITY);
		db[thing].set_mass (0);
		db[thing].set_volume (0);
		db[thing].set_mass_limit (STANDARD_THING_MASS_LIMIT);
		db[thing].set_volume_limit (STANDARD_THING_VOLUME_LIMIT);
		db[thing].set_lock_key (TRUE_BOOLEXP);
		Created (thing);

		/* home is here (if we can link to it) or player's home */
		if((loc = db[player].get_location ()) != NOTHING && can_link_to (*this, loc))
			db[thing].set_destination (loc);
		else
			db[thing].set_destination (db[ID].get_destination ());

		/* link it in */
		PUSH (thing, player, contents);

		/* and we're done */
		set_return_string (unparse_for_return (*this, thing));
		if (!in_command())
		{
			notify_colour(player, player, COLOUR_MESSAGES, "Object %s created with id #%d", getname (thing), thing);
		}
		return_status = COMMAND_SUCC;
	}
}


/**
 * Create a command attached to the context's player.
 *	Its name is in name; a command list is in commands.
 */

void
context::do_at_command (
const	String& name,
const	String& commands)

{
	dbref			thing;
	dbref			old_owner;
	dbref			new_owner;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#if defined(RESTRICTED_BUILDING) && defined (COMMAND_NEEDS_BUILDER)
	if(!Builder(ID))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */
	if(!name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create a command called what?");
		return;
	}
	if(!ok_name(name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a command!");
		return;
	}
	if (in_command())
	{
		if(!Wizard(old_owner = db[get_current_command()].get_owner()))
			new_owner = old_owner;
		else
			new_owner = player;
	}
	else
	{
		if (Wizard(get_effective_id()) && !Wizard(player))
			new_owner = player;
		else
			new_owner = ID;
	}
	if(!payfor(new_owner, COMMAND_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, not enough building points available.");
		return;
	}
	/* create the object */
	thing = db.new_object(*new (Command));

	/* initialize everything */
	db[thing].set_name (name);
	db[thing].set_description (commands);
	db[thing].set_location (player);
	db[thing].set_destination (NOTHING);
	db[thing].set_owner(new_owner);
	db[thing].set_flag(FLAG_OPAQUE);
	Settypeof(thing, TYPE_COMMAND);
	Created(thing);

	/* link it in */
	PUSH (thing, player, commands);

	/* and we're done */
	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Command %s created with id #%d", getname (thing), thing);
	}

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}


/**
 * Create an Array called name (the second parameter is not used). The array is attached
 * to the context's player, and contains no elements.
 */

void
context::do_at_array (
const	String& array_name,
const	String& )
{
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#if defined(RESTRICTED_BUILDING) && defined (COMMAND_NEEDS_BUILDER)
	if(!Builder(ID))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */
	if(!array_name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create an array called what?");
		return;
	}
	if(!ok_name(array_name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "That's a silly name for a array!");
		return;
	}
	if(!payfor(ID, ARRAY_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points.");
		return;
	}

	/* create the object */
	thing = db.new_object(*new (Array));

	/* initialize everything */
	db[thing].set_name (array_name);
	db[thing].set_location (player);
	db[thing].set_owner (ID);
	Settypeof(thing, TYPE_ARRAY);
	Created(thing);

	/* link it in */
	PUSH (thing, player, info_items);

	/* and we're done */
	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Array %s created with id #%d", getname(thing), thing);
	}

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}


/**
 * Create a new empty Dictionary called name (the second parameter is not used) and attaches
 *	it to the context's player.
 */

void
context::do_at_dictionary (
const	String& dictionary_name,
const	String& )
{
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#if defined(RESTRICTED_BUILDING) && defined (COMMAND_NEEDS_BUILDER)
	if(!Builder(ID))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */
	if(!dictionary_name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create a dictionary called what?");
		return;
	}
	if(!ok_name(dictionary_name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a dictionary!");
		return;
	}
	if(!payfor(ID, DICTIONARY_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points.");
		return;
	}

	/* create the object */
	thing = db.new_object(*new (Dictionary));

	/* initialize everything */
	db[thing].set_name (dictionary_name);
	db[thing].set_location (player);
	db[thing].set_owner (ID);
	Settypeof(thing, TYPE_DICTIONARY);
	Created(thing);

	/* link it in */
	PUSH (thing, player, info_items);

	/* and we're done */
	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Dictionary %s created with id #%d", getname(thing), thing);
	}

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}

/**
 * Create a new Property called name, with value as its description.  Attach it to the context's
 *	player.
 */

void
context::do_at_property (
const	String& property_name,
const	String& value)
{
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#if defined(RESTRICTED_BUILDING) && defined (COMMAND_NEEDS_BUILDER)
	if(!Builder(ID))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */
	if(!property_name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create a property called what?");
		return;
	}
	if(!ok_name(property_name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a property!");
		return;
	}
	if(!payfor(ID, PROPERTY_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points.");
		return;
	}

	/* create the object */
	thing = db.new_object(*new (Property));

	/* initialize everything */
	db[thing].set_name (property_name);
	db[thing].set_description (value);
	db[thing].set_location (player);
	db[thing].set_owner (ID);
	Settypeof(thing, TYPE_PROPERTY);
	Created(thing);

	/* link it in */
	PUSH (thing, player, info_items);

	/* and we're done */
	if (!in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Property %s created with id #%d", getname(thing), thing);
	}

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}


/*
 * Create a new Variable called name, with value as its description.
 *	NOTE: Variables are deprecated - use Properties instead.
 *	No colour strings have been added as @var is obsolete.
 */

void
context::do_at_variable (
const	String& variable_name,
const	String& value)
{
notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Warning - @variable is a deprecated command. Please use @property instead.");
	/* if we are in a command, dump command information */
	if (in_command()) {
		log_bug("player %s(#%d) used @variable in command %s(#%d)",
				getname(player), player,
				getname(get_current_command()), get_current_command());
	}
	else {
		log_bug("player %s(#%d) used @variable on the command line",
				getname(player), player);
	}
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

#if defined(RESTRICTED_BUILDING) && defined (COMMAND_NEEDS_BUILDER)
	if(!Builder(ID))
	{
		notify(player, "That command is restricted to authorized builders.");
		return;
	}
#endif /* RESTRICTED_BUILDING */
	if(!variable_name)
	{
		notify(player, "Create a variable called what?");
		return;
	}
	if(!ok_name(variable_name))
	{
		notify(player, "That's a silly name for a variable!");
		return;
	}
	if(!payfor(ID, VARIABLE_COST))
	{
		notify(player, "Sorry, you don't have enough building points.");
		return;
	}

	/* create the object */
	thing = db.new_object(*new (Variable));

	/* initialize everything */
	db[thing].set_name (variable_name);
	db[thing].set_description (value);
	db[thing].set_location (player);
	db[thing].set_owner (ID);
	Settypeof(thing, TYPE_VARIABLE);
	Created(thing);

	/* link it in */
	PUSH (thing, player, info_items);

	/* and we're done */
	if (!in_command())
	{
		notify(player, "Variable %s created with id #%d", getname(thing), thing);
	}

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}


/**
 * Try to turn a command name into a command or HOME,
 *	ensuring that we can link to it. Used by do_at_csuccess and do_at_cfailure.
 */

dbref
parse_linkable_command (
context		&c,
const	String& command_name)

{
	dbref	command;

	if (!string_compare (command_name, "home"))
		return (HOME);
	Matcher matcher (c.get_player (), command_name, TYPE_COMMAND, c.get_effective_id ());
	if (c.gagged_command ())
		matcher.work_silent ();
	matcher.match_absolute ();
	matcher.match_command ();
	if ((command = matcher.noisy_match_result ()) == NOTHING)
		return (NOTHING);

	/* check command */
	if (command < 0 || command >= db.get_top () || Typeof(command) != TYPE_COMMAND)
	{
		notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "That's not a command!");
		return NOTHING;
	}
	if(!c.controls_for_read (command))
	{
		notify_colour(c.get_player (), c.get_player(), COLOUR_ERROR_MESSAGES, "You can't link to that.");
		return NOTHING;
	}
	else
		return command;
}


/*
 * Set the success pointer for the command 'name' to be the start of 'command_name'.
 */

void
context::do_at_csuccess (
const	String& name,
const	String& command_name)

{
	dbref thing;
	dbref destination;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!command_name)
		destination = NOTHING;
	else
	{
		if ((destination = parse_linkable_command (*this, command_name)) == NOTHING)
			return;
	}

	Matcher thing_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		thing_matcher.work_silent ();
	thing_matcher.match_command ();
	thing_matcher.match_fuse_or_alarm ();
	thing_matcher.match_absolute ();

	if((thing = thing_matcher.noisy_match_result()) != NOTHING)
	{
		switch (Typeof (thing))
		{
			case TYPE_COMMAND:
				break;
			case TYPE_ALARM:
			case TYPE_FUSE:
				if (destination == HOME)
				{
					notify_colour (player, player, COLOUR_MESSAGES, "Only commands can have csuccess set to HOME.");
					return;
				}
				break;
			default:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You can't link a success command to that.\n");
				return;
		}
		
		if (!controls_for_write (thing))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "Permission denied (source).");
			return;
		}
		db[thing].set_csucc (destination);
		Modified (thing);
		if (destination != HOME && destination != NOTHING)
			Accessed (destination);
		if (!in_command())
		{
			if (destination == NOTHING)
				notify_colour(player, player, COLOUR_MESSAGES, "Unlinked.");
			else
				notify_colour(player, player, COLOUR_MESSAGES, "Linked.");
		}
		return_status = COMMAND_SUCC;
		set_return_string (unparse_for_return (*this, destination));
	}
}


/*
 * Set the failure pointer for the command 'name' to be the start of 'command_name'.
 */

void
context::do_at_cfailure (
const	String& name,
const	String& command_name)

{
	dbref thing;
	dbref destination;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!command_name)
		destination = NOTHING;
	else
	{
		if ((destination = parse_linkable_command (*this, command_name)) == NOTHING)
			return;
	}

	Matcher thing_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	if (gagged_command ())
		thing_matcher.work_silent ();
	thing_matcher.match_command ();
	thing_matcher.match_fuse_or_alarm ();
	thing_matcher.match_absolute();

	if((thing = thing_matcher.noisy_match_result()) != NOTHING)
	{
		switch (Typeof (thing))
		{
			case TYPE_COMMAND:
				break;
			case TYPE_FUSE:
				if (destination == HOME)
				{
					notify_colour (player, player, COLOUR_MESSAGES, "Only commands can have cfailure set to HOME.");
					return;
				}
				break;
			default:
				notify_colour (player, player, COLOUR_ERROR_MESSAGES,  "You can't link a fail command to that.\n");
				return;
		}
		if (!controls_for_write (thing))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Permission denied (source).");
			return;
		}
		db[thing].set_cfail (destination);
		Modified (thing);
		if (destination != HOME && destination != NOTHING)
			Accessed (destination);
		if (!in_command())
		{
			if (destination == NOTHING)
				notify_colour(player, player, COLOUR_MESSAGES, "Unlinked.");
			else
				notify_colour(player, player, COLOUR_MESSAGES, "Linked.");
		}
		return_status = COMMAND_SUCC;
		set_return_string (unparse_for_return (*this, destination));
	}
}


/**
 * Create a new Fuse called 'name' that fires 'command_name' and put it on the player. Note that
 *	this is a different convention to almost every other create command; the others set
 *	the description as the second parameter.
 */

void
context::do_at_fuse (const String& fuse_name, const String& command_name)
{
	dbref	thing;
	dbref destination;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!fuse_name)
	{
		notify_colour (player,player, COLOUR_ERROR_MESSAGES, "Fuses? What do you want to blow up, the bank? (Please specify a valid fuse name.)");
		return;
	}
	if(!ok_name(fuse_name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a fuse!");
		return;
	}
	if(!payfor(ID, FUSE_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points.");
		return;
	}
	/* create the object */
	thing = db.new_object(*new (Fuse));

	/* initialize everything */
	db[thing].set_name (fuse_name);
	db[thing].set_description (NULLSTRING);
	db[thing].set_location (player);
	db[thing].set_destination (NOTHING);
	db[thing].set_owner (ID);
	Settypeof(thing, TYPE_FUSE);
	Created(thing);

	/* link it in */
	PUSH (thing, player, fuses);

	/* and we're done */
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES,  "Fuse %s created with id #%d", getname (thing), thing);

	/* check to see if we have to link into a command */
	if (command_name)
	{
		if ((destination = parse_linkable_command (*this, command_name)) == NOTHING)
			return;

		db[thing].set_exits (destination);
		Modified (thing);
		if (destination != HOME)
			Accessed (destination);
		if (!in_command())
			notify_colour (player,player, COLOUR_MESSAGES, "Linked.");
	}

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}


/**
 * Create a new Alarm on the player, with a time string of time_of_execution.
 */

void
context::do_at_alarm (const String& alarm_name, const String& time_of_execution)
{
	dbref	thing;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!Wizard (get_effective_id ())
		&& !Apprentice(get_effective_id())
		&& !XBuilder(get_effective_id()))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if(!alarm_name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create a alarm called what?");
		return;
	}
	if(!ok_name(alarm_name))
	{
		notify_colour(player,  player, COLOUR_ERROR_MESSAGES,"That's a silly name for a alarm!");
		return;
	}
	if(!payfor(ID, ALARM_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "Sorry, you don't have enough building points.");
		return;
	}

	/* create the object */
	thing = db.new_object(*new (Alarm));

	/* initialize everything */
	db[thing].set_name (alarm_name);
	db[thing].set_description (time_of_execution);
	db[thing].set_location (player);
	db[thing].set_destination (NOTHING);
	db[thing].set_owner (ID);
	Settypeof(thing, TYPE_ALARM);
	Created (thing);

	/* and we're done */
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Alarm %s created with id #%d", getname (thing), thing);
	else
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "WARNING: Alarm (#%d) created in command.",thing);

	/* link it in */
	PUSH (thing, player, fuses);

	/* pend it */
	if (db[thing].get_description ())
		db.pend (thing);

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}


/*
 * Create a new Puppet in the player's location, owned by the player.
 */

void
context::do_at_puppet (
const	String& puppet_name,
const	String& )

{
	dbref	thing,
		target;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	/* We're allowing them, Reaper 08/09/95
	if (!Wizard (get_effective_id ()))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}
	*/
	if(in_command() && (!Wizard(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Can't create puppets within commands");
		return;
	}

	if(!puppet_name)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Create a puppet called what?");
		return;

	}
	if(!ok_name(puppet_name))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's a silly name for a puppet!");
		return;
	}
	if(!payfor(ID, PUPPET_COST))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, you don't have enough building points.");
		return;
	}
	if((target=lookup_player(player, puppet_name))!=NOTHING)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't create a puppet with the same name as an existing player!");
		return;
	}

	/* create the object */
	thing = db.new_object(*new (puppet));

	/* initialize everything */
	db[thing].set_name (puppet_name);
	db[thing].set_description (NULLSTRING);
	db[thing].set_location (db[player].get_location());
	if(controls_for_write(db[db[player].get_location()].get_owner()))
		db[thing].set_destination (db[player].get_location());
	else
		db[thing].set_destination (db[player].get_destination());
	db[thing].set_owner (ID);
	db[thing].set_build_id(thing);

	db[thing].set_mass(STANDARD_PLAYER_MASS);	/* Lee TC	*/
	db[thing].set_volume(STANDARD_PLAYER_VOLUME);
	db[thing].set_mass_limit(STANDARD_PLAYER_MASS_LIMIT);
	db[thing].set_volume_limit(STANDARD_PLAYER_VOLUME_LIMIT);
				
	Settypeof(thing, TYPE_PUPPET);
	Created (thing);

	/* and we're done */
	notify_colour(player, player, COLOUR_MESSAGES, "Puppet %s created with id #%d", getname (thing), thing);

	/* link it in */
	PUSH (thing, db [player].get_location (), contents);

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)thing);
	set_return_string (scratch_return_string);
}
/*
void
context::do_at_clone(const String& newname, const String& original)
{
	dbref newthing = NOTHING;
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(newthing == NOTHING)
		return;

	return_status = COMMAND_SUCC;
	sprintf (scratch_return_string, "#%d", (int)newthing);
	set_return_string (scratch_return_string);
}

dbref
context::really_do_at_clone(const dbref original, const dbref location, bool copyDescription)
{
	// At this point we assume that you have permission to clone the object, and to place it in the new location.
	dbref owner = db[original].get_owner();
	dbref thing = NOTHING;

	switch(Typeof(original))
	{
	case TYPE_PROPERTY:
		if(!payfor(owner, PROPERTY_COST))
		{
			return NOTHING;
		}
		thing = db.new_object(*new (Property));
		break;
	case TYPE_ARRAY:
		if(!payfor(owner, ARRAY_COST))
		{
			return NOTHING;
		}
		thing = db.new_object(*new (Array));
		break;
	case TYPE_DICTIONARY:
		if(!payfor(owner, DICTIONARY_COST))
		{
			return NOTHING;
		}
		thing = db.new_object(*new (Dictionary));
		break;
	default:
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unable to clone an object of that type");
		return NOTHING;
	}

	if(thing == NOTHING)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Something strange happened whilst trying to clone an object");
		return NOTHING;
	}

	// initialize everything
	// Well, not everything.
	// We need a 'clone' method on all classes, otherwise this will get messy.
	db[thing].set_name (db[original].get_name());
	db[thing].set_location (location);
	db[thing].set_owner (owner);
	Settypeof(thing, Typeof(original));
	Created(thing);

	// link it in
	PUSH (thing, location, info_items);

	if(copyDescription)
	{
		switch(Typeof(thing))
		{
			case TYPE_PROPERTY:
				db[thing].set_description(db[original].get_description());
				break;
			case TYPE_ARRAY:
			case TYPE_DICTIONARY:
				break;
		}
	}
	// and we're done
	return thing;
}
*/

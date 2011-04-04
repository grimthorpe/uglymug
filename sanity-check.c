/* static char SCCSid[] = "@(#)sanity-check.c	1.25\t10/19/95"; */
#include "copyright.h"
#include "externs.h"
#include "db.h"
#include "config.h"
#include "context.h"

#include <stdlib.h>


#define	FLAGS		0x1
#define	LOCATION	0x2
#define	DESTINATION	0x4
#define	CONTENTS	0x8
#define	EXITS		0x10
#define	NEXT		0x20
#define	KEY		0x40
#define	OWNER		0x80
#define	PENNIES		0x100
#define	COMMANDS	0x200
#define	INFO_ITEMS	0x400
#define	FUSES		0x800
#define	CONTROLLER	0x1000
#define	TYPE		0x2000
#define	PARENT		0x4000

#define SANITY_CHECK_OUTPUT_FILE "violate_me_baby"

#define	SANITY_MEMBER_HAS_FOUND		0x1
#define	SANITY_IN_A_CONTENTS_LIST	0x2
#define	SANITY_LOCATION_HAS_FOUND	0x4

#define	Member_has_found(ref)		(sanity_array[ref].flags & SANITY_MEMBER_HAS_FOUND)
#define	In_a_contents_list(ref)		(sanity_array[ref].flags & SANITY_IN_A_CONTENTS_LIST)
#define	Location_has_found(ref)		(sanity_array[ref].flags & SANITY_LOCATION_HAS_FOUND)

int	bp_check = 0;
int	chown_check = 0;
int	hack_check = 0;
int	room_check = 0;
int	wizard_check = 0;
int	puppet_check = 0;

int	fix_things = 0;

int	range = 0;
int	fatal = 0;
int	broken = 0;
int	hack = 0;
int	fixed = 0;

extern char	scratch_buffer[];

static	context	unparse_context (UNPARSE_ID, context::DEFAULT_CONTEXT);

FILE	*sanity_file;

struct	sanity_item
{
	int	flags;
	dbref	first_list;
};


struct	sanity_item		*sanity_array;


const char *
sane_unparse_object (dbref thing)

{
	static	char	buf [100];

	if ((thing < 0 || thing >= db.get_top()) && thing != NOTHING && thing != HOME)
	{
		sprintf (buf, "***ILLEGAL (%d)***", (int)thing);
		return (buf);
	}
	else if(Typeof(thing) == TYPE_FREE)
	{
		sprintf (buf, "***FREE (%d)***", (int)thing);
		return (buf);
	}
	else
		return (unparse_object (unparse_context, thing));
}


void
violate (
dbref		i,
const	char	*s,
int		fields)

{
	fprintf(sanity_file, "\nObject %s violates %s.\n", sane_unparse_object (i), s);
	if (fields & LOCATION)
		fprintf(sanity_file, "\tLocation: %s\n", sane_unparse_object(db[i].get_location()));
	if (fields & DESTINATION)
		fprintf(sanity_file, "\tDestination: %s\n", sane_unparse_object(db[i].get_destination()));
	if (fields & CONTENTS)
		fprintf(sanity_file, "\tContents: %s\n", sane_unparse_object (db[i].get_contents()));
	if (fields & EXITS)
		fprintf(sanity_file, "\tExits: %s\n", sane_unparse_object (db[i].get_exits()));
	if (fields & NEXT)
		fprintf(sanity_file, "\tNext: %s\n", sane_unparse_object (db[i].get_next()));
	if (fields & KEY)
	{
		fprintf(sanity_file, "\tKey: %s\n", db[i].get_key()->unparse (unparse_context));
		if (Typeof (i) == TYPE_THING)
			fprintf(sanity_file, "\tLock key: %s\n", db[i].get_lock_key()->unparse (unparse_context));
	}
	if (fields & OWNER)
		fprintf(sanity_file, "\tOwner: %s\n", sane_unparse_object(db[i].get_owner()));
	if (fields & PENNIES)
		fprintf(sanity_file, "\tBuilding points: %d\n", db[i].get_pennies());
	if (fields & COMMANDS)
		fprintf(sanity_file, "\tCommands: %s\n", sane_unparse_object (db[i].get_commands()));
	if (fields & INFO_ITEMS)
		fprintf(sanity_file, "\tVariables: %s\n", sane_unparse_object (db[i].get_variables()));
	if (fields & FUSES)
		fprintf(sanity_file, "\tFuses: %s\n", sane_unparse_object (db[i].get_fuses()));
	if (fields & CONTROLLER)
	{
		if (Typeof (i) == TYPE_PLAYER)
			fprintf(sanity_file, "\tController: %s\n", sane_unparse_object (db[i].get_controller()));
		else
			fprintf(sanity_file, "\tController: %d is not a player, so no controller field!\n", (int)i);
	}
/*	if (fields & FLAGS)
		fprintf(sanity_file, "\tFlags: %x\n", db[i].get_flags());*/
	if (fields & TYPE)
		fprintf(sanity_file, "\tType: %x\n", Typeof (i));
	if (fields & PARENT)
		fprintf(sanity_file, "\tParent: %s(%d)\n", getname (db[i].get_parent ()), (int)(db[i].get_parent ()));
}


int
member_with_sanity (
dbref	thing,
dbref	list,
dbref	owner)

{
	dbref		temp_list;
	dbref		prev_list;
	int		status = 0;
	static	char	buffer [100];

	prev_list = list;
	DOLIST(temp_list, list)
	{
		if (In_a_contents_list (temp_list) && (sanity_array [temp_list].first_list != owner))
		{
			sprintf (buffer, "CONTENTS RULES: IN LISTS FOR #%d AND #%d", (int)(sanity_array [temp_list].first_list), (int)owner);
			violate (prev_list, buffer, OWNER|LOCATION);
			fatal++;
		}
		else
		{
			sanity_array [temp_list].flags |= SANITY_IN_A_CONTENTS_LIST;
			sanity_array [temp_list].first_list = owner;
		}

		/* Check for cycles */
		if (Member_has_found (temp_list))
		{
			violate (prev_list, "LIST CYCLE RULES: NEXT OBJECT IS FURTHER UP THE SAME LIST", OWNER|LOCATION|NEXT);
			fatal++;
			break;
		}

		/* Remember that we've looked at this object */
		sanity_array [temp_list].flags |= SANITY_MEMBER_HAS_FOUND;

		/* Note if it's what we're looking for */
		if(temp_list == thing)
			status = 1;

		/* Update our chaser pointer */
		prev_list = temp_list;
	}

	/* Reset flags and check rooms */
	DOLIST(temp_list, list)
	{
		if (Member_has_found (temp_list))
		{
			sanity_array [temp_list].flags &= ~(SANITY_MEMBER_HAS_FOUND);
			if (db [temp_list].get_location() == NOTHING)
			{
				sprintf (buffer, "POINTER RULES: OBJECT HAS NO LOCATION, BUT IS IN LIST for %d", (int)(sanity_array [temp_list].first_list));
				violate (temp_list, buffer, 0);
				fatal++;
			}
		}
		else
			break;
	}

	return (status);
}


/*
 * location_is_sane: No cycles are allowed in location chains.
 */

void
location_is_sane (
dbref	thing)

{
	dbref		loc;

	for (loc = thing; loc != NOTHING; loc = db [loc].get_location())
	{
		if (loc < 0 || loc >= db.get_top())
		{
			violate (loc, "RANGE RULES: LOCATION IS OUTSIDE DATABASE", 0);
			range++;
			break;
		}

		if (Location_has_found (loc))
		{
			violate (loc, "CYCLE RULES: OBJECT CONTAINS ITSELF", LOCATION|CONTENTS);
			fatal++;
			break;
		}

		/* Remember that we've looked at this object */
		sanity_array [loc].flags |= SANITY_LOCATION_HAS_FOUND;
	}

	/* Reset flags */
	for (loc = thing; loc >= 0 && loc < db.get_top(); loc = db [loc].get_location())
	{
		if (Location_has_found (loc))
			sanity_array [loc].flags &= ~(SANITY_LOCATION_HAS_FOUND);
		else
			break;
	}
}


int
check_ranges (
dbref	i)

{
	int	ok = 1;
	switch (Typeof (i))
	{
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_EXIT:
		case TYPE_FUSE:
		case TYPE_PUPPET:
		case TYPE_PLAYER:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
		case TYPE_PROPERTY:
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
			break;
		default:
			violate (i, "TYPE RULES: NOT A KNOWN TYPE", TYPE);
			ok = 0;
			fatal++;
			return (0);
	}
	if ((db [i].get_location() < 0) || (db [i].get_location() > db.get_top()))
		if (Typeof (i) != TYPE_ROOM)
		{
			violate (i, "RANGE RULES: LOCATION ISN'T IN DATABASE", LOCATION);
			ok = 0;
			range++;
		}
	switch(Typeof(i))
	{
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_PROPERTY:
		case TYPE_VARIABLE:
		case TYPE_COMMAND:
			if (db [i].get_destination() != NOTHING)
			{
				violate (i, "RANGE RULES: DESTINATION ISN'T NOTHING", DESTINATION);
				ok = 0;
				range++;
			}
			break;
		default:
			if ((db [i].get_destination() < 0) || (db [i].get_destination() > db.get_top()))
				switch (Typeof (i))
				{
					case TYPE_FUSE:
					case TYPE_ALARM:
						if (db [i].get_destination() != NOTHING)
						{
							violate (i, "RANGE RULES: DESTINATION ISN'T IN DATABASE", DESTINATION);
							range++;
							ok = 0;
						}
						break;
					case TYPE_EXIT:
					case TYPE_ROOM:
						if ((db [i].get_destination() != NOTHING) && (db [i].get_destination() != HOME))
						{
							violate (i, "RANGE RULES: DESTINATION ISN'T IN DATABASE", DESTINATION);
							range++;
							ok = 0;
						}
						break;
					case TYPE_THING:
					case TYPE_PLAYER:
						violate (i, "RANGE RULES: DESTINATION ISN'T IN DATABASE", DESTINATION);
						range++;
						ok = 0;
						break;
					default:
						violate (i, "RANGE RULES: OBJECT IS WRONG TYPE", TYPE);
						range++;
						ok = 0;
						break;

				}
	}

	if (((db [i].get_contents() < 0) || (db [i].get_contents() > db.get_top())) && (db [i].get_contents() != NOTHING))
		if ((Typeof (i) != TYPE_COMMAND) || (db [i].get_contents() != HOME))
		{
			violate (i, "RANGE RULES: CONTENTS HEAD ISN'T IN DATABASE", CONTENTS);
			range++;
			ok = 0;
		}
	if (((db [i].get_exits() < 0) || (db [i].get_exits() > db.get_top())) && (db [i].get_exits() != NOTHING))
		if ((Typeof (i) != TYPE_COMMAND) || (db [i].get_exits() != HOME))
		{
			violate (i, "RANGE RULES: EXITS HEAD ISN'T IN DATABASE", EXITS);
			range++;
			ok = 0;
		}
	if (((db [i].get_next() < 0) || (db [i].get_next() > db.get_top())) && (db [i].get_next() != NOTHING))
	{
		violate (i, "RANGE RULES: NEXT ISN'T IN DATABASE", NEXT);
		range++;
		ok = 0;
	}
	if ((db [i].get_owner() < 0) || (db [i].get_owner() > db.get_top()))
	{
		if(fix_things)
		{
			db[i].set_owner(i);
			violate (i, "RANGE RULES: OWNER ISN'T IN DATABASE, Repaired, set to player", OWNER);
			fixed++;
		}
		else
		{
			violate (i, "RANGE RULES: OWNER ISN'T IN DATABASE", OWNER);
			range++;
			ok = 0;
		}
	}
	if (((db [i].get_commands() < 0) || (db [i].get_commands() > db.get_top())) && (db [i].get_commands() != NOTHING))
	{
		violate (i, "RANGE RULES: COMMANDS HEAD ISN'T IN DATABASE", COMMANDS);
		range++;
		ok = 0;
	}
	if (((db [i].get_info_items() < 0) || (db [i].get_info_items() > db.get_top())) && (db [i].get_info_items() != NOTHING))
	{
		violate (i, "RANGE RULES: INFO_ITEMS HEAD ISN'T IN DATABASE", INFO_ITEMS);
		range++;
		ok = 0;
	}
	if (((db [i].get_fuses() < 0) || (db [i].get_fuses() > db.get_top())) && (db [i].get_fuses() != NOTHING))
	{
		violate (i, "RANGE RULES: ALARMS/FUSES HEAD ISN'T IN DATABASE", FUSES);
		range++;
		ok = 0;
	}
	if (((Typeof (i) == TYPE_PLAYER) && (db[i].get_controller() < 0)) || (db[i].get_controller() > db.get_top()))
	{
		violate (i, "RANGE RULES: CONTROLLER ISN'T IN DATABASE", CONTROLLER);
		range++;
		ok = 0;
	}

	return (ok);
}


void
check_flags (
dbref	i)
{
	typeof_type	type = Typeof (i);
	int		x;

	for(x=0;flag_list[x].flag;x++)
		if(db[i].get_flag(flag_list[x].flag))
			if (!is_flag_allowed(type, flag_list[x].flag))
			{
				sprintf(scratch_buffer, "Flag Rules: Object has Illegal Flag (%s (%d))", flag_list[x].string, flag_list[x].flag);
				violate (i, scratch_buffer, FLAGS);
				broken++;
			}
        if (Wizard(i) && wizard_check)
	{
		violate(i,"wizard rules: Object has Wizard flag", 0);
		broken++;
	}
	switch (Typeof (i))
	{
		case TYPE_PLAYER:
			if (db[i].get_flag(FLAG_CHOWN_OK))
			{
				if (Wizard (i))
				{
					violate(i,"PLAYER OWNERSHIP RULES: WIZARD CAN HAVE OBJECTS CHOWN'ED TO IT", 0);
					broken++;
				}
				else if (chown_check)
				{
					violate(i,"player ownership rules: Player can have objects chown'ed to it", 0);
					broken++;
				}
			}
			if (db [i].get_flag(FLAG_BUILDER) && Puppet (i) && puppet_check)
			{
				violate (i, "puppet rules: Builder is also a puppet", CONTROLLER);
				broken++;
			}
		default:
			break;
	}
}


void
check_ownership (
dbref	i)

{
		dbref owner = db[i].get_owner();

		if (owner < 0 || owner > db.get_top())
		{
			if (fix_things)
			{
				if (Typeof(i) == TYPE_PLAYER)
				{
					violate (i, "OWNERSHIP RULES: OWNER ISN'T IN DATABASE: Repaired, set to player", LOCATION);
					db[i].set_owner(i);
					fixed++;
				}
				else
				{
					db[i].set_owner(GOD_ID);
					violate (i, "OWNERSHIP RULES: OWNER ISN'T IN DATABASE: Repaired, set to God", LOCATION);
					fixed++;
				}
			}
			else
			{
				violate(i,"OWNERSHIP RULES: OWNER ISN'T IN DATABASE", OWNER);
				range++;
				return;
			}

		}

		if ((Typeof(owner) != TYPE_PLAYER) && (Typeof(owner) != TYPE_PUPPET))
		{
			violate(i,"OWNERSHIP RULES: OWNER ISN'T A PLAYER", OWNER);
			fatal++;
		}
		if (Typeof(i) == TYPE_PLAYER && owner != i)
		{
			if(fix_things)
			{
				db[i].set_owner(i);
				violate(i,"player ownership rules: Player is owned by someone else. Repaired, set to player", OWNER);
				fixed++;
			}
			else
			{
				violate(i,"player ownership rules: Player is owned by someone else", OWNER);
				fatal++;
			}
		}
		if ((Typeof (i) == TYPE_PLAYER) && (db[i].get_controller() != i))
		{
			if (Wizard (i))
			{
				violate (i, "HACKING RULES: WIZARD IS A PUPPET", CONTROLLER);
				hack++;
			}
			if (Typeof (db[i].get_controller()) != TYPE_PLAYER)
			{
				violate (i, "TYPE RULES: CONTROLLER IS NOT A PLAYER", CONTROLLER);
				broken++;
			}
		}
}


void
boolexp::valid_key (
const	dbref	i)
const

{
	if (this == TRUE_BOOLEXP)
		return;
	switch(type)
	{
		case BOOLEXP_AND:
		case BOOLEXP_OR:
			sub2->valid_key (i);
			/* FALLTHROUGH */
		case BOOLEXP_NOT:
			sub2->valid_key (i);
			break;
		case BOOLEXP_CONST:
			switch (Typeof(thing))
			{
				case TYPE_PLAYER:
				case TYPE_PUPPET:
				case TYPE_THING:
				case TYPE_COMMAND:
					break;
				default:
					violate(i,"VALID KEY RULES: KEY LEAVES MUST BE PLAYERS, THINGS OR COMMANDS", KEY);
					fatal++;
					break;
			}
			break;
		case BOOLEXP_FLAG:
			/* Always a valid key for a flag */
			break;
		default:
			violate(i,"BOOLEAN KEY RULES: NODE WASN'T AND, OR, NOT, FLAG or CONST", KEY);
			fatal++;
	}
}


void
check_destination (
dbref	i)

{
	dbref			destination = db[i].get_destination();
	typeof_type	dest_type = TYPE_NO_TYPE;

	if ((destination >= 0) && (destination < db.get_top()))
		dest_type = Typeof (destination);

	switch(Typeof(i))
	{
		case TYPE_EXIT:
			if ((destination != HOME) && (destination != NOTHING))
				switch(dest_type)
				{
					case TYPE_ROOM:
						break;
					case TYPE_THING:
						if (!Container (destination))
						{
							violate(i,"TYPE RULES: DESTINATION THING IS NOT A CONTAINER", DESTINATION);
							fatal++;
						}
						break;
					default:
						violate(i,"TYPE RULES: DESTINATION IS NOT A ROOM OR THING", DESTINATION);
						fatal++;
						break;
				}
			break;
		case TYPE_ROOM:
			if ((destination != HOME) && (destination != NOTHING))
				switch(dest_type)
				{
					case TYPE_ROOM:
					case TYPE_THING:
						break;
					default:
						violate(i,"TYPE RULES: DESTINATION (DROPTO) IS NOT A ROOM", DESTINATION);
						fatal++;
						break;
				}
			if (destination == i)
			{
				violate (i, "CYCLE RULES: DROPS TO ITSELF", DESTINATION);
				broken++;
			}
			break;
		case TYPE_THING:
			if ((dest_type != TYPE_ROOM) && (dest_type != TYPE_THING) && (dest_type != TYPE_PLAYER) && (dest_type != TYPE_PUPPET))
			{
				if(fix_things)
				{
					db[i].set_destination(NOTHING);
					fixed++;
				}
				else
				{
					violate (i, "TYPE RULES: DESTINATION (HOME) IS NOT A PLAYER, PUPPET, ROOM OR CONTAINER", DESTINATION);
					fatal++;
				}
			}
			if (destination == i)
			{
				violate (i, "CYCLE RULES: HOME IS ITSELF", DESTINATION);
				fatal++;
			}
			break;
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if ((dest_type != TYPE_ROOM) && (dest_type != TYPE_THING))
			{
				violate (i, "TYPE RULES: DESTINATION (HOME) IS NOT A ROOM OR A CONTAINER", DESTINATION);
				fatal++;
			}
			if (destination == i)
			{
				violate (i, "CYCLE RULES: HOME IS ITSELF", DESTINATION);
				fatal++;
			}
			break;
		case TYPE_ALARM:
		case TYPE_COMMAND:
		case TYPE_FUSE:
		case TYPE_VARIABLE:
		case TYPE_PROPERTY:
		case TYPE_DICTIONARY:
		case TYPE_ARRAY:
			break;
		default:
			break;
	}
}


void
check_location (
dbref	i)

{
	dbref	loc = db[i].get_location();

	if(loc >= 0)
	{
		if(Typeof(loc) == TYPE_FREE)
		{
			if(fix_things)
			{
				db[i].set_location(LIMBO);
				violate (i, "TYPE RULES: LOCATION WAS OF TYPE FREE: Repaired", LOCATION);
				fixed++;
			}
			else
			{
				violate (i, "TYPE RULES: LOCATION IS OF TYPE FREE", LOCATION);
				broken++;
			}
			return;
		}
	}

	switch(Typeof(i))
	{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if ((Typeof (loc) != TYPE_THING) && (Typeof (loc) != TYPE_ROOM))
			{
				violate (i, "TYPE RULES: LOCATION IS NOT A ROOM OR THING", LOCATION);
				fatal++;
			}
			else if (!member_with_sanity (i, db[loc].get_contents(), loc))
			{
				violate(i,"CONTENTS RULES: PLAYER IS NOT IN LOCATION'S CONTENTS LIST", LOCATION);
				fatal++;
			}
			break;
		case TYPE_THING:
			if ((Typeof (loc) != TYPE_PUPPET) && (Typeof (loc) != TYPE_PLAYER) && (Typeof (loc) != TYPE_THING) && (Typeof (loc) != TYPE_ROOM))
			{
				violate (i, "TYPE RULES: LOCATION IS NOT A PLAYER, ROOM OR THING", LOCATION);
				fatal++;
			}
			else if (!member_with_sanity (i, db[loc].get_contents(), loc))
			{
				violate(i,"CONTENTS RULES: OBJECT IS NOT IN LOCATION'S CONTENTS LIST", LOCATION);
				fatal++;
			}
			break;
		case TYPE_EXIT:
			switch (Typeof (loc))
			{
				case TYPE_PLAYER:
					if (member_with_sanity (i, db[loc].get_contents(), loc))
					{
						violate(i,"CONTENTS RULES: EXIT IS IN PLAYER'S CONTENTS LIST - OLD FORMAT", LOCATION);
						fatal++;
					}
					/* FALLTHROUGH */
				case TYPE_ROOM:
				case TYPE_THING:
					if (!member_with_sanity (i, db[loc].get_exits(), loc))
					{
						violate(i,"CONTENTS RULES: EXIT IS NOT IN LOCATION'S EXIT LIST", LOCATION);
						fatal++;
					}
					break;
				default:
					violate(i,"TYPE RULES: EXIT IS NOT IN A PLAYER, ROOM OR THING", LOCATION);
					fatal++;
			}
			break;
		case TYPE_ROOM:
			location_is_sane (i);
			if (loc != NOTHING)
				if (!member_with_sanity (i, db[loc].get_contents(), loc))
				{
					violate(i,"CONTENTS RULES: ROOM IS NOT IN LOCATIONS CONTENTS LIST", LOCATION);
					fatal++;
				}
			break;
		case TYPE_COMMAND:
			if ((db[loc].get_owner() != db[i].get_owner()) && (Wizard (db [i].get_owner())) && hack_check)
			{
				violate(i,"HACKING RULES: WIZARD COMMAND IS ACCESSIBLE BY A MORTAL", OWNER|LOCATION);
				hack++;
			}
			if (!member_with_sanity (i, db[loc].get_commands(), loc))
			{
				violate(i,"CONTENTS RULES: COMMAND IS NOT IN LOCATION'S COMMAND LIST", LOCATION);
				fatal++;
			}
			break;
		case TYPE_ARRAY:
		case TYPE_PROPERTY:
		case TYPE_DICTIONARY:
		case TYPE_VARIABLE:
			if (!member_with_sanity (i, db[loc].get_info_items(), loc))
			{
				violate(i,"CONTENTS RULES: INFO_ITEM IS NOT IN LOCATION'S INFO_ITEMS LIST", LOCATION);
				fatal++;
			}
			break;
		case TYPE_FUSE:
		case TYPE_ALARM:
			if (!member_with_sanity (i, db[loc].get_fuses(), loc))
			{
				violate(i,"CONTENTS RULES: FUSE/ALARM IS NOT IN LOCATION'S FUSE LIST", LOCATION);
				fatal++;
			}
			break;
		default:
			violate(i, "CONTENTS RULES: OBJECT TYPE IS NOT VALID", TYPE);
			break;
	}
}


void
check_pennies (dbref i)
{
	dbref 	pennies = db[i].get_pennies();

	switch(Typeof(i))
	{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			if ((pennies <0 || pennies > bp_check) && bp_check)
			{
				violate(i, "building points rules: Player is too rich", PENNIES);
				broken++;
			}
			break;
		default:
			break;
	}
}


void
check_next (
dbref	i)

{
	if (db[i].get_next() == NOTHING)
		return;

	switch (Typeof (i))
	{
		case TYPE_ROOM:
			if (db [i].get_location() == NOTHING)
			{
				violate (i, "NEXT RULES: TOP-LEVEL AREA HAS NEXT OBJECT", NEXT);
				fatal++;
			}
			else
			{
				switch(Typeof(db[i].get_next()))
				{
					case TYPE_PLAYER:
					case TYPE_PUPPET:
					case TYPE_THING:
					case TYPE_ROOM:
						break;
					default:
						violate (i, "NEXT RULES: ROOM POINTS TO INVALID OBJECT TYPE", NEXT);
						fatal++;
				}
			}
			break;
		case TYPE_THING:
			switch(Typeof (db[i].get_next()))
			{
				case TYPE_ROOM:
				case TYPE_THING:
				case TYPE_PLAYER:
				case TYPE_PUPPET:
					break;
				default:
					violate (i, "NEXT RULES: THING POINTS TO INVALID OBJECT TYPE", NEXT);
					fatal++;
					break;
			}
			break;
		case TYPE_EXIT:
			if (Typeof (db[i].get_next()) != TYPE_EXIT)
			{
				violate (i, "NEXT RULES: EXIT POINTS TO INVALID OBJECT TYPE", NEXT);
				fatal++;
			}
			break;
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			switch(Typeof (db[i].get_next()))
			{
				case TYPE_ROOM:
				case TYPE_THING:
				case TYPE_PLAYER:
				case TYPE_PUPPET:
					break;
				default:
					violate (i, "NEXT RULES: PLAYER POINTS TO INVALID OBJECT TYPE", NEXT);
					fatal++;
					break;
			}
			break;
		case TYPE_COMMAND:
			if (Typeof (db[i].get_next()) != TYPE_COMMAND)
			{
				violate (i, "NEXT RULES: COMMAND DOESN'T POINT TO COMMAND", NEXT);
				fatal++;
			}
			/* Check csucc/cfail here too */
			if ((db [i].get_contents() != NOTHING) && (db [i].get_contents() != HOME))
			{
				if (Typeof (db [i].get_contents()) != TYPE_COMMAND)
				{
					violate (i, "NEXT RULES: CSUCC (CONTENTS) OF COMMAND DOESN'T POINT TO COMMAND", CONTENTS);
					fatal++;
				}
				if (Wizard (db [db [i].get_contents()].get_owner()) && !Wizard (db [i].get_owner()) && hack_check)
				{
					violate (i, "HACKING RULES: CSUCC (CONTENTS) OF MORTAL COMMAND IS WIZARD COMMAND", OWNER|CONTENTS|LOCATION);
					hack++;
				}
			}
			if ((db [i].get_exits() != NOTHING) && (db [i].get_exits() != HOME))
			{
				if (Typeof (db [i].get_exits()) != TYPE_COMMAND)
				{
					violate (i, "NEXT RULES: CFAIL (EXITS) OF COMMAND DOESN'T POINT TO COMMAND", EXITS);
					fatal++;
				}
				if (Wizard (db [db [i].get_exits()].get_owner()) && !Wizard (db [i].get_owner()) && hack_check)
				{
					violate (i, "HACKING RULES: CFAIL (EXITS) OF MORTAL COMMAND IS WIZARD COMMAND", EXITS|OWNER|LOCATION);
					hack++;
				}
			}
			break;
		case TYPE_ARRAY:
		case TYPE_DICTIONARY:
		case TYPE_PROPERTY:
		case TYPE_VARIABLE:
			switch(Typeof(db[i].get_real_next()))
			{
				case TYPE_ARRAY:
				case TYPE_DICTIONARY:
				case TYPE_PROPERTY:
				case TYPE_VARIABLE:
					break;
				default:
					violate (i, "NEXT RULES: INFO_ITEM DOESN'T POINT TO INFO_ITEM", NEXT);
					fatal++;
					break;
			}
			break;
		case TYPE_FUSE:
		case TYPE_ALARM:
			if ((Typeof (db[i].get_next()) != TYPE_FUSE) && (Typeof (db[i].get_next()) != TYPE_ALARM))
			{
				violate (i, "NEXT RULES: FUSE OR ALARM DOESN'T POINT TO FUSE OR ALARM", NEXT);
				fatal++;
			}
			break;
		case TYPE_FREE:
			if (Typeof (db[i].get_next()) != TYPE_FREE)
			{
				violate (i, "NEXT RULES: FREE OBJECT DOESN'T POINT TO FREE OBJECT", NEXT);
				fatal++;
			}
			break;
		default:
			violate (i, "NEXT RULES: OBJECT HAS UNKNOWN NEXT TYPE", NEXT);
			fatal++;
			break;
	}
}


void
check_parent (
dbref	i)

{
	if (db [i].get_parent () != NOTHING)
	{
		if (Typeof (db [i].get_parent ()) != Typeof (i))
		{
			if (fix_things)
			{
				violate (i, "parent rules: parent type does not match child type (unparented)", PARENT);
				db [i].set_parent (NOTHING);
				fixed++;
			}
			else
			{
				violate (i, "PARENT RULES: PARENT TYPE DOES NOT MATCH CHILD TYPE", PARENT);
				fatal++;
			}
		}
	}
}


void Database::sanity_check ()
{
	dbref	i;

	if((sanity_file = fopen(SANITY_CHECK_OUTPUT_FILE, "w")) == NULL)
	{
		Trace( "Couldn't open Sanity Check file: %s\n", SANITY_CHECK_OUTPUT_FILE);
		exit(1);
	}

	if ((sanity_array = (struct sanity_item *) malloc (db.get_top() * sizeof (struct sanity_item))) == NULL)
	{
		Trace( "Couldn't allocate array for sanity-check array\n");
		exit (1);
	}
	//Little line to let you know it has worked sort of
	fprintf(sanity_file, "Database has been loaded. Running those checks for our sanity.\n");
	
	/* Zero our array */
	for (i = 0; i < db.get_top(); i++)
	{
		sanity_array [i].flags = 0;
	}

	/* Chase each dbref */
	for (i = 0; i < db.get_top(); i++)
	{
		if (Typeof(i) == TYPE_FREE)
			continue;
		if (!check_ranges (i))
			continue;
		check_flags (i);
		check_ownership (i);
		check_location(i);
		check_destination(i);
		check_parent (i);
		check_next(i);
		check_pennies(i);
		db[i].get_key ()->valid_key (i);
		if (Typeof (i) == TYPE_THING)
			db [i].get_lock_key()->valid_key (i);
		if ((Typeof(i) != TYPE_COMMAND) && (Typeof(i) != TYPE_FUSE))
		{
			member_with_sanity (NOTHING, db [i].get_exits(), i);
			member_with_sanity (NOTHING, db [i].get_contents(), i);
		}
		/* Check that we haven't already seen this object */
		member_with_sanity (NOTHING, db [i].get_commands(), i);
		member_with_sanity (NOTHING, db [i].get_variables(), i);
		member_with_sanity (NOTHING, db [i].get_fuses(), i);
	}

	/* Check everything's referenced */
	for (i = 0; i < db.get_top(); i++)
	{
		if (Typeof (i) == TYPE_FREE)
			continue;
		if (!In_a_contents_list (i))
		{
			if (Typeof (i) == TYPE_ROOM)
			{
				if (db [i].get_location() != NOTHING)
					violate (i, "CONTENTS RULES: ROOM NOT IN ANY CONTENTS LIST", LOCATION);
				else if (room_check)
					violate (i, "contents rules: room not in an area", 0);
			}
			else
				violate (i, "CONTENTS RULES: NOT IN ANY CONTENTS LIST AND NOT ON FREE LIST", LOCATION);
		}
	}
	fclose(sanity_file);
	free(sanity_array);
}

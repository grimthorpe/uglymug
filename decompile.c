/* decompile.c was originally going to be soft-coded, but it got a little to difficult to do that way.
 * So I've glued it together in hard code. A lot of it is just a tweaking of existing examine code,
 * but I'm sure we all know it's not quite _that_ easy! ;-)
 *	Colin 'Chisel' Wright - Sep 1995
 */

#include <string.h>
#include <math.h>
#include "db.h"
#include "config.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "command.h"
#include "context.h"


/*
 * This is hopefully going to be a little routine which will take a text string as input, and the output it with 'the necessary
 * protected characters'  
 * This is where you can check for #<id> in a description.
 */


static char *
decompile_string (
const	char	*string)

{
	static	char	return_string [BUFFER_LEN + 1];
	const	char	*src = string;
		char	*dest = return_string;

	while (src && *src && (dest - return_string) < BUFFER_LEN)
	{
		switch (*src)
		{
			case '{':
			case '$':
			case '\\':
			case '\n':
				*dest++ = '\\';
				break;
			case '%':
				*dest++ = '%';
				break;
			/* Default: Do nothing */
		}

		*dest++ = *src++;
	}

	*dest = '\0';
	return return_string;
}

static char*
decompile_string(const String& string)
{
	return decompile_string(string.c_str());
}

static void
decompile_mass_and_volume (dbref object, dbref player)

{
const char* object_name = decompile_string(db[object].get_name());

	if (db[object].get_gravity_factor() != 1)
	{
		notify (player, "@gravityfactor %s = %.9g", object_name, db[object].get_gravity_factor());
	}
	notify (player, "@mass %s = %.9g", object_name, db[object].get_mass ());
	notify (player, "@volume %s = %.9g", object_name, db[object].get_volume ());
 
	if (db[object].get_mass_limit () >= HUGE_VAL)
		notify(player, "@masslimit %s = None", object_name);
 else
	notify(player, "@masslimit %s = %.9g", object_name, db[object].get_mass_limit ());

 if (db[object].get_volume_limit () >= HUGE_VAL)
	notify(player, "@volumelimit %s = None", object_name);
 else
	notify(player, "@volumelimit %s = %.9g", object_name, db[object].get_volume_limit ());
}


static bool
decompile_flags (dbref object, dbref player)
{
 int		i;
 bool	readonly_set = false;


 readonly_set = (db[object].get_flag(FLAG_READONLY));


 for (i = 0 ; flag_list [i].string != NULL; i++)
 {
	{
	  if ( (flag_list[i].flag == FLAG_CONNECTED) ||
	       (flag_list[i].flag == FLAG_REFERENCED) )
		/* Do nothing - just don't output a these flags */ ;
	  else
	  if (db [object].get_flag(flag_list [i].flag))
		notify (player, "@set %s = %s", decompile_string(db[object].get_name()), flag_list [i].string);
	 }
 }
 
 return readonly_set;
}


/* This function decides what should be typed to actually re-create the object */

static void
decompile_how_to_make(dbref object, dbref player)

{
 char create_word[BUFFER_LEN];
 
 switch (Typeof (object))
 {
	case TYPE_ROOM:
		strcpy (create_word, "dig");
		break;
	case TYPE_THING:
		strcpy (create_word, "create");
		break;
	case TYPE_PLAYER:
		notify (player, "@pcreate %s = PASSWORD", decompile_string(db[object].get_name()));
		return;
	case TYPE_EXIT:
		strcpy (create_word, "open");
		break;
	case TYPE_PUPPET:
		strcpy (create_word, "puppet");
		break;
	case TYPE_COMMAND:
		strcpy (create_word, "command");
		break;
	case TYPE_VARIABLE:
		strcpy (create_word, "variable");
		break;
	case TYPE_PROPERTY:
		strcpy (create_word, "property");
		break;
	case TYPE_DICTIONARY:
		strcpy (create_word, "dictionary");
		break;
	case TYPE_ARRAY:
		strcpy (create_word, "array");
		break;
	case TYPE_FUSE:
		strcpy (create_word, "fuse");
		break;
	case TYPE_ALARM:
		strcpy (create_word, "alarm");
		break;
	default:
		notify (player, "I don't know what type that is, so I can't decompile it.");
		return;
 }

 notify (player, "@%s %s", create_word, decompile_string(db[object].get_name()));

}


static void
decompile_alias (
dbref	object,
dbref	player)

{
		int	i;
	for (i = 0; i < MAX_ALIASES; i++)
	{
		if (db[object].get_alias(i))
			notify (player, "@alias %s = %s", decompile_string(db[object].get_name()), db[object].get_alias(i).c_str());
	}
}



void
context::do_decompile (
const	CString& name,
const	CString& args)

{
	dbref	object;			/* We need to be able to refer to the object */
	bool	readonly_flag_set;	/* We need to know if an object is RO - it's a special flag - needs to be final command */
	int	number,			/* The number of elements in an array or dictionary */
		temp;			/* A counter for outputting array or dictionary elements */
	char	decompiled_name[BUFFER_LEN * 2];

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	/* If nothing specified to decompile, give up */
	if (!name)
	{
		notify (player, "You must specify something to decompile.");
		return;
	}

	Matcher controller_matcher (player, name, TYPE_PLAYER, get_effective_id());

	/* Set up the object matcher*/
	Matcher thing_matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
	thing_matcher.match_everything ();

	/* get result */
	if((object = thing_matcher.noisy_match_result()) == NOTHING)
		return;


	if (!Wizard(player))
	{
		notify (player, permission_denied);
		return;
	}


	if (in_command())
	{
		notify (player, "You can't use @decompile in a compound command.");
		return;
	}



	strcpy (decompiled_name, decompile_string(db[object].get_name()));

/* I know that this is a splodge of code at the moment, but I'm hoping to order it more sensibly later */

	notify (player, "[Decompile Output Start]");					/* Let them know where the decompiling starts */
	if ((Wizard(get_effective_id()) && (Typeof(object) == TYPE_PLAYER)))
		notify (player, "@pcreate %s = PASSWORD", decompiled_name);
	else
		decompile_how_to_make (object, player);

/* Depending on what TYPE we have go through the relevant decompile steps */

	switch (Typeof(object))
	{
                case TYPE_ALARM:
		case TYPE_COMMAND:
                case TYPE_EXIT:
		case TYPE_FUSE:
			if (db[object].get_drop_message())
			    /* It makes more sense to set the drop message first - not affect ticks on fuses */
			    notify (player, "@drop %s = %s", decompiled_name, db[object].get_drop_message().c_str());
		case TYPE_PLAYER:
		case TYPE_PROPERTY:
		case TYPE_PUPPET:
		case TYPE_ROOM:
		case TYPE_THING:
		case TYPE_VARIABLE:
			notify (player,
                                "@describe %s = %s",
                                decompiled_name,
                                decompile_string(db[object].get_description())
				);
			break;
		case TYPE_DICTIONARY:
			number= db[object].get_number_of_elements();
			if (number > 0)
			{
				for (temp= 1 ; temp <= number ; temp++)
					if(db[object].get_element(temp))
						notify (player, "@describe %s[%s] = %s", decompiled_name, db[object].get_index(temp).c_str(),	decompile_string(db[object].get_element(temp).c_str()));
					else
						notify (player, "@describe %s[%s]", decompiled_name, db[object].get_index(temp).c_str());
			}
			break;
		case TYPE_ARRAY:
			 number= db[object].get_number_of_elements();
			if (number > 0)
			{
				for (temp= 1 ; temp <= number ; temp++)
					if (db[object].get_element(temp))
						notify (player, "@describe %s[%d] = %s", decompiled_name, temp, decompile_string(db[object].get_element(temp)));
					else
						notify (player, "@describe %s[%d]", decompiled_name, temp);
			}
			break;
		default:
			notify (player, "I don't know what type that object is so I can't decompile it!");
			return;
	}


/* These bits are common to all objects */
	readonly_flag_set = decompile_flags (object, player);			/* Decompile flags set on object */
	notify (player, "@owner %s = %s", decompiled_name, unparse_for_return(*this, db[object].get_owner()));

	switch (Typeof (object))
	{
		case TYPE_PLAYER:
			if (Wizard(get_effective_id()))
			    notify (player, "@score %s = %d", decompiled_name, db[object].get_score());
		case TYPE_PUPPET:
			if (Wizard(get_effective_id()))
			{
				controller_matcher.match_me ();
				controller_matcher.match_neighbor ();
				controller_matcher.match_player ();
				controller_matcher.match_absolute ();
				notify (player, "@controller %s = #%d", decompiled_name, controller_matcher.noisy_match_result());
				notify (player, "give %s = %d", decompiled_name, db[object].get_pennies());
			}
			decompile_alias (object, player);
			notify (player, "@race %s = %s", decompiled_name, decompile_string(db[object].get_race()));
		case TYPE_ROOM:
		case TYPE_THING:
			if (db[object].get_contents_string())
				notify(player, "@cstring %s = %s", decompiled_name, decompile_string(db[object].get_contents_string()));
			decompile_mass_and_volume (object, player);
		case TYPE_EXIT:
		        if (db[object].get_destination() != -1)
				notify (player, "@link %s = #%d", decompiled_name, db[object].get_destination());
		case TYPE_COMMAND:
			if (db[object].get_key() != NULL)
				notify (player, "@lock %s = %s", decompiled_name, db[object].get_key()->unparse_for_return(*this));
		case TYPE_VARIABLE:
		case TYPE_ALARM:
			if (db[object].get_odrop())
				notify (player, "@odrop %s = %s", decompiled_name, decompile_string(db[object].get_odrop()));
		case TYPE_FUSE:
			if (db[object].get_succ_message())
				notify (player, "@succ %s = %s", decompiled_name, decompile_string(db[object].get_succ_message()));

			if (db[object].get_fail_message())
				notify (player, "@fail %s = %s", decompiled_name, decompile_string(db[object].get_fail_message()));

			if (db[object].get_csucc() != -1)
				notify (player, "@csucc %s = #%d", decompiled_name, db[object].get_csucc());

			if (db[object].get_cfail() != -1)
				notify (player, "@cfail %s = #%d", decompiled_name, db[object].get_cfail());

			if (db[object].get_osuccess())
				notify (player, "@osucc %s = %s", decompiled_name, decompile_string(db[object].get_osuccess()));

			if (db[object].get_ofail())
				notify (player, "@ofail %s = %s", decompiled_name, decompile_string(db[object].get_ofail()));
			break;
		case TYPE_PROPERTY:
		case TYPE_DICTIONARY:
		case TYPE_ARRAY:
			break;
		default:
			notify (player, "I don't know what type that object is, so I can't decompile it.");
			return;

	}

	
	if (db[object].get_parent() != -1)
		notify (player, "@parent %s = #%d", decompiled_name, db[object].get_parent());


/* If an object is set readonly it must be the last thing you set on. I think the reasons for this are fairly obvious! */
	if (readonly_flag_set == true)
		notify (player, "@set %s = ReadOnly", decompiled_name);

	notify (player, "[Decompile Output End]");

	return_status= COMMAND_SUCC;
	set_return_string (ok_return_string);

}



/* static char SCCSid[] = "@(#)player.c	1.33\t7/27/95"; */
#include "copyright.h"

#include <string.h>
#include <time.h>
#include "os.h"
#if NEEDS_CRYPT_H
#	include <crypt.h>
#endif /* NEEDS_CRYPT_H */
#include "db.h"
#include "objects.h"
#include "config.h"
#include "interface.h"
#include "externs.h"
#include "command.h"
#include "match.h"
#include "context.h"
#include "colour.h"

dbref lookup_player(dbref player, const String& name)
{
	if(string_compare(name, "me")==0)
		return player;
	
	return db.lookup_player(name);
}

/* Return NOTHING if the player does not exist, or 0 if incorrect password (but player exists) */

dbref
connect_player (
const	String& name,
const	String& password)

{
	dbref	player;

	if ((player = lookup_player (NOTHING, name)) == NOTHING)
		return NOTHING;
	if(db[player].get_password ()
		&& string_compare(db[player].get_password (), (char *) (crypt(password.c_str(), password.c_str()) +2)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "WARNING: Connection attempt with password '%s'.", password.c_str());
		return 0;
	}
	notify_colour(player, player, COLOUR_ERROR_MESSAGES, "WARNING: Connection attempt successful\n");
	return player;
}

dbref create_player(const String& name, const String& password, bool effective_wizard)
{
	dbref	player;

	if (!ok_player_name(name) || !ok_password(password, effective_wizard))
		return NOTHING;

	/* else he doesn't already exist, create him */
	player = db.new_object(*new (Player));

	/* initialize everything */
	db[player].set_name		(name);
	db[player].set_location		(PLAYER_START);
	db[player].set_destination	(PLAYER_START);
	db[player].set_exits		(NOTHING);
	/* We are about to set the owner which
	   sets the referenced flag. We need to unset
	   it. It was either this or make a new
	   accessor function. This was easier and keener */
	db[player].set_owner		(player);
	db[player].set_flag		(FLAG_LISTEN);
	db[player].set_flag		(FLAG_PRETTYLOOK);
	db[player].set_flag		(FLAG_FCHAT);
	Settypeof			(player, TYPE_PLAYER);
	if(password)
	{
		db[player].set_password		((char *) (crypt(password.c_str(), password.c_str()) +2));
	}
	else
	{
		db[player].set_password(NULLSTRING);
	}
	db[player].set_score		(0);
	db[player].set_race		("Human");
	db[player].set_gravity_factor	(STANDARD_PLAYER_GRAVITY);
	db[player].set_mass		(STANDARD_PLAYER_MASS);
	db[player].set_volume		(STANDARD_PLAYER_VOLUME);
	db[player].set_mass_limit	(STANDARD_PLAYER_MASS_LIMIT);
	db[player].set_volume_limit	(STANDARD_PLAYER_VOLUME_LIMIT);
	db[player].set_controller	(player);
	db[player].set_build_id		(player);
	db[player].set_who_string	(NULLSTRING);
	db[player].set_email_addr	(NULLSTRING);

	db[player].clear_flag		(FLAG_REFERENCED);
	
	/* link him to PLAYER_START */
	PUSH(player, PLAYER_START, contents);

	/* add him to the auxiliary player cache */
	db.add_player_to_cache(player, name);

	/* Execute '.create' in #3 (if it exists) */

	dbref create = NOTHING;
	for(create = db[PLAYER_START].get_commands(); create != NOTHING; create = db[create].get_next())
	{
		if(string_compare(db[create].get_name(), ".create") == 0)
		{
			context* c = new context(player, context::DEFAULT_CONTEXT);
			c->prepare_compound_command(create, ".create", "", "");
			delete mud_scheduler.push_new_express_job(c);
		}
	}

	return player;
}

void
context::do_at_password (
const	String& old,
const	String& newpw)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!old)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Whats your old password?");
		return;
	}
	if(string_compare((char *) (crypt(old.c_str(), old.c_str()) + 2), db[player].get_password ()))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry");
		return;
	}

	if(!ok_password(newpw))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Bad new password.");
		return;
	}

	db[player].set_password ((crypt(newpw.c_str(), newpw.c_str()) +2));
	notify_colour(player, player, COLOUR_MESSAGES, "Password changed.");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


#ifdef ALIASES
void
context::do_at_listaliases (
const	String& person,
const	String& )

{
		int	i;
	const	char	*buf;
		dbref	owner;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!person)
	{
		owner = player;
//		notify_colour(player, player, COLOUR_MESSAGES, "List the aliases of whom?");
//		return;
	}
	else if ((string_compare(person, "me") == 0))
	{
		owner = player;
	}
	else if (person)
	{
		if ((owner = lookup_player(player, person)) == NOTHING)
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "No such player");
			return;
		}
	}
	else
	{
		owner = player;
	}

	/*
	 * Okay, so owner is the person we're examining, and we're
	 * reporting to 'player'.
   	 */
	const colour_at& ca=db[player].get_colour_at();
	notify_censor_colour(player, player, COLOUR_MESSAGES, "Full name: %s", db[owner].get_inherited_name().c_str());
	for (i = 0; i < MAX_ALIASES; i++)
	{
		buf = db [owner].get_alias (i).c_str();
		notify_censor(player, player, "%sAlias %d:%s %s", ca[COLOUR_TITLES],(i + 1), COLOUR_REVERT, (buf ? buf : "<not set>"));
	}
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_recall (
const	String& lines,
const	String& match)

{
	if(string_compare(lines, "clear") == 0)
	{
		db[player].ditch_recall();
		if(!gagged_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Recall buffer cleared.");
		RETURN_SUCC;
	}

        if (in_command())
        {
                notify_colour (player, player, COLOUR_ERROR_MESSAGES,  "Sorry, @recall may not be used within compound commands.");
		RETURN_FAIL;
                return;
        }

        int num_lines = DEFAULT_RECALL_LINES;

        if(lines)
        {
                num_lines = strtol (lines.c_str(), (char **)NULL, 10);

                if((num_lines <= 0) || (num_lines > MAX_RECALL_LINES))
                {
                        num_lines = DEFAULT_RECALL_LINES;
                }
        }

	if ( match.c_str() == NULL )
	{
        notify_norecall(player, "%s %d %s", "---------------Recalling",num_lines,"lines-----------------------------\n");
        db[player].output_recall_conditional("\0",num_lines, this);
        notify_norecall(player, "%s %d %s", "---------------End Recall of", num_lines, "lines-------------------------\n");
	}
	else
	{
        notify_norecall(player, "%s %d %s %s %s", "----------Recalling",num_lines,"lines, showing matches to:", match.c_str(),"-------------\n");
        db[player].output_recall_conditional(match,num_lines, this);
        notify_norecall(player, "%s %d %s %s %s", "----------End Recall of", num_lines, "lines showing matches to:",match.c_str(),"----------\n");
	}

	RETURN_SUCC;
}

void
context::do_at_alias (
const	String& person,
const	String& string)

{
	dbref	victim;
	int	test;
	int	result;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, person, TYPE_PLAYER, get_effective_id ());
	matcher.match_me ();
	matcher.match_neighbor ();
	matcher.match_player ();
	matcher.match_absolute ();
	if ((victim = matcher.noisy_match_result ()) == NOTHING)
		return;

	if (!controls_for_write (victim))
	{
		if (!gagged_command())
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if(db.lookup_player(string) == victim)
	{
		// We're changing an alias/name to ourselves
	}
	else
	{
		test = ok_alias_string(victim, string);
		if (!string || (test != -1))
		{
			if (!gagged_command())
			{
				if (test == 0)
				{
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Invalid alias string");
					return;
				}
				notify_censor_colour(player, player, COLOUR_ERROR_MESSAGES, "That name/alias is already in use by %s", db[test].get_name().c_str());
			}
			return;
		}
	}

	/*
	 * Find blank slot in the alias list.
	 */

	result = db[victim].add_alias(string);
	if (!gagged_command())
	{
		switch (result)
		{
			case 1:
				notify_colour(player, player, COLOUR_MESSAGES, "Alias added.");
				db.add_player_to_cache(victim, string);
				break;
			case 0:
				notify_colour(player, player, COLOUR_MESSAGES, "Only %d aliase%s are allowed.", MAX_ALIASES, PLURAL(MAX_ALIASES));
				break;
			case -1:
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Alias already exists.");
				break;
			default:
				break;
		}
	}

	dump_email_addresses();

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_at_unalias (
const	String& person,
const	String& string)

{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, person, TYPE_PLAYER, get_effective_id ());
	matcher.match_me ();
	matcher.match_neighbor ();
	matcher.match_player ();
	matcher.match_absolute ();
	if ((victim = matcher.noisy_match_result ()) == NOTHING)
		return;

	if (!controls_for_write (victim))
	{
		if (!gagged_command())
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if (db[victim].remove_alias(string))
	{
		if (!gagged_command())
		{
			notify_colour(player, player, COLOUR_MESSAGES, "Alias removed.");
			db.remove_player_from_cache(string);
		}
	}
	else
	{
		if (!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No such alias.");
		return;
	}

	dump_email_addresses();

	return_status = COMMAND_SUCC;
	set_return_string (db[victim].get_who_string ().c_str());
}
#endif /* ALIASES */


void
context::do_at_who (
const	String& person,
const	String& string)

{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, person, TYPE_PLAYER, get_effective_id ());
	matcher.match_me ();
	matcher.match_neighbor ();
	matcher.match_player ();
	matcher.match_absolute ();
	if ((victim = matcher.noisy_match_result ()) == NOTHING)
		return;

	if(Typeof(victim) != TYPE_PLAYER && Typeof(victim) != TYPE_PUPPET)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That is no t a player or NPC");
		return;
	}

	if (!controls_for_write (victim))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	if (string && (!ok_who_string (victim,string)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "That is not a valid WHO string.");
		return;
	}

	db[victim].set_who_string (string);

	if (really_in_command() && !Wizard(get_current_command()))
		notify_colour (victim, victim, COLOUR_ERROR_MESSAGES, "WARNING: Your who string has been changed to %s.", string.c_str());
	else if (!in_command())
		notify_colour (player,  player, COLOUR_MESSAGES,"Set.");


	return_status = COMMAND_SUCC;
	set_return_string (db[victim].get_who_string ().c_str());
}


void
context::do_at_controller (
const	String& name,
const	String& other)

{
	dbref victim;
	dbref controller;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher victim_matcher (player, name, TYPE_PLAYER, get_effective_id ());
	victim_matcher.match_me ();
	victim_matcher.match_neighbor ();
	victim_matcher.match_player ();
	victim_matcher.match_absolute ();
	if ((victim = victim_matcher.noisy_match_result ()) == NOTHING)
		return;
	
	Matcher controller_matcher (player, other, TYPE_PLAYER, get_effective_id ());
	controller_matcher.match_me ();
	controller_matcher.match_neighbor ();
	controller_matcher.match_player ();
	controller_matcher.match_absolute ();
	if ((controller = controller_matcher.noisy_match_result ()) == NOTHING)
		return;

	if (in_command())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot use @controller inside a command.");
		return;
	}
	if (!controls_for_write (victim))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot change the controller of that puppet.");
		return;
	}
	if ((Typeof (victim) != TYPE_PLAYER) && (Typeof (victim) != TYPE_PUPPET))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Non-players don't have controllers!");
		return;
	}
#if 0		/* If you want puppets not to be able to control other puppets, use this one */
	if (Typeof (controller) != TYPE_PLAYER)
#else
	if ((Typeof (controller) != TYPE_PLAYER) && (Typeof (controller) != TYPE_PUPPET))
#endif
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Controlled by whom?");
		return;
	}
	if (!controls_for_write (controller))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You cannot change the controller to that player.");
		return;
	}
	if (Wizard (victim))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Wizards cannot be puppets.");
		return;
	}

	/* @controller succeeds */
	if (victim == controller)
	{
		if(Typeof(victim) == TYPE_PUPPET)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "You can't make a puppet its own controller");
			return;
		}
		notify_colour(victim, victim, COLOUR_ERROR_MESSAGES, "You are no longer a puppet (your controller was %s).", getname (db[victim].get_controller ()));
		if (player != victim)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "%s is no longer a puppet.", getname (victim));
		}
	}
	else
	{
		notify_colour (player, player, COLOUR_MESSAGES, "Controller set.");
		if (player != victim)
		{
			notify_colour(victim, victim, COLOUR_ERROR_MESSAGES, "WARNING:  You are now controlled by %s.", getname (controller));
		}
	}

	if(Typeof(victim) == TYPE_PUPPET)
		db[victim].set_owner (controller);
	else
		db[victim].set_controller (controller);

	return_status = COMMAND_SUCC;
	set_return_string (unparse_for_return (*this, victim));
}

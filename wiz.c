/* static char SCCSid[] = "@(#)wiz.c	1.39\t10/19/95"; */
#include "copyright.h"

/* Wizard-only commands */

#if defined (linux) || (sun)
  #include <crypt.h>
#endif /* defined (linux) || (sun) */ 

#include "db.h"
#include "interface.h"
#include "match.h"
#include "externs.h"
#include "config.h"
#include "command.h"
#include "context.h"
#include "colour.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>


char 	player_booting[BUFFER_LEN];
char	boot_reason[BUFFER_LEN];

void
context::do_welcome (
const   CString& arg1,
const   CString& arg2)
{
        const char *message;

        return_status = COMMAND_FAIL;
        set_return_string (error_return_string);

        if (!Welcomer(player) && !(Welcomer(get_effective_id ())))
                notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
        else
        {
                message = reconstruct_message (arg1, arg2);
                if (message[0] == POSE_TOKEN)
		{
                        if ((message[1] == '\'') || (message[1] == '`'))
                          notify_welcomer_natter ("  %s%s", getname_inherited (player), message+1);
                        else
                          notify_welcomer_natter ("  %s %s", getname_inherited (player), message+1);
		}

                else
                        notify_welcomer_natter ("  %s natters \"%s\"", getname_inherited (player), message);
                return_status = COMMAND_SUCC;
                set_return_string (ok_return_string);
	}
}


void
context::do_force (
const	CString& what,
const	CString& command)

{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	/* get victim */
	if((victim = lookup_player(player, what)) == NOTHING)
	{
		Matcher matcher (player, what, TYPE_PUPPET, get_effective_id());
		matcher.match_everything ();
		if (((victim = matcher.noisy_match_result ()) == NOTHING) ||
			(Typeof(victim) != TYPE_PUPPET))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That player does not exist.");
			return;
		}
	}

	/* Check player/victim dependencies */
	if ((!controls_for_write (victim))
		|| (Wizard (victim) && !Wizard (player)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	/* force victim to do command */
	{
		context	*victim_context = new context (victim);
		if (in_command ())
			victim_context->calling_from_command ();

		count_down_fuses (*victim_context, victim, !TOM_FUSE);
		count_down_fuses (*victim_context, db[victim].get_location(), !TOM_FUSE);
		victim_context->commands_executed = commands_executed;
		victim_context->step_limit = step_limit;
		victim_context->process_basic_command (command.c_str());
		mud_scheduler.push_express_job (victim_context);
		/* Copy context information */
		copy_returns_from (*victim_context);
		if (!in_command())
			notify_colour(player,  player, COLOUR_MESSAGES, "Forced.");
		commands_executed = victim_context->commands_executed;
		delete victim_context;
	}
}


void
context::do_stats (
const	CString& name,
const	CString& )

{
	int	rooms	= 0;
	int	exits	= 0;
	int	things	= 0;
	int	players	= 0;
	int	frees	= 0;
	int	commands= 0;
	int	fuses	= 0;
	int	alarms	= 0;
	int	unknowns= 0;
	int	puppets	= 0;
	int	arrays	= 0;
	int	total	= 0;
	int	variables = 0;
	int	properties = 0;
	int	dictionaries = 0;
	int	npcs	= 0;
	int	ashcans	= 0;

	int	type;
	int	top = db.get_top();
	dbref	i;
	dbref	owner;
	char	buf [BUFFER_LEN];

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (name)
	{
		if ((owner = lookup_player(player, name)) == NOTHING)
		{
			notify_colour (player,  player, COLOUR_ERROR_MESSAGES, "No such player");
			return;
		}
		if (!controls_for_read (owner))
		{
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
			return;
		}
	}
	else
	{
		owner = player;
	}

	if (!name && Wizard (player))
		notify_colour(player, player, COLOUR_TITLES, "The universe contains %d objects:", top);
	else
		notify_colour(player, player, COLOUR_TITLES, "The universe contains %d objects, of which %s owns:", top, getname_inherited (owner));

	for(i = 0; i < top; i++)
	{
		type = Typeof(i);

		if (type == TYPE_FREE)
		{
			frees++;
			continue;
		}
		if(owner == NOTHING || owner == db[i].get_owner() || (type == TYPE_PLAYER && Puppet (i)) || (Wizard(player) && (!name)))
		{
			total++;
			if(Ashcan(i))
				ashcans++;
			switch(type)
			{
				case TYPE_ROOM:
					rooms++;
					break;
				case TYPE_EXIT:
					exits++;
					break;
				case TYPE_THING:
					things++;
					break;
				case TYPE_PLAYER:
					if (Puppet (i))
					{
						if ((db [i].get_controller () == owner) || (owner == NOTHING) || (Wizard(player) && (!name)))
							puppets++;
						else
							total--;
					}
					else
						players++;
					break;
				case TYPE_PUPPET:
					npcs++;
					break;
				case TYPE_VARIABLE:
					variables++;
					break;
				case TYPE_COMMAND:
					commands++;
					break;
				case TYPE_FUSE:
					fuses++;
					break;
				case TYPE_ALARM:
					alarms++;
					break;
				case TYPE_ARRAY:
					arrays++;
					break;
				case TYPE_DICTIONARY:
					dictionaries++;
					break;
				case TYPE_PROPERTY:
					properties++;
					break;
				default:
					unknowns++;
					break;
			}
		}
	}
	sprintf (buf,
		"%d objects = %d rooms, %d exits, %d things, %d players, %d puppets, %d NPC's, %d variables, %d properties, %d arrays, %d dictionaries, %d commands, %d fuses, %d alarms, %d free, %d unknowns, %d ashcanned.",
		total, rooms, exits, things, players, puppets, npcs, variables, properties, arrays, dictionaries, commands, fuses, alarms, frees, unknowns, ashcans);
	notify_colour (player, player, COLOUR_MESSAGES, buf);

	return_status = COMMAND_SUCC;
	sprintf (buf, "%d", total);
	set_return_string (buf);
}


void
context::do_newpassword (
const	CString& name,
const	CString& password)

{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (in_command())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES,"No password changes allowed in commands.");
		return;
	}

	if (!Wizard (player))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied);
	else if((victim = lookup_player(player, name)) == NOTHING)
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No such player.");
	else if (!controls_for_write (victim))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
	else if(password && !ok_password(password))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Bad password");
	else
	{
		/* it's ok, do it */
		if(password)
		{
			db[victim].set_password ((char *) crypt(password.c_str(),password.c_str()) + 2);
		}
		else
		{
			db[victim].set_password(NULL);
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "WARNING: Password set to NOTHING. The game will not prompt for a password");
		}
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Password changed.");
		notify_colour(victim, player, COLOUR_ERROR_MESSAGES, "Your password has been changed by %s.", getname_inherited (player));
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

    
void
context::do_boot (
const	CString& victim,
const	CString& reason)

{
	int allow = 0;
	dbref recipient;
	strcpy(player_booting, db[player].get_name().c_str());
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if (!victim)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "+++ Out of cheese error +++ MELON MELON MELON +++ Redo from start (And please specify a player)");
		return;
	}
	
	if((recipient = lookup_player(player, victim)) == NOTHING)
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That player does not exist.");
	if (recipient == player)
	{
		if (connection_count (player) > 1)
			allow=1;
		else
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You cannot boot your last remaining connection - use QUIT to quit.");
	}
	else if ((!Wizard (player)) && (!Apprentice (player)))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards and Apprentices may boot people.");
	else if (Wizard (recipient))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Oy! No booting Wizards!");
	else if ((Apprentice (recipient)) && (Apprentice (player)))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Apprentices may not boot other apprentices.");
	else
		allow=1;
	if(allow)
	{
		if (Connected (recipient))
		{
			strcpy(boot_reason, reason.c_str());
			notify_colour(recipient, player, COLOUR_ERROR_MESSAGES, "You got booted by %s%s%s.", getname_inherited (player), (!reason)?"":" ", boot_reason);
			boot_player (recipient,player);
			notify_colour(player, player, COLOUR_MESSAGES, "Booted.");
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			log_boot(recipient, db[recipient].get_name().c_str(), player, db[player].get_name().c_str(), (reason) ? boot_reason : "for no apparent reason");
		}
		else
			notify_colour(player, player, COLOUR_MESSAGES,"That player is not connected.");
	}
	boot_reason[0] = '\0';
	player_booting[0] = '\0';
}


void
context::do_from (
const	CString& name,
const	CString& )

{
	dbref		thing;
	dbref		an_object;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!name)
		thing = db[player].get_location();
	else
	{
		Matcher	matcher (player, name, TYPE_NO_TYPE, get_effective_id ());
		matcher.match_everything ();
		if ((thing = matcher.noisy_match_result()) == NOTHING)
			return;
	}

	if (!controls_for_read (thing))
	{
		notify_colour (player,  player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	for (an_object = 0; an_object < db.get_top (); an_object ++)
		if ((Typeof (an_object) == TYPE_EXIT) && (db[an_object].get_destination() == thing))
		{
			sprintf (scratch_buffer, "Object %s -> ", unparse_objectandarticle_inherited (*this, db [an_object].get_location(), ARTICLE_LOWER_INDEFINITE));
			strcat (scratch_buffer, unparse_objectandarticle_inherited (*this, an_object, ARTICLE_LOWER_INDEFINITE));
			notify_colour (player, player, COLOUR_ERROR_MESSAGES, scratch_buffer);
		}
	notify_colour (player, player, COLOUR_CONTENTS, "*** End of list ***");

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_recursionlimit (
const	CString& limit_string,
const	CString& )
{
	int	limit;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if ((limit = atoi (limit_string.c_str())) <= 0)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES,"Thats not a valid recursion limit.");
		return;
	}
	if (!(Wizard(get_effective_id ()) || Apprentice(get_effective_id()) || XBuilder(get_effective_id())) && (limit > COMPOUND_COMMAND_BASE_LIMIT))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "You must be a Wizard to set a recursionlimit greater than %d.", COMPOUND_COMMAND_BASE_LIMIT);
		return;
	}
	if (limit > COMPOUND_COMMAND_MAXIMUM_LIMIT)
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Maximum recursion limit cannot be exceeded.");
		return;
	}

	step_limit = limit;
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_email (
const	CString& name,
const	CString& email_addr)

{
	dbref	victim;
	int	i;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (in_command())
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "No email address changes allowed in commands.");
		return;
	}

	if (!Wizard (player))
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Only Wizards may change email addresses.");
	else if((victim = lookup_player(player, name)) == NOTHING)
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No such player.");
	else if (!controls_for_write (victim))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
	else
	{
		/* it's ok, do it */
		db[victim].set_email_addr (email_addr);

		/* Only do warning if email is not null */
		if(email_addr)
			for (i = 0; i < db.get_top (); i++)
			{
				if (i == victim)
					continue;
				if (Typeof (i)==TYPE_PLAYER && db[i].get_email_addr ())
					if (string_compare (email_addr, db[i].get_email_addr ().c_str())==0)
						notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Warning:  %s has identical email address (%s).", getname (i), db[i].get_email_addr ().c_str());
			}
		if(victim != player)
			notify_colour(player, player, COLOUR_MESSAGES,"Email address changed.");
		notify_colour(victim, victim, COLOUR_ERROR_MESSAGES, "Your email address has been changed to %s", db[victim].get_email_addr ().c_str());
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
		dump_email_addresses ();
	}
}

void
context::dump_email_addresses ()
{
#if !defined (linux) && !defined(__FreeBSD__) 
	extern	char	*sys_errlist[];
#endif /* !defined (linux) && !defined(__FreeBSD__) */
	int	i;
	FILE	*fp;

	/* Flup's email dumping routine */
	if ((fp=fopen (EMAIL_FILE ".tmp", "w"))==NULL)
		log_bug("couldn't open %s (%s)", EMAIL_FILE ".tmp", sys_errlist[errno]);
	else
	{
		for (i = 0; i < db.get_top (); i++)
			if (Typeof (i) == TYPE_PLAYER)
			{
				fprintf (fp, "%s|", db [i].get_name ().c_str());
				for (int j = 0; j < MAX_ALIASES; j++)
				{
					fprintf (fp, "%s|", db [i].get_alias (j).c_str());
				}
				fprintf (fp,
					"%d|%s|%s\n",
					i,
					(db [i].get_email_addr ())
						? db [i].get_email_addr ().c_str()
						: "NO_MAIL_ADDRESS",
					(NoHuhLogs(i) ? "NOHUH" : "HUH")	/* HUH logs? */
				);
			}
		fclose(fp);

		unlink(EMAIL_FILE); // Don't care if it fails.
		link(EMAIL_FILE ".tmp", EMAIL_FILE);
//		notify_colour (player, player, COLOUR_MESSAGES, "Done.");
	}

}

void
context::do_pcreate (
const	CString& name,
const	CString& password)

{
	dbref	result;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!Wizard (get_effective_id ()))
		notify_colour (player,player, COLOUR_ERROR_MESSAGES, "Only Wizards may create players.");
	else if ((result = create_player (name, password)) == NOTHING)
			notify_colour (player, player, COLOUR_ERROR_MESSAGES,"Illegal name or password.");
		else
		{
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			if (!in_command()) notify_colour (player, player, COLOUR_MESSAGES, "Player '%s' created with id #%d (location is #%d)", name.c_str(), result, PLAYER_START);
		}
}

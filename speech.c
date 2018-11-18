#include "copyright.h"

/* Commands which involve speaking */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "db.h"
#include "interface.h"
#include "lists.h"
#include "match.h"
#include "colour.h"
#include "config.h"
#include "externs.h"
#include "command.h"
#include "context.h"
#include "log.h"

#define MAX_IDLE_MESSAGE_LENGTH 70

void notify_except_colour (dbref first,dbref exception, const char *prefix,const char *msg, bool speechmarks, dbref talker, ColourAttribute colour);

static void
strcat_with_indent (
String&		dest,
const String&	src)

{
	if(src)
	{
		const char* p = src.c_str();
		while(*p)
		{
			dest += *p;
			if(*p == '\n')
			{
				dest += ' ';
				dest += ' ';
			}
			p++;
		}
	}
}


static void
strcpy_with_indent (
String&		dest,
const	String&	src)

{
	dest = "";
	strcat_with_indent (dest, src);
}


/*
 * this function is a kludge for regenerating messages split by '='
 */

String
reconstruct_message (
const	String& arg1,
const	String& arg2)
{
	String buf;

	/* Handle reconstruction */
	strcpy_with_indent (buf, arg1);
	if(arg2)
	{
		buf += " = ";
		strcat_with_indent (buf, arg2);
	}

	return (buf);
}


void
context::do_say (
const	String& arg1,
const	String& arg2)

{
		dbref	loc;
		dbref	prop;
		String	message;
	const	char	*say;
	const	char	*says;
	const	colour_at& ca=db[player].get_colour_at();
	if((loc = getloc(player)) == NOTHING)
		return;

	message = reconstruct_message(arg1, arg2);

        return_status = COMMAND_FAIL;
        set_return_string (error_return_string);

	/* You can't speak in a Silent room */

	if (Silent(loc) && !Wizard(player) && !Apprentice(player))
	{
		Matcher matcher_prop(player, ".silent", TYPE_PROPERTY, get_effective_id());
		matcher_prop.match_variable_remote(loc);

		if ((prop=matcher_prop.match_result()) == NOTHING || (prop == AMBIGUOUS))
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't speak in a silent room!");
		else
			notify_public_colour(player, player, COLOUR_ERROR_MESSAGES, db[prop].get_inherited_description().c_str() );
		return;
	}

	/* Silent players can't speak */
	if (Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player,player, COLOUR_ERROR_MESSAGES, "You have been set Silent. You cannot speak.");
		return;
	}
   

	/* you can't say nothing */
	if (!message)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "What do you want to say?");
		return;
	}

	/* notify everybody */
	switch(message[message.length() - 1])
	{
		case '?':
			say = "ask";
			says = "asks";
			break;
		case '!':
			say = "exclaim";
			says = "exclaims";
			break;
		default:
			say = "say";
			says = "says";
			break;
	}
	notify_public(player, player, "%sYou %s \"%s%s%s\"", ca[rank_colour(player)], say, ca[COLOUR_SAYS], message.c_str(), ca[rank_colour(player)]);

	sprintf(scratch_buffer, "%s %s ", getname_inherited (player),says);
	notify_except_colour(db[loc].get_contents(),
				player,
				scratch_buffer,
			        message.c_str(),
				true,
				player,
				COLOUR_SAYS);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_whisper (
const	String& arg1,
const	String& arg2)

{
#ifndef	QUIET_WHISPER
	dbref loc;
#endif	/* QUIET_WHISPER */
	dbref who,where,prop;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	Matcher matcher (player, arg1, TYPE_PLAYER, get_effective_id ());
	matcher.match_neighbor();
	matcher.match_me();
	if(Wizard(get_effective_id ())){
		matcher.match_absolute();
		matcher.match_player();
	}
	
	/* Silent players can't whisper. */
	if (Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are set silent, you cannot whisper.");
		return;
	}

	/* None of this whispering in silent rooms */

	where= db[player].get_location();
	if (Silent(where) && !Wizard(player) && !Apprentice(player))
	{
		Matcher matcher_prop(player, ".silent", TYPE_PROPERTY, get_effective_id());
		matcher_prop.match_variable_remote(where);

		if ((prop=matcher_prop.match_result()) == NOTHING || (prop == AMBIGUOUS))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't whisper in a silent room!");
		}
		else
		{
				notify_public_colour(player, player, COLOUR_ERROR_MESSAGES, db[prop].get_inherited_description().c_str() );
		}
		return;
	}

	switch(who = matcher.match_result())
	{
		case NOTHING:
			notify_colour(player, player, COLOUR_MESSAGES,"Whisper to whom?");
			break;
		case AMBIGUOUS:
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "I don't know who you mean!");
			break;
		default:
			if (Typeof(who) != TYPE_PLAYER)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only whisper to players.");
				return;
			}
			notify_public_colour(player, who, COLOUR_WHISPERS, "You whisper \"%s\" to %s.", arg2.c_str(), getname_inherited (who));
			notify_public_colour(who, who, COLOUR_WHISPERS, "%s whispers \"%s\"", getname_inherited (player), arg2.c_str());
			Accessed (who);
#ifndef QUIET_WHISPER
			sprintf(scratch_buffer, "%s whispers something to %s.",
				getname_inherited (player), getname_inherited (who));
			if((loc = getloc(player)) != NOTHING)
			{
				notify_except2(db[loc].get_contents(), player, player, who, scratch_buffer);
			}
#endif /* QUIET_WHISPER */
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			break;
	}
}
 

void
context::do_at_areanotify (
const	String& arg1,
const	String& arg2)

{
	dbref loc;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
        Matcher where_matcher (player, arg1, TYPE_ROOM, get_effective_id ());
        where_matcher.match_everything ();
        if (((loc = where_matcher.match_result ()) == NOTHING)
                || (loc == AMBIGUOUS)
                || (loc < 0)
                || (loc >= db.get_top ())
//                || ((Typeof (loc) != TYPE_ROOM) && !Container (loc)) // Should we restrict it to rooms or containers?
                || !controls_for_read (loc))
        {
                notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
                return;
        }

	/* notify everybody */
	notify_area(loc, player, "%s", arg2.c_str());
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_notify (
const	String& arg1,
const	String& arg2)

{
	dbref loc;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if((loc = getloc(player)) == NOTHING)
		return;

	if (!controls_for_read (loc))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
		return;
	}

	/* notify everybody */
	notify_area(loc, player, "%s", reconstruct_message(arg1, arg2).c_str());
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_pose (
const	String& arg1,
const	String& arg2)

{
	do_emote(arg1, arg2, false);
}

void
context::do_emote(const String& arg1, const String& arg2, bool oemote)
{
	dbref loc, prop;
	String message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if((loc = getloc(player)) == NOTHING)
		return;

	message = reconstruct_message(arg1, arg2);

	/* Silent players can't emote. */
	
	if(Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		if(!oemote)
			notify_colour(player,  player, COLOUR_MESSAGES,"You are set Silent. You can't emote anything.");
		return;
	}
	
	/* No emoting in silent rooms */

	if(Silent(loc) && !Wizard(player) && !Apprentice(player))
	{
	
	  if(string_compare(message,"has connected")==0)
	  {
		  if(DontAnnounce(player))
			  return;
	  }
	  else
	  {
		if(!oemote)
		{
			Matcher matcher_prop(player, ".silent", TYPE_PROPERTY, get_effective_id());
			matcher_prop.match_variable_remote(loc);

			if ((prop=matcher_prop.match_result()) == NOTHING || (prop == AMBIGUOUS))
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't emote in a silent room!");
			else
				notify_public_colour(player, player, COLOUR_ERROR_MESSAGES, db[prop].get_inherited_description().c_str() );
		}
		return;
	  }

	}

	/* you can't pose nothing */
	if (!message)
	{
		if(!oemote)
			notify_colour(player, player, COLOUR_MESSAGES, "You can't emote nothing!");
		return;
	}

	/* notify everybody */
	if ((message[0] == '\'') || (message[0] == '`'))
		sprintf(scratch_buffer, "%s", getname_inherited (player));
	else
		sprintf(scratch_buffer, "%s ", getname_inherited (player));
	dbref except = NOTHING;
	if(oemote)
		except = player;

	notify_except_colour(db[loc].get_contents(), except, scratch_buffer, message.c_str(), false, player, COLOUR_EMOTES);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_wall (
const	String& arg1,
const	String& arg2)

{
	String message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	message = reconstruct_message(arg1, arg2);
	if (Wizard (get_effective_id ()) || Apprentice (player))
	{
#ifdef	LOG_WALLS
		log_wall(player, getname_inherited(player), message.c_str());
#endif	/* LOG_WALLS */
		if(message)
		{
			notify_all ("%s shouts \"%s\"", getname_inherited (player), message.c_str());
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
		}
		else
			notify_colour(player, player, COLOUR_MESSAGES,"Now try a message that isn't blank.");
	}
	else
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards and Apprentices may shout.");
}

void
context::do_at_note (
const	String& arg1,
const	String& arg2)

{
	String message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	message = reconstruct_message(arg1, arg2);

	if(!message)
		notify_colour(player, player, COLOUR_MESSAGES, "Not really worth taking a note of, huh?");
	else
	{
		dbref npc_owner;
		/* if the player is actually an NPC we want to redirect the @note
		 * to the owner of the NPC
		 */
		if (Typeof(player) == TYPE_PUPPET) {
			npc_owner = db[player].get_owner();
		}
		else {
			npc_owner = -1;
		}
		log_note(
				player,
				getname_inherited (player),
				db[player].get_location(),
				getname_inherited (db[player].get_location ()),
				npc_owner,
				controls_for_read (db[player].get_location()) ? true : false,
				message.c_str()
		);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Your note has been logged.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_gripe (
const	String& arg1,
const	String& arg2)

{
	dbref loc;
	String message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	loc = db[player].get_location();
	message = reconstruct_message(arg1, arg2);

	if(in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No griping in commands.");
		return;
	}

	if(!message)
		notify_colour(player, player, COLOUR_MESSAGES, "Anything in particular, or just generally moaning?");
	else
	{
		sprintf(scratch_buffer, "GRIPE from %s(%d) in %s%s(%d): %s\n",
			getname_inherited (player), (int)player,
			getarticle (loc, ARTICLE_LOWER_INDEFINITE),
			getname_inherited (loc), (int)loc,
			message.c_str());
		log_gripe(	player, getname_inherited(player),
					loc,	getname_inherited(loc),
					message.c_str()
		);
		notify_colour(player, player, COLOUR_MESSAGES, "Your complaint has been logged.");
		notify_wizard ("%s", scratch_buffer);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_page (
const	String& part1,
const	String& part2)

{
	Player_list	targets(player);
	Player_list	all_targets(player);

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!part1)
	{
		if (!in_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: page <playerlist> = <message>");
		return;
	}

	if(Typeof(player) == TYPE_PUPPET)
	{
		notify(player, "Sorry, NPC's can't page");
		return;
	}

	/* You can't page if you are set silent */

	if(Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are set Silent. You can't send pages.");
		return;
	}

	char* duparg1=(part1)?strdup(part1.c_str()):NULL;
	char* duparg2=(part2)?strdup(part2.c_str()):NULL;
	char *arg1=duparg1;
	char *arg2=duparg2;

	if (arg2==NULL)
	{
		char *p=arg1;
		char *wordstart=arg1;
		while (*p)
		{
			wordstart=p;

			/* Skip the player name */
			while (*p && isalnum(*p))
				p++;

			/* Do we have a separator? */
			if ((*p=='\0') || ((*p != ';') && (*p != ',')))
			{
				wordstart=p;
				break;
			}

			/* Skip any spaces (or any separators)*/
			while (*p && ((*p==' ') || (*p==',') || (*p==';')))
			    p++;

			if (*p=='\0')
			{
			    wordstart=p;
			    break;
			}
		}
		arg2=wordstart;
		while (*arg2 && (*arg2==' '))
			arg2++;
		*wordstart='\0';
	}

	targets.include_unconnected();
	targets.build_from_text(player, arg1);
	targets.exclude_from_reverse_list(player, PLIST_IGNORE, "!has set the 'ignore' flag on you so you can't page them.");
	targets.filter_out_if_unset(FLAG_CONNECTED, "*");
	int howmany;

	dbref	temp_all=targets.get_first();
	while (temp_all != NOTHING)
	{
		all_targets.include(temp_all);
		temp_all= targets.get_next();
	}



	if ((howmany=targets.filter_out_if_set(FLAG_HAVEN, "#"))==0)
	{
		if (targets.get_realsize() == 0)
			notify_colour(player, player, COLOUR_MESSAGES, "You didn't specify any valid names to page!");
		else if (targets.get_realsize() > 1)
			notify_colour(player, player, COLOUR_MESSAGES, "Those players are either set haven or are not connected.");
		free(duparg1);
		free(duparg2);
		return;
	}
	else
	{
		if (blank(arg2))
			strcpy (scratch_buffer, "tries to contact you.\n");
		else if (*arg2==':')
		{
			strcpy (scratch_buffer, arg2+1);
			strcat (scratch_buffer, "\n");
		}
		else
		{
			sprintf(scratch_buffer, "says \"%s\"",arg2);
			strcat (scratch_buffer, "\n");
		}
		char *suffix=strdup(scratch_buffer);

		targets.warn_me_if_idle();
		targets.beep();

		/* Need to generate different headers depending on who we're sending to
		 * Therefore step thru the list ourself.
		 */
	
		dbref target=targets.get_first();

		while (target != NOTHING)
		{
			const colour_at& ca=db[target].get_colour_at();
			context	unparse_context (target, *this);
			strcpy (scratch_buffer, unparse_objectandarticle_inherited (unparse_context, db[player].get_location(), ARTICLE_LOWER_INDEFINITE).c_str());
			strcat (scratch_buffer, ", ");
			strcat (scratch_buffer, ca[rank_colour(player)]);
			strcat (scratch_buffer, unparse_object_inherited (unparse_context, player).c_str());

			if (howmany > 1)
				notify_censor_colour (target, player, COLOUR_PAGES, "%%w%%hPAGING%%z to %s from %s%s %s%s", targets.generate_courtesy_string(player, target), ca[COLOUR_ROOMNAME], scratch_buffer, ca[COLOUR_PAGES], boldify(target, suffix));
			else
				notify_censor_colour (target, player, COLOUR_PAGES, "%%w%%hPAGING%%z from %s%s%s %s", ca[COLOUR_ROOMNAME], scratch_buffer, ca[COLOUR_PAGES], boldify(target, suffix));

			Accessed (target);

			target=targets.get_next();
		}

		free (suffix);
	}

	all_targets.trigger_command(".page", *this);
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES,  "Message sent to %s.", targets.generate_courtesy_string(player, player, true));

	if (Haven (player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "WARNING: Your HAVEN flag has been removed because of this page.");
		db[player].clear_flag(FLAG_HAVEN);
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	free (duparg1);
	free (duparg2);

}


void
context::do_at_natter (
const	String& arg1,
const	String& arg2)
{
	String message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!(Wizard (get_effective_id ()) || Apprentice (player) || Natter (player)))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards, Apprentices and those set Natter may use the @natter channel.");
	}
	else if(!arg1 && !arg2)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "What do you want to natter?");
	}
	else
	{
		message = reconstruct_message (arg1, arg2);
		if (message[0] == POSE_TOKEN)
			{
			if ((message[1] == '\'') || (message[1] == '`'))
			  notify_wizard_natter ("  %s%s", getname_inherited (player), message.c_str()+1);
			else
			  notify_wizard_natter ("  %s %s", getname_inherited (player), message.c_str()+1);
			}

		else
			notify_wizard_natter ("  %s natters \"%s\"", getname_inherited (player), message.c_str());
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_at_echo (
const	String& arg1,
const	String& arg2)

{
	
	notify_censor_colour(player, player, COLOUR_MESSAGES, "%s%s", reconstruct_message(arg1, arg2).c_str(), COLOUR_REVERT);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_oecho (
const	String& arg1,
const	String& arg2)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if (controls_for_write (db[player].get_location()))
	{
		sprintf (scratch_buffer, "%s", reconstruct_message(arg1, arg2).c_str());
		notify_except (db [db [player].get_location()].get_contents(), player, player, scratch_buffer);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
	else
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must own the room to use @oecho in it.");
}


void
context::do_at_oemote (
const	String& arg1,
const	String& arg2)

{
	do_emote(arg1, arg2, true);
}

void
context::do_at_pemote (
const	String& name,
const	String& what)

{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (name)
	{
		if ((victim = lookup_player(player, name)) == NOTHING)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That player does not exist.");
			return;
		}

		/* The victim must be in a location that the executor has controls_for_read on */
		/* For mortals this means a room that they own, or any room that is set visible */
		/* For wizards it means anywhere */

		if (controls_for_read(db[victim].get_location()))
		{
			if ((*what.c_str() == '\'') || (*what.c_str() == '`'))
				sprintf(scratch_buffer, "%s%s", getname_inherited (player), what.c_str());
			else
				sprintf(scratch_buffer, "%s %s", getname_inherited (player), what.c_str());
			notify_censor(victim, player, scratch_buffer);
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
			return;
		}
		else
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only emote to players in locations owned by you.");
			return;
		}
	}
	notify_colour(player, player, COLOUR_MESSAGES, "Emote to who?");
	return;
}

void
context::do_at_notify (
const	String& names,
const	String& what)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!names || !what)
	{
		if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @notify <playerlist> = message");
		return;
	}
	return_status=COMMAND_SUCC;
	set_return_string(ok_return_string);

	Player_list targets(player);
	targets.build_from_text(player, names);
	int count=targets.filter_out_if_unset(FLAG_CONNECTED);
	if (count==0)
		return;

	dbref victim=targets.get_first();
	while (victim!=NOTHING)
	{
		if (!controls_for_read(db[victim].get_location()))
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only @notify to someone who is in a room that you own.");
		else
			notify_censor(victim, player, "%s", what.c_str());
		victim=targets.get_next();
	}
	if (!in_command())
		notify(player, "Notified.");
}

void
context::do_fchat (
const	String& arg1,
const	String& arg2)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!arg1)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "What do you want to chat to your friends?");
		return;
	}

	if (!Fchat(player))
	{
		if (!in_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must have your fchat flag set to use fchat.");
		return;
	}

	return_status=COMMAND_SUCC;
	set_return_string(ok_return_string);

	Player_list targets(player);
	targets.include_from_list(player, PLIST_FRIEND);
	targets.include_from_reverse_list(player, PLIST_FRIEND);
	targets.filter_out_if_unset(FLAG_FCHAT);
	targets.filter_out_if_unset(FLAG_CONNECTED);
	targets.exclude_from_reverse_list(player, PLIST_IGNORE);
	targets.exclude_from_reverse_list(player, PLIST_FBLOCK);
	targets.include(player);
	String output=reconstruct_message(arg1, arg2);
	if (output[0] == ':')
		sprintf(scratch_buffer, "%%y%%h[FRIENDS] %%w%s%s%s", db[player].get_name().c_str(), (output[1] == '\'') ? "" : " ", output.c_str()+1); 
	else
		sprintf(scratch_buffer, "%%y%%h[FRIENDS] %%w%s says \"%s\"", db[player].get_name().c_str(), output.c_str()); 
	dbref target=targets.get_first();

	while (target != NOTHING)
	{
		notify_censor(target, player, "%s", scratch_buffer);
		target=targets.get_next();
	}
	
	return;
}

/*  This function is very specific to the speech code */

void
notify_except_colour (
dbref		first,
dbref		exception,
const	char	*prefix,
const	char	*msg,
bool		speechmarks,
dbref		talker,
ColourAttribute	colour)

{
	const char *myprefix;
	const char *mymsg;

	int docensor=Censored(db[talker].get_location());
	if (docensor)
	{
		myprefix=strdup(censor(prefix));
		mymsg=strdup(censor(msg));
	}
	else
	{
		myprefix=prefix;
		mymsg=msg;
	}

	DOLIST (first, first)
		if ((Connected (first) ) && (first != exception))
		{

			const colour_at& ca=db[first].get_colour_at();
			if (speechmarks==true)
				notify_public(first, talker, "%s%s\"%s%s%s\"%%z", ca[rank_colour(talker)], myprefix, ca[colour],  mymsg, ca[rank_colour(talker)]);
			else
				notify_public(first, talker, "%s%s%s%s%%z", ca[rank_colour(talker)], myprefix, ca[colour],  mymsg);
			
		}
	if (docensor)
	{
		free (const_cast <char *> (mymsg));
		free (const_cast <char *> (myprefix));
	}
}

void
notify_except (
dbref		first,
dbref		originator,
dbref		exception,
const	String&	msg)

{
	notify_except(first, originator, exception, msg.c_str());
}

void
notify_except (
dbref		first,
dbref		originator,
dbref		exception,
const	char	*msg)

{
	DOLIST (first, first)
		if ((Connected (first) ) && (first != exception))
			notify_public (first, originator, "%s", msg);
}


void
notify_except2 (
dbref		first,
dbref		originator,
dbref		exc1,
dbref		exc2,
const	String&	msg)

{
	notify_except2(first, originator, exc1, exc2, msg.c_str());
}

void
notify_except2 (
dbref		first,
dbref		originator,
dbref		exc1,
dbref		exc2,
const	char	*msg)

{
	DOLIST (first, first)
		if ((Connected (first) ) && (first != exc1) && (first != exc2))
			notify_public (first, originator, "%s", msg);
}


int
blank (
const	char	*s)

{
	if (s == NULL)
		return (1);
	while (*s && isspace(*s))
		s++;

	return !(*s);
}

bool
really_do_tell(char *arg1, char *arg2, bool force_emote, context &c)
{
	Player_list	targets(c.get_player());
	Player_list	all_targets(c.get_player());
	dbref		player= c.get_player();
	int	count;

	if (blank(arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: tell <playerlist> [=] message");
		return false;
	}
	if (blank(arg2))
	{
		/* Damn, we have some work to do to find the end of the names and the start of the tell

			Here's how we're going to treat it:
				tell fred,jim,excess luggage Hi 
				will send luggage hi to fred,jim and excess luggage.
		*/

		/* Pattern to match: (<word><separator>)*<space><message>
				<word>=<letter>*
				<separator>=,' '*
				<message>=<letter>*
				<letter>={A-Z} */

		char *p=arg1;
		char *wordstart=arg1;
		while (*p)
		{
			wordstart=p;

			/* Skip the player name */
			while (*p && isalnum(*p))
				p++;

			/* Do we have a separator? */
			if ((*p=='\0') || ((*p != ';') && (*p != ',')))
			{
				wordstart=p;
				break;
			}

			/* Skip any spaces (or any separators)*/
			while (*p && ((*p==' ') || (*p==',') || (*p==';')))
			    p++;

			if (*p=='\0')
			{
			    wordstart=p;
			    break;
			}
		}
		arg2=wordstart;
		while (*arg2 && (*arg2==' '))
			arg2++;
		*wordstart='\0';
	}

	if (blank(arg1) || blank(arg2))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Usage:  tell <playerlist> [=] <message>");
		notify_colour(player, player, COLOUR_MESSAGES, "(see 'help playerlist' for more information about player lists.)");
		return false;
	}

	/* You can't tell if you are set silent */

	if(Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are set Silent. You can't 'tell' to people.");
		return false;
	}
	targets.include_unconnected();
	count=targets.build_from_text(player, arg1);

	targets.exclude(player, "You must be very forgetful to need to tell yourself!");
	count=targets.filter_out_if_unset(FLAG_CONNECTED, "*");

	dbref	temp_all=targets.get_first();
	while (temp_all != NOTHING)
	{
		all_targets.include(temp_all);
		temp_all= targets.get_next();
	}


	targets.exclude_from_reverse_list(player, PLIST_IGNORE, "!has set the 'ignore' flag on you.");
	count=targets.filter_out_if_set(FLAG_HAVEN, "#");

	
	if(count==0)	/* Nothing valid to send the message to. */
	{
		if (targets.get_realsize() == 0)
			notify_colour(player, player, COLOUR_MESSAGES, "You didn't specify any valid names to tell to!");
		else if (targets.get_realsize() > 1)
			notify_colour(player, player, COLOUR_MESSAGES, "Those players are either set haven or are not connected.");
		all_targets.trigger_command(".tell", c);
		return false;
	}

	targets.warn_me_if_idle();

	if (Haven(player))
	{
		db[player].clear_flag(FLAG_HAVEN);
		notify_colour(player, player, COLOUR_MESSAGES, "Your Haven flag has been removed because of this tell.");
	}


	dbref target= targets.get_first();
	while (target != NOTHING)
	{

		const colour_at& ca=db[target].get_colour_at();
		if ((*arg2==':') || (force_emote==true))
		{
			char *s=(force_emote==true)?arg2:arg2+1;

			notify_censor (target, player, "%s[To %s] %s%s%s%s", ca[COLOUR_TELLS], targets.generate_courtesy_string(player, target), db[player].get_name().c_str(), (*s=='\'' || *s=='`')? "":" ", ca[COLOUR_TELLMESSAGES], s);
		}
		else
			notify_censor (target, player, "%s[To %s] %s tells you \"%s%s%s\"", ca[COLOUR_TELLS], targets.generate_courtesy_string(player, target), db[player].get_name().c_str(), ca[COLOUR_TELLMESSAGES], arg2, ca[COLOUR_TELLS]);
		target= targets.get_next();
	}

	all_targets.trigger_command(".tell", c);

	const colour_at& ca=db[player].get_colour_at();
	if ((*arg2==':') || (force_emote==true))
	{
		char *s=(force_emote==true)?arg2:arg2+1;

		notify(player,"%s[To %s] %s%s%s%s",ca[COLOUR_TELLS], targets.generate_courtesy_string(player, player, true), db[player].get_name().c_str(), (*s=='\'' || *s=='`')? "":" ", ca[COLOUR_TELLMESSAGES], s);
	}
	else
		notify(player,"%sYou tell %s \"%s%s%s\"",ca[COLOUR_TELLS], targets.generate_courtesy_string(player, player, true), ca[COLOUR_TELLMESSAGES], arg2, ca[COLOUR_TELLS]);

	return true;
}

/*
 * tell (<player> [; <player> [...]] | <listname> | <channel number>) = [:] <message>)
 *
 * List names take precedence (unless there aren't any!).
 *
 */

void context::do_tellemote (const String& part1, const String& part2)
{
	char *arg1;
	char *arg2;

	arg1=(part1)?strdup(part1.c_str()):NULL;
	arg2=(part2)?strdup(part2.c_str()):NULL;

	if (really_do_tell(arg1, arg2, true, *this)==true)
	{
		return_status=COMMAND_FAIL;
		set_return_string(error_return_string);
	}
	else
	{
		return_status=COMMAND_SUCC;
		set_return_string(ok_return_string);
	}
	free(arg1);
	free(arg2);

}

void context::do_tell(const String& part1, const String& part2)
{
	char *arg1;
	char *arg2;

	arg1=(!part1)?NULL:strdup(part1.c_str());
	arg2=(!part2)?NULL:strdup(part2.c_str());

	if (really_do_tell(arg1, arg2, false, *this)==true)
	{
		return_status=COMMAND_FAIL;
		set_return_string(error_return_string);
	}
	else
	{
		return_status=COMMAND_SUCC;
		set_return_string(ok_return_string);
	}

	free(arg1);
	free(arg2);
}

void context::do_at_censor(const String& arg1, const String& arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@censor can only be used by Apprentices or Wizards.");
		return;
	}

	if (!arg1)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @censor <word>");
		return;
	}

	const char *p=arg1.c_str();
	while (*p)
	{
		if (!isalnum(*p) || isspace(*p))
		{
			if (!in_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's not a valid thing to censor.");
			return;
		}
		p++;
	}

	if (add_rude(arg1) == false)
	{
		if (!in_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Error: Unable to add word to censor list.");
		return;
	}
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Censored.");
	return_status=COMMAND_SUCC;
	set_return_string(ok_return_string);
	return;
}


void context::do_at_exclude(const String& arg1, const String& arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@exclude can only be used by Apprentices or Wizards.");
		return;
	}

	if (arg2)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @exclude <word>");
		return;
	}

	const char *p=arg1.c_str();
	while (*p)
	{
		if (!isalnum(*p) || isspace(*p))
		{
			if (!in_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's not a valid thing to exclude.");
			return;
		}
		p++;
	}

	if (add_excluded(arg1) == false)
	{
		if (!in_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Error: Unable to add word to excluded list.");
		return;
	}
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Excluded.");
	return_status=COMMAND_SUCC;
	set_return_string(ok_return_string);
	return;
}


void context::do_at_uncensor(const String& arg1, const String& arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@uncensor can only be used by Apprentices or Wizards.");
		return;
	}

	if (arg2)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @uncensor <word>");
		return;
	}

	const char *p=arg1.c_str();
	while (*p)
	{
		if (!isalnum(*p) || isspace(*p))
		{
			if (!in_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's not a valid thing to censor.");
			return;
		}
		p++;
	}

	if (un_rude(arg1) == false)
	{
		if (!in_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Error: Unable to remove word from censored list.");
		return;
	}

	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Uncensored.");
	return_status=COMMAND_SUCC;
	set_return_string(ok_return_string);
	return;
}


void context::do_at_unexclude(const String& arg1, const String& arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@unexclude can only be used by Apprentices or Wizards.");
		return;
	}

	if (arg2)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @unexclude <word>");
		return;
	}

	const char *p=arg1.c_str();
	while (*p)
	{
		if (!isalnum(*p) || isspace(*p))
		{
			if (!in_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's not a valid thing to unexclude.");
			return;
		}
		p++;
	}

	if (un_exclude(arg1) == false)
	{
		if (!in_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Error: Unable to remove word from excluded list.");
		return;
	}
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES, "Unexcluded");
	return_status=COMMAND_SUCC;
	set_return_string(ok_return_string);
	return;
}

char SCCSid[] = "@(#)speech.c	1.60\t9/19/95";
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

#define MAX_IDLE_MESSAGE_LENGTH 70

void notify_except_colour (dbref first,dbref exception, const char *prefix,const char *msg, Boolean speechmarks, dbref talker,int colour);

static void
strcpy_with_indent (
char		*dest,
const	char	*src)

{
	while (src && *src)
		if (*src == '\n')
		{
			*dest++ = *src++;
			*dest++ = ' ';
			*dest++ = ' ';
		}
		else
			*dest++ = *src++;
	*dest = '\0';
}


static void
strcat_with_indent (
char		*dest,
const	char	*src)

{
	strcpy_with_indent (dest + strlen (dest), src);
}


/*
 * this function is a kludge for regenerating messages split by '='
 */

const char *
reconstruct_message (
const	char	*arg1,
const	char	*arg2)
{
	static	char	buf [BUFFER_LEN];

	/* Handle reconstruction */
	strcpy_with_indent (buf, arg1);
	if(arg2 && *arg2)
	{
		strcat (buf, " = ");
		strcat_with_indent (buf, arg2);
	}

	return (buf);
}


void
context::do_say (
const	char	*arg1,
const	char	*arg2)

{
		dbref	loc;
		dbref	prop;
	const	char	*message;
		int	length;
	const	char	*say;
	const	char	*says;
	const	char *const *const colour_at=db[player].get_colour_at();
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
			notify_public_colour(player, player, COLOUR_ERROR_MESSAGES, db[prop].get_inherited_description() );
		return;
	}

	/* Silent players can't speak */
	if (Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player,player, COLOUR_ERROR_MESSAGES, "You have been set Silent. You cannot speak.");
		return;
	}
   

	/* you can't say nothing */
	if ((length = strlen(message)) == 0)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "What do you want to say?");
		return;
	}

	/* notify everybody */
	switch(message[length - 1])
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
	notify_public(player, player, "%sYou %s \"%s%s%s\"", colour_at[rank_colour(player)], say, colour_at[COLOUR_SAYS], message, colour_at[rank_colour(player)]);

	sprintf(scratch_buffer, "%s %s ", getname_inherited (player),says);
	notify_except_colour(db[loc].get_contents(),
				player,
				scratch_buffer,
			        message,
				True,
				player,
				COLOUR_SAYS);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_whisper (
const	char	*arg1,
const	char	*arg2)

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
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't whisper in a silent room!");
		else
				notify_public_colour(player, player, COLOUR_ERROR_MESSAGES, db[prop].get_inherited_description() );
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
			notify_public_colour(player, who, COLOUR_WHISPERS, "You whisper \"%s\" to %s.", arg2, getname_inherited (who));
			notify_public_colour(who, who, COLOUR_WHISPERS, "%s whispers \"%s\"", getname_inherited (player), arg2);
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
context::do_notify (
const	char	*arg1,
const	char	*arg2)

{
	dbref loc;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if((loc = getloc(player)) == NOTHING)
		return;

	if (!controls_for_read (loc))
	{
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, permission_denied);
		return;
	}

	/* notify everybody */
	notify_area(loc, player, "%s", reconstruct_message(arg1, arg2));
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_pose (
const	char	*arg1,
const	char	*arg2)

{
	dbref loc, prop;
	const char *message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if((loc = getloc(player)) == NOTHING)
		return;

	message = reconstruct_message(arg1, arg2);

	/* Silent players can't emote. */
	
	if(Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player,  player, COLOUR_MESSAGES,"You are set Silent. You can't emote anything.");
		return;
	}
	
	/* No emoting in silent rooms */

	if(Silent(loc) && !Wizard(player) && !Apprentice(player))
	{
	
	  if(strcmp(message,"has connected")==0);
	  else
	  {
		Matcher matcher_prop(player, ".silent", TYPE_PROPERTY, get_effective_id());
		matcher_prop.match_variable_remote(loc);

		if ((prop=matcher_prop.match_result()) == NOTHING || (prop == AMBIGUOUS))
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't emote in a silent room!");
		else
			notify_public_colour(player, player, COLOUR_ERROR_MESSAGES, db[prop].get_inherited_description() );
		return;
	  }

	}

	/* you can't pose nothing */
	if (strlen(message) == 0)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You can't emote nothing!");
		return;
	}

	/* notify everybody */
	if ((*message == '\'') || (*message == '`'))
		sprintf(scratch_buffer, "%s", getname_inherited (player));
	else
		sprintf(scratch_buffer, "%s ", getname_inherited (player));
	notify_except_colour(db[loc].get_contents(), NOTHING, scratch_buffer, message, False, player, COLOUR_EMOTES);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_wall (
const	char	*arg1,
const	char	*arg2)

{
	const char *message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	message = reconstruct_message(arg1, arg2);
	if (Wizard (get_effective_id ()) || Apprentice (player))
	{
#ifdef	LOG_WALLS
		fprintf(stderr, "WALL from %s(%d): %s\n", getname_inherited (player), player, message);
#endif	/* LOG_WALLS */
		if(!blank(message))
		{
			notify_all ("%s shouts \"%s\"", getname_inherited (player), message);
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
context::do_broadcast (
const	char	*arg1,
const	char	*arg2)

{
	const char *message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	message = reconstruct_message(arg1, arg2);
	if (Wizard (get_effective_id ()) || Apprentice (player))
	{
#ifdef	LOG_WALLS
		fprintf(stderr, "BROADCAST from %s(%d): %s\n", getname_inherited (player), player, message);
#endif	/* LOG_WALLS */
		if(!blank(message))
		{
			notify_listeners (message);
			return_status = COMMAND_SUCC;
			set_return_string (ok_return_string);
		}
		else
			notify_colour(player, player, COLOUR_MESSAGES,"Now try a message that isn't blank.");
	}
	else
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards and Apprentices may use @broadcast.");
}

void
context::do_at_note (
const	char	*arg1,
const	char	*arg2)

{
	const char *message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	message = reconstruct_message(arg1, arg2);

	if(blank(message))
		notify_colour(player, player, COLOUR_MESSAGES, "Not really worth taking a note of, huh?");
	else
	{
		fprintf(stderr, "NOTE from |%s|(%d) in %s%s(%d)[%s]: %s\n",
			getname_inherited (player),
			player,
			getarticle (db[player].get_location(), ARTICLE_LOWER_INDEFINITE),
			getname_inherited (db[player].get_location ()),
			db[player].get_location(),
			getname_inherited (db[db[player].get_location()].get_owner()),
			message);
		fflush(stderr);
		if (!in_command())
			notify_colour(player, player, COLOUR_MESSAGES, "Your note has been logged.");
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}

void
context::do_gripe (
const	char	*arg1,
const	char	*arg2)

{
	dbref loc;
	const char *message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	loc = db[player].get_location();
	message = reconstruct_message(arg1, arg2);

	if(in_command())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No griping in commands.");
		return;
	}

	if(blank(message))
		notify_colour(player, player, COLOUR_MESSAGES, "Anything in particular, or just generally moaning?");
	else
	{
		sprintf(scratch_buffer, "GRIPE from %s(%d) in %s%s(%d): %s\n",
			getname_inherited (player), player,
			getarticle (loc, ARTICLE_LOWER_INDEFINITE),
			getname_inherited (loc), loc,
			message);
		fputs (scratch_buffer, stderr);
		fflush(stderr);
		notify_colour(player, player, COLOUR_MESSAGES, "Your complaint has been logged.");
		notify_wizard ("%s", scratch_buffer);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_page (
const	char	*part1,
const	char	*part2)

{
	Player_list	targets(player);
	Player_list	all_targets(player);

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (blank(part1))
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

	char *arg1=(part1==NULL)?NULL:strdup(part1);
	char *arg2=(part2==NULL)?NULL:strdup(part2);

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
		free(arg1);
		free(arg2);
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

		const	char *const *colour_at;
		while (target != NOTHING)
		{

			colour_at=db[target].get_colour_at();
			context	unparse_context (target);
			strcpy (scratch_buffer, unparse_objectandarticle_inherited (unparse_context, db[player].get_location(), ARTICLE_LOWER_INDEFINITE));
			strcat (scratch_buffer, ", ");
			strcat (scratch_buffer, colour_at[rank_colour(player)]);
			strcat (scratch_buffer, unparse_object_inherited (unparse_context, player));

			if (howmany > 1)
				notify_censor_colour (target, player, COLOUR_PAGES, "%%w%%hPAGING%%z to %s from %s%s %s%s", targets.generate_courtesy_string(player, target), colour_at[COLOUR_ROOMNAME], scratch_buffer, colour_at[COLOUR_PAGES], boldify(target, suffix));
			else
				notify_censor_colour (target, player, COLOUR_PAGES, "%%w%%hPAGING%%z from %s%s%s %s", colour_at[COLOUR_ROOMNAME], scratch_buffer, colour_at[COLOUR_PAGES], boldify(target, suffix));

			target=targets.get_next();
		}

		free (suffix);
	}

	all_targets.trigger_command(".page", *this);
	if (!in_command())
		notify_colour(player, player, COLOUR_MESSAGES,  "Message sent to %s.", targets.generate_courtesy_string(player, player, True));

	if (Haven (player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,  "WARNING: Your HAVEN flag has been removed because of this page.");
		db[player].clear_flag(FLAG_HAVEN);
	}

	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
	free (arg1);
	free (arg2);

}


void
context::do_natter (
const	char	*arg1,
const	char	*arg2)
{
	const char *message;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (!(Wizard (get_effective_id ()) || Apprentice (player)))
		notify_colour (player, player, COLOUR_ERROR_MESSAGES, "Only Wizards and Apprentices may use the @natter channel.");
	else
	{
		message = reconstruct_message (arg1, arg2);
		if (message[0] == POSE_TOKEN)
			{
			if ((message[1] == '\'') || (message[1] == '`'))
			  notify_wizard_natter ("  %s%s", getname_inherited (player), message+1);
			else
			  notify_wizard_natter ("  %s %s", getname_inherited (player), message+1);
			}

		else
			notify_wizard_natter ("  %s natters \"%s\"", getname_inherited (player), message);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
}


void
context::do_echo (
const	char	*arg1,
const	char	*arg2)

{
	
	notify_censor_colour(player, player, COLOUR_MESSAGES, "%s%s", reconstruct_message(arg1, arg2), COLOUR_REVERT);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
}


void
context::do_at_oecho (
const	char	*arg1,
const	char	*arg2)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);
	if (controls_for_write (db[player].get_location()))
	{
		sprintf (scratch_buffer, "%s", reconstruct_message(arg1, arg2));
		notify_except (db [db [player].get_location()].get_contents(), player, player, scratch_buffer);
		return_status = COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
	else
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must own the room to use @oecho in it.");
}


void
context::do_at_oemote (
const	char	*arg1,
const	char	*arg2)

{

       return_status = COMMAND_FAIL;
       set_return_string (error_return_string);

  
       if(!Silent(player) || Wizard(player) || Apprentice(player))
       {
	sprintf (scratch_buffer, "%s %s", getname_inherited (player), reconstruct_message(arg1, arg2));
	notify_except (db [db [player].get_location()].get_contents(), player, player, scratch_buffer);
	return_status = COMMAND_SUCC;
	set_return_string (ok_return_string);
       }
       else
	   notify_colour(player,  player, COLOUR_ERROR_MESSAGES, "Silent players can't @oemote anything!");
}

void
context::do_pemote (
const	char	*name,
const	char	*what)

{
	dbref	victim;

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (name && *name)
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
			what = value_or_empty(what);
			if ((*what == '\'') || (*what == '`'))
				sprintf(scratch_buffer, "%s%s", getname_inherited (player), what);
			else
				sprintf(scratch_buffer, "%s %s", getname_inherited (player), what);
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
const	char	*names,
const	char	*what)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (blank(names) || blank(what))
	{
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
			notify_censor(victim, player, "%s", what);
		victim=targets.get_next();
	}
	if (!in_command())
		notify(player, "Notified.");
}

void
context::do_fchat (
const	char	*arg1,
const	char	*arg2)

{
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	if (blank(arg1))
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
	const char *output=reconstruct_message(arg1, arg2);
	if (*output == ':')
		sprintf(scratch_buffer, "%%y%%h[FRIENDS] %%w%s%s%s", db[player].get_name(), (output[1] == '\'') ? "" : " ", output+1); 	
	else
		sprintf(scratch_buffer, "%%y%%h[FRIENDS] %%w%s says \"%s\"", db[player].get_name(), output); 	
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
Boolean		speechmarks,
dbref		talker,
int		colour)

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

	const	char *const *colour_at;
	DOLIST (first, first)
		if ((Connected (first) ) && (first != exception))
		{

			colour_at=db[first].get_colour_at();
			if (speechmarks==True)
				notify_public(first, talker, "%s%s\"%s%s%s\"%%z", colour_at[rank_colour(talker)], myprefix, colour_at[colour],  mymsg, colour_at[rank_colour(talker)]);
			else
				notify_public(first, talker, "%s%s%s%s%%z", colour_at[rank_colour(talker)], myprefix, colour_at[colour],  mymsg);
			
		}
	if (docensor)
	{
		free (mymsg);
		free (myprefix);
	}
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

Boolean
really_do_tell(char *arg1, char *arg2, Boolean force_emote, context &c)
{
	Player_list	targets(c.get_player());
	Player_list	all_targets(c.get_player());
	dbref		player= c.get_player();
	int	count;

	if (blank(arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: tell <playerlist> [=] message");
		return;
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
		return False;
	}

	/* You can't tell if you are set silent */

	if(Silent(player) && !Wizard(player) && !Apprentice(player))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are set Silent. You can't 'tell' to people.");
		return False;
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
		return False;
	}

	targets.warn_me_if_idle();

	if (Haven(player))
	{
		db[player].clear_flag(FLAG_HAVEN);
		notify_colour(player, player, COLOUR_MESSAGES, "Your Haven flag has been removed because of this tell.");
	}


	dbref target= targets.get_first();
	const	char *const *colour_at;
	while (target != NOTHING)
	{

		colour_at=db[target].get_colour_at();
		if ((*arg2==':') || (force_emote==True))
			notify_censor (target, player, "%s[To %s] %s %s%s", colour_at[COLOUR_TELLS], targets.generate_courtesy_string(player, target), db[player].get_name(), colour_at[COLOUR_TELLMESSAGES], (force_emote==True)?arg2:arg2+1);
		else
			notify_censor (target, player, "%s[To %s] %s tells you \"%s%s%s\"", colour_at[COLOUR_TELLS], targets.generate_courtesy_string(player, target), db[player].get_name(), colour_at[COLOUR_TELLMESSAGES], arg2, colour_at[COLOUR_TELLS]);
		target= targets.get_next();
	}

	all_targets.trigger_command(".tell", c);

	colour_at=db[player].get_colour_at();
	if ((*arg2==':') || (force_emote==True))
		notify(player,"%s[To %s] %s %s%s",colour_at[COLOUR_TELLS], targets.generate_courtesy_string(player, player, True), db[player].get_name(), colour_at[COLOUR_TELLMESSAGES], (force_emote==True)?arg2:arg2+1);
	else
		notify(player,"%sYou tell %s \"%s%s%s\"",colour_at[COLOUR_TELLS], targets.generate_courtesy_string(player, player, True), colour_at[COLOUR_TELLMESSAGES], arg2, colour_at[COLOUR_TELLS]);

	return True;
}

/*
 * tell (<player> [; <player> [...]] | <listname> | <channel number>) = [:] <message>)
 *
 * List names take precedence (unless there aren't any!).
 *
 */

void context::do_tellemote (const char *part1, const char *part2)
{
	char *arg1;
	char *arg2;

	arg1=(part1==NULL)?NULL:strdup(part1);
	arg2=(part2==NULL)?NULL:strdup(part2);

	if (really_do_tell(arg1, arg2, True, *this)==True)
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

void context::do_tell(const char *part1, const char *part2)
{
	char *arg1;
	char *arg2;

	arg1=(part1==NULL)?NULL:strdup(part1);
	arg2=(part2==NULL)?NULL:strdup(part2);

	if (really_do_tell(arg1, arg2, False, *this)==True)
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

void context::do_at_censor(const char *arg1, const char *arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@censor can only be used by Apprentices or Wizards.");
		return;
	}

	if (blank(arg1))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @censor <word>");
		return;
	}

	const char *p=arg1;
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

	if (add_rude(arg1) == False)
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


void context::do_at_exclude(const char *arg1, const char *arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@exclude can only be used by Apprentices or Wizards.");
		return;
	}

	if (!blank(arg2))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @exclude <word>");
		return;
	}

	const char *p=arg1;
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

	if (add_excluded(arg1) == False)
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


void context::do_at_uncensor(const char *arg1, const char *arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@uncensor can only be used by Apprentices or Wizards.");
		return;
	}

	if (!blank(arg2))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @uncensor <word>");
		return;
	}

	const char *p=arg1;
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

	if (un_rude(arg1) == False)
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


void context::do_at_unexclude(const char *arg1, const char *arg2)
{
	return_status=COMMAND_FAIL;
	set_return_string(error_return_string);

	if (!(Wizard(get_effective_id()) || Apprentice(get_effective_id())))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@unexclude can only be used by Apprentices or Wizards.");
		return;
	}

	if (!blank(arg2))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Syntax: @unexclude <word>");
		return;
	}

	const char *p=arg1;
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

	if (un_exclude(arg1) == False)
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

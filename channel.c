#include <string.h>
#include <ctype.h>
#include <time.h>

#include "db.h"
#include "objects.h"
#include "externs.h"
#include "context.h"
#include "command.h"
#include "interface.h"
#include "channel.h"
#include "colour.h"
#include "context.h"

#define	MAKELONG(a,b,c,d)	((a<<24)|(b<<16)|(c<<8)|d)
#define INVITE_FREQUENCY	(60)
static struct channel *channels=NULL;


static struct channel *
find_channel (
const	char	*name)

{
	for(struct channel *current=channels; current; current=current->next)
		if(strcasecmp(current->name, name)==0)
			return current;
	
	return NULL;
}


static inline Boolean
ok_channel_name (
const	char	*name)

{
	if(blank(name) || *name!=CHANNEL_MAGIC_COOKIE ||
	   strlen(name)<2 || strlen(name)>14)
		return False;

	return True;
}


static struct channel_player *
on_channel (
	struct	channel_player	*list,
const	dbref			player)

{
	for(struct channel_player *current=list; current; current=current->next)
		if(current->player==player)
			return current;
	
	return NULL;
}


static void
delete_channel (
struct	channel	*channel)

{
	struct channel_player *cp, *old;

	if(channel->players)
	{
		Trace( "BUG: delete_channel called with non-empty channel\n");
		return;
	}

	for(cp=channel->invites; cp; )
	{
		old=cp;
		cp=cp->next;
		free(cp);
	}

	for(cp=channel->bans; cp; )
	{
		old=cp;
		cp=cp->next;
		free(cp);
	}

	if(channel->prev)
		channel->prev->next=channel->next;
	if(channel->next)
		channel->next->prev=channel->prev;
	if(channel==channels)
		channels=channels->next;
}


static struct channel_player *add_to_list(struct channel_player **list, dbref player, enum channel_player_flags flags=CHANNEL_PLAYER_NORMAL)
{
	struct channel_player *channel_player=(struct channel_player *) malloc(sizeof(struct channel_player));

	channel_player->player=player;
	channel_player->flags=flags;
	channel_player->next=*list;
	channel_player->prev=NULL;
	time(&(channel_player->issued));
	if(*list)
		(*list)->prev=channel_player;
		
	*list=channel_player;

	return channel_player;
}


static void remove_from_list(struct channel_player **list, dbref player)
{
	for(struct channel_player *current=*list; current; current=current->next)
		if(current->player==player)
		{
			if(current->prev)
				current->prev->next=current->next;
			if(current->next)
				current->next->prev=current->prev;
			if(current==*list)
				*list=(*list)->next;
			
			free(current);
			break;
		}
}


static struct channel *
join_channel (
const	char	*name,
const	dbref	player)
{
	struct	channel	*channel = find_channel(name);
	const colour_at& ca = db[player].get_colour_at();

	if(!channel)
	{
		channel=(struct channel *) malloc(sizeof(struct channel));

		channel->name=strdup(name);
		channel->mode=CHANNEL_PUBLIC;
		channel->players=NULL;
		channel->invites=NULL;
		channel->bans=NULL;
		channel->next=channels;
		channel->prev=NULL;

		if(channels)
			channels->prev=channel;
		
		channels=channel;

		notify(player, "%sNew channel \"%s\" created.%s", ca[COLOUR_ERROR_MESSAGES], name, COLOUR_REVERT);
	}

	if(channel->mode==CHANNEL_PRIVATE && !on_channel(channel->invites, player))
	{
		notify(player, "%s, Sorry, channel \"%s\" is private, and you don't have an invitation.%s", ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT);
		return NULL;
	}

	if(on_channel(channel->bans, player))
	{
		notify(player, "%sSorry, you are banned from joining this channel.%s", ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		return NULL;
	}

	add_to_list(&channel->players, player, channel->players? CHANNEL_PLAYER_NORMAL : CHANNEL_PLAYER_OPERATOR);

	notify(player, "Joined channel \"%s\".", channel->name);

	return channel;
}


static void
send_chat_message (
const	dbref	player,
const	char	*msg,
const	int	system = 0)
{
		static	char	buf[8192];
	const	struct	channel	*channel=db[player].get_channel();

	if(!channel)
		return;

	for (struct channel_player *current=channel->players; current; current=current->next)
	{
		const colour_at& ca = db[current->player].get_colour_at();

		if(*msg==':')
			sprintf(buf, "%s[%s]%s  %s%s %s%s", ca[COLOUR_CHANNEL_NAMES], channel->name, COLOUR_REVERT, ca[COLOUR_CHANNEL_MESSAGES], db[player].get_name().c_str(), msg+1, COLOUR_REVERT);
		else if(system)
			sprintf(buf, "%s[%s]%s  %s[%s]%s", ca[COLOUR_CHANNEL_NAMES], channel->name, COLOUR_REVERT, ca[COLOUR_ERROR_MESSAGES], msg, COLOUR_REVERT);
		else
			sprintf(buf, "%s[%s]%s  %s%s says \"%s\"%s", ca[COLOUR_CHANNEL_NAMES], channel->name, COLOUR_REVERT, ca[COLOUR_CHANNEL_MESSAGES], db[player].get_name().c_str(), msg, COLOUR_REVERT);

		notify_censor(current->player, player, "%s", buf);
	}
}


void context::do_chat(const char *arg1, const char *arg2)
{
	if (Typeof(player) == TYPE_PUPPET)
	{
		notify_colour(db[player].get_owner(), db[player].get_owner(), COLOUR_ERROR_MESSAGES, "NPCs are not allowed to use chatting channels. Your NPC named '%s' just tried to join one.", db[player].get_name().c_str());
		RETURN_FAIL;
	}

	const colour_at& ca = db[get_player()].get_colour_at();
	if(blank(arg1))
	{
		if(!db[player].get_channel())
			notify(player, "%sYou are not on any channels.%s",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		else
		{
			notify(player, "%sDefault channel:%s  %s%s%s", ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT, ca[COLOUR_CHANNEL_NAMES], db[player].get_channel()->name, COLOUR_REVERT);
			sprintf(scratch_buffer, "On channels:  ");

			for(struct channel *current=channels; current; current=current->next)
				if(on_channel(current->players, player))
				{
					strcat(scratch_buffer, current->name);
					strcat(scratch_buffer, ", ");
				}
			
			scratch_buffer[strlen(scratch_buffer)-2]='\0';
			notify(player, "%s", scratch_buffer);
		}

		RETURN_SUCC;
	}

	if(*arg1==CHANNEL_MAGIC_COOKIE)
	{
		/* chat #woo - or - chat #woo=message */

		if(!ok_channel_name(arg1))
		{
			notify(player, "That isn't a valid channel name.");
			RETURN_FAIL;
		}

		struct channel *channel=find_channel(arg1);
		
		if(channel && on_channel(channel->players, player))
		{
			db[player].set_channel(channel);
			if(blank(arg2))
			{
				notify(player, "%sSwitched to channel \"%s\".%s", ca[COLOUR_ERROR_MESSAGES], arg1, COLOUR_REVERT);
				RETURN_SUCC;
			}
		}
		else if((channel=join_channel(arg1, player)))
		{
			db[player].set_channel(channel);
			sprintf(scratch_buffer, "%s has joined the channel", db[player].get_name().c_str());
			send_chat_message(player, scratch_buffer, 1);

			if(blank(arg2))
				RETURN_SUCC;
		}
		else
			RETURN_FAIL;

		/* chat #woo=message */

		send_chat_message(player, arg2);
		RETURN_SUCC;
	}

	/* chat message - or - chat message=message */

	if(db[player].get_channel())
	{
		send_chat_message(player, reconstruct_message(arg1, arg2));
		RETURN_SUCC;
	}

	notify(player, "%sYou are not on any channels.%s", ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);

	RETURN_FAIL;
}

/*
 * Queries on the channnel state.
 */
void context::do_query_channel (const char *const arg1, const char * const arg2){
	const colour_at& ca = db[get_player()].get_colour_at();
	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	dbref victim;
	if ((arg1) && ((strcasecmp(arg1, "primary") == 0) || (strcasecmp(arg1, "all") == 0)))
	{
		if (arg2)
		{
			if((victim = lookup_player(player, arg2)) == NOTHING)
			{
				notify(player, "%sPlayer not found.%s", ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
				RETURN_FAIL;
			}
		}
		else
			victim = player;
	}
	struct channel *interested_channel;
	if ((arg1) && ((strcasecmp(arg1, "members") == 0) || (strcasecmp(arg1, "operators") == 0)))
	{
		if (arg2 != NULL)
			interested_channel = find_channel (arg2);
		else
			interested_channel = db[player].get_channel();
		if (interested_channel == NULL)
		{
			RETURN_FAIL;
		}
	}

	if((arg1) && (strcasecmp(arg1, "primary") == 0))
	{
		struct channel *ch = db[victim].get_channel();
		if(ch)
		{
			return_status = COMMAND_SUCC;
			set_return_string (ch->name);
		}
	}
	else if((arg1) && (strcasecmp(arg1, "all") == 0))
	{
		scratch_buffer[0] = '\0';
		for(struct channel *current=channels; current; current=current->next)
		{
			for(struct channel_player *current_player=current->players; current_player; current_player=current_player->next)
			{
				if (current_player->player == victim)
				{
					if (scratch_buffer[0] != '\0')
						strcat (scratch_buffer, ";");
					strcat (scratch_buffer, current->name);
				}
			}
		}
		return_status = COMMAND_SUCC;
		set_return_string (scratch_buffer);
	}
	else if ((arg1) && (strcasecmp(arg1, "members") == 0))
	{
		scratch_buffer[0] = '\0';
		for(struct channel_player *current_player=interested_channel->players; current_player; current_player=current_player->next)
		{
			if (scratch_buffer[0] != '\0')
				strcat (scratch_buffer, ";");
			strcat (scratch_buffer, db[current_player->player].get_name().c_str());
		}
		return_status = COMMAND_SUCC;
		set_return_string (scratch_buffer);
	}
	else if ((arg1) && (strcasecmp(arg1, "operators") == 0))
	{
		scratch_buffer[0] = '\0';
		for(struct channel_player *current_player=interested_channel->players; current_player; current_player=current_player->next)
		{
			if (current_player->flags==CHANNEL_PLAYER_OPERATOR)
			{
				if (scratch_buffer[0] != '\0')
					strcat (scratch_buffer, ";");
				strcat (scratch_buffer, db[current_player->player].get_name().c_str());
			}
		}
		return_status = COMMAND_SUCC;
		set_return_string (scratch_buffer);
	}
	else
	{
		notify(player, "%sThat is not a valid @?channel option%s", ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
	}
}

void
context::do_channel (
const	char	*arg1,
const	char	*arg2)
{
		dbref			victim;
	const colour_at&	ca = db[get_player()].get_colour_at();

	if(blank(arg1))
	{
		notify(player, "%sUsage:  @channel opadd = <player>%s",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		notify(player, "        @channel opdel = <player>");
		notify(player, "        @channel status");
		notify(player, "        @channel who");
		notify(player, "        @channel mode = public / private");
		notify(player, "        @channel invite = <player>");
		notify(player, "        @channel uninvite = <player>");
		notify(player, "        @channel boot = <player>");
		notify(player, "        @channel ban = <player>");
		notify(player, "        @channel unban = <player>");
		notify(player, "        @channel list");
		notify(player, "        @channel rename = <name>");
		notify(player, "        @channel leave [ = #channel (or all)");
		notify(player, "\nAll commands (except list) apply to your current channel. ");
		notify(player, "Type \"help @channel\" for more information.%s",COLOUR_REVERT );

		RETURN_FAIL;
	}

	if(strcasecmp(arg1, "list")==0)
	{
		if(!channels)
			notify(player, "%sThere are no channels in use.%s",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		else
		{
			int total=0;

			for(struct channel *current=channels; current; current=current->next, total++)
			{
				sprintf(scratch_buffer, "%-15.15s%s  ", current->name, current->mode==CHANNEL_PRIVATE? "P":"");
				for(struct channel_player *current_player=current->players; current_player; current_player=current_player->next)
				{
					if(current_player->flags==CHANNEL_PLAYER_OPERATOR)
						strcat(scratch_buffer, "@");
					strcat(scratch_buffer, db[current_player->player].get_name().c_str());
					strcat(scratch_buffer, ", ");
				}
			
				scratch_buffer[strlen(scratch_buffer)-2]='\0';
				notify(player, "%s", scratch_buffer);
			}

			notify(player, "%s%d channel%s listed.%s",ca[COLOUR_ERROR_MESSAGES], total, PLURAL(total), COLOUR_REVERT);
		}

		RETURN_SUCC;
	}

	struct channel *channel=db[player].get_channel();

	if(!channel)
	{
		notify(player, "%sYou must belong to a channel to use this command.%s",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		RETURN_FAIL;
	}

	struct channel_player *channel_player=on_channel(channel->players, player);

	if(!channel_player)
	{
		notify(player, "%sBug in code - player missing from default channel.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
		Trace( "BUG: player missing from default channel\n");
		RETURN_FAIL;
	}

	if (string_prefix("opadd", arg1))
	{
		dbref victim;
		struct channel_player *vc;

		if(blank(arg2))
		{
			notify(player, "%sYou must specify someone to add as an operator.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard(get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel %s.%s", ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT );
			RETURN_FAIL;
		}

		if((victim=lookup_player(player, arg2))==NOTHING)
		{
			notify(player, "%sNo such player \"%s\".%s", ca[COLOUR_ERROR_MESSAGES], arg2, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!(vc=on_channel(channel->players, victim)))
		{
			notify(player, "%s%s is not on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(vc->flags==CHANNEL_PLAYER_OPERATOR)
		{
			notify(player, "%s%s is already an operator on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		vc->flags=CHANNEL_PLAYER_OPERATOR;

		// add himself etc here one day.
		if (player != victim)
			sprintf(scratch_buffer, "%s makes %s an operator", db[player].get_name().c_str(), db[victim].get_name().c_str());
		else
			sprintf(scratch_buffer, "%s gets a promotion to operator", db[player].get_name().c_str());

		send_chat_message(player, scratch_buffer, 1);

		RETURN_SUCC;
	}


	if(string_prefix("opdel", arg1))
	{
		struct channel_player *vc;

		if(blank(arg2))
		{
			notify(player, "%sYou must specify someone to remove as an operator.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard(get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel \"%s\".%s", ca[COLOUR_ERROR_MESSAGES],channel->name,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((victim=lookup_player(player, arg2))==NOTHING)
		{
			notify(player, "%sNo such player \"%s\".%s",ca[COLOUR_ERROR_MESSAGES], arg2,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!(vc=on_channel(channel->players, victim)))
		{
			notify(player, "%s%s is not on channel %s.%s", ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(vc->flags!=CHANNEL_PLAYER_OPERATOR)
		{
			notify(player, "%s%s is not an operator on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name,COLOUR_REVERT);
			RETURN_FAIL;
		}

		vc->flags=CHANNEL_PLAYER_NORMAL;

		if (player != victim)
			sprintf(scratch_buffer, "%s removes %s as an operator", db[player].get_name().c_str(), db[victim].get_name().c_str());
		else
			/* resigns from being an operator */
			sprintf(scratch_buffer, "%s resigns from being an operator", db[player].get_name().c_str());
		send_chat_message(player, scratch_buffer, 1);

		RETURN_SUCC;
	 
	}

	if(string_prefix("status", arg1))
	{
		notify(player, "%sChannel %s is %s.%s",ca[COLOUR_ERROR_MESSAGES], channel->name, channel->mode==CHANNEL_PUBLIC? "public":"private",COLOUR_REVERT);

		if(channel->invites)
		{
			sprintf(scratch_buffer, "Channel %s outstanding invites:  ", channel->name);
			for(struct channel_player *cp=channel->invites; cp; cp=cp->next)
			{
				strcat(scratch_buffer, db[cp->player].get_name().c_str());
				if(cp->next)
					strcat(scratch_buffer, ", ");
			}

			notify(player, "%s%s%s", ca[COLOUR_ERROR_MESSAGES],scratch_buffer,COLOUR_REVERT);
		}

		if(channel->bans)
		{
			sprintf(scratch_buffer, "Channel %s banned players:  ", channel->name);
			for(struct channel_player *cp=channel->bans; cp; cp=cp->next)
			{
				strcat(scratch_buffer, db[cp->player].get_name().c_str());
				if(cp->next)
					strcat(scratch_buffer, ", ");
			}

			notify(player, "%s%s%s", ca[COLOUR_ERROR_MESSAGES],scratch_buffer,COLOUR_REVERT);
		}
	}

	if (string_prefix("who", arg1) || string_prefix("status", arg1))
	{
		sprintf(scratch_buffer, "Channel %s players:  ", channel->name);
		for(struct channel_player *cp=channel->players; cp; cp=cp->next)
		{
			if(cp->flags==CHANNEL_PLAYER_OPERATOR)
				strcat(scratch_buffer, "@");
			strcat(scratch_buffer, db[cp->player].get_name().c_str());
			if(cp->next)
				strcat(scratch_buffer, ", ");
		}
		notify(player, "%s%s%s", ca[COLOUR_ERROR_MESSAGES],scratch_buffer,COLOUR_REVERT);
			RETURN_SUCC;
	}

	if (string_prefix("mode", arg1))
	{
		if(blank(arg2))
		{
			notify(player, "You must specify whether to make the channel public or private.");
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard (get_effective_id())))
		{
			notify(player, "You are not an operator on channel %s.", channel->name);
			RETURN_FAIL;
		}

		if(strncasecmp(arg2, "pub", 3)==0)
		{
			if(channel->mode==CHANNEL_PUBLIC)
			{
				notify(player, "Channel %s is already public.", channel->name);
				RETURN_FAIL;
			}

			channel->mode=CHANNEL_PUBLIC;
			sprintf(scratch_buffer, "%s sets the channel public", db[player].get_name().c_str());
			send_chat_message(player, scratch_buffer, 1);
			RETURN_SUCC;
		}

		if(strncasecmp(arg2, "pri", 3)==0)
		{
			if(channel->mode==CHANNEL_PRIVATE)
			{
				notify(player, "Channel %s is already private.", channel->name);
				RETURN_FAIL;
			}

			channel->mode=CHANNEL_PRIVATE;
			sprintf(scratch_buffer, "%s sets the channel private", db[player].get_name().c_str());
			send_chat_message(player, scratch_buffer, 1);
			RETURN_SUCC;
		}

		notify(player, "%sYou must specify either \"private\" or \"public\".%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
		RETURN_FAIL;
	}

	if (string_prefix("uninvite", arg1) || string_prefix("invite", arg1))
	{
		if(blank(arg2))
		{
			notify(player, "%sYou must specify a player to invite.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((victim=lookup_player(player, arg2))==NOTHING)
		{
			notify(player, "%sNo such player \"%s\".%s", ca[COLOUR_ERROR_MESSAGES],arg2,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard (get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel %s.%s", ca[COLOUR_ERROR_MESSAGES],channel->name,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(on_channel(channel->players, victim))
		{
			notify(player, "%s%s is already on channel %s.%s", ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(on_channel(channel->bans, victim))
		{
			notify(player, "%s%s is banned from channel %s.%s", ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!Connected(victim))
		{
			notify(player, "%s%s is not connected.%s", ca[COLOUR_ERROR_MESSAGES],db[victim].get_name().c_str(), COLOUR_REVERT);
			RETURN_FAIL;
		}

		if (string_prefix("uninvite", arg1))
		{
			if(on_channel(channel->invites, victim))
			{
				remove_from_list(&channel->invites, victim);
				notify(victim, "%s[%s withdraws your invitation to channel %s]%s",ca[COLOUR_ERROR_MESSAGES], db[player].get_name().c_str(), channel->name,COLOUR_REVERT);
				sprintf(scratch_buffer, "%s withdraws %s's invitation", db[player].get_name().c_str(), db[victim].get_name().c_str());
				send_chat_message(player, scratch_buffer, 1);
			}
				else
					notify(player, "%sThat player is not currently invited.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
		}
		else	// must be 'invite'
		{
			time_t now;
			struct channel_player *which;
			time(&now);
			if((which= on_channel(channel->invites, victim)) && (now-which->issued < INVITE_FREQUENCY))
				notify(player, "%sThat player is already invited (you can re-send the invite in %s).%s",ca[COLOUR_ERROR_MESSAGES],time_string(INVITE_FREQUENCY - now + which->issued), COLOUR_REVERT);
			else
			{
				if (which)
				{
					time(&(which->issued));
					notify(victim, "%s[%s re-invites you to channel %s (type \"chat %s\" to join)]%s", ca[COLOUR_ERROR_MESSAGES], db[player].get_name().c_str(), channel->name, channel->name,COLOUR_REVERT);
				sprintf(scratch_buffer, "%s re-invites %s to the channel", db[player].get_name().c_str(), db[victim].get_name().c_str());
				}
				else
				{
					add_to_list(&channel->invites, victim);
					notify(victim, "%s[%s invites you to channel %s (type \"chat %s\" to join)]%s", ca[COLOUR_ERROR_MESSAGES], db[player].get_name().c_str(), channel->name, channel->name,COLOUR_REVERT);
				sprintf(scratch_buffer, "%s invites %s to the channel", db[player].get_name().c_str(), db[victim].get_name().c_str());
				}
				send_chat_message(player, scratch_buffer, 1);
			}
		}
		RETURN_SUCC;
	}

	if (string_prefix("boot", arg1))
	{
		if(blank(arg2))
		{
			notify(player, "%sYou must specify who to boot from the channel.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((victim=lookup_player(player, arg2))==NOTHING)
		{
			notify(player, "%sNo such player \"%s\".%s",ca[COLOUR_ERROR_MESSAGES], arg2,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard (get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!on_channel(channel->players, victim))
		{
			notify(player, "%s%s is not on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name,COLOUR_REVERT);
			RETURN_FAIL;
		}

		remove_from_list(&channel->players, victim);
		notify(victim, "%s[%s has booted you from channel %s]%s", ca[COLOUR_ERROR_MESSAGES], db[player].get_name().c_str(), channel->name, COLOUR_REVERT);
		db[victim].set_channel(NULL);
		if(channel->players)
		{
			sprintf(scratch_buffer, "%s has booted %s from the channel", db[player].get_name().c_str(), db[victim].get_name().c_str());
			send_chat_message(player, scratch_buffer, 1);
		}
		else
			delete_channel(channel);
			
		RETURN_SUCC;
	}
	if (string_prefix("ban", arg1))
	{
		if(blank(arg2))
		{
			notify(player, "%sYou must specify a player to ban.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((victim=lookup_player(player, arg2))==NOTHING)
		{
			notify(player, "%sNo such player \"%s\".%s", ca[COLOUR_ERROR_MESSAGES], arg2, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard (get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel %s.%s", ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(on_channel(channel->bans, victim))
		{
			notify(player, "%s%s is already banned from channel %s.%s",ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		notify(victim, "%s[%s has banned you from channel %s]%s", ca[COLOUR_CHANNEL_MESSAGES], db[player].get_name().c_str(), channel->name, COLOUR_REVERT);
		if(on_channel(channel->players, victim))
		{
			remove_from_list(&channel->players, victim);
			db[victim].set_channel(NULL);
		}
			
		add_to_list(&channel->bans, victim);
		sprintf(scratch_buffer, "%s has banned %s from the channel.", db[player].get_name().c_str(), db[victim].get_name().c_str());
		send_chat_message(player, scratch_buffer, 1);

		RETURN_SUCC;
	}
		
	if (string_prefix("unban", arg1))
	{
		if(blank(arg2))
		{
			notify(player, "%sYou must specify a player to unban.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((victim=lookup_player(player, arg2))==NOTHING)
		{
			notify(player, "%sNo such player \"%s\".%s",ca[COLOUR_ERROR_MESSAGES], arg2, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard (get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel %s.%s", ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!on_channel(channel->bans, victim))
		{
			notify(player, "%s%s is not banned from channel %s.%s", ca[COLOUR_ERROR_MESSAGES], db[victim].get_name().c_str(), channel->name, COLOUR_REVERT);
			RETURN_FAIL;
		}

		notify(victim, "%s[%s has removed the ban on you from channel %s]%s", ca[COLOUR_ERROR_MESSAGES], db[player].get_name().c_str(), channel->name, COLOUR_REVERT);
			
		remove_from_list(&channel->bans, victim);
		sprintf(scratch_buffer, "%s has removed the ban on %s", db[player].get_name().c_str(), db[victim].get_name().c_str());
		send_chat_message(player, scratch_buffer, 1);

		RETURN_SUCC;
	}

	if ((string_prefix("rename", arg1)) || (string_prefix("name", arg1)))
	{
		if(blank(arg2))
		{
			notify(player, "%sYou must give a new name for the channel.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		if((channel_player->flags!=CHANNEL_PLAYER_OPERATOR) && (!Wizard (get_effective_id())))
		{
			notify(player, "%sYou are not an operator on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], channel->name,COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!ok_channel_name(arg2))
		{
			notify(player, "%sThat isn't a valid channel name.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		// Check the channel is not already in use.
		if (find_channel (arg2))
		{
			notify(player, "%sThat channel name is already taken.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
			RETURN_FAIL;
		}

		sprintf(scratch_buffer, "%s has changed the channel name to \"%s\"", db[player].get_name().c_str(), arg2);
		send_chat_message(player, scratch_buffer, 1);

		free(channel->name);
		channel->name=strdup(arg2);

		RETURN_SUCC;
	}

	if (string_prefix("leave", arg1))
	{
		if(!blank(arg2))
		{
			if(strcasecmp(arg2, "all")==0)
			{
				notify(player, "%sYou are no longer on any channels.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
				channel_disconnect(player);
				RETURN_SUCC;
			}

			if(!ok_channel_name(arg2))
			{
				notify(player, "%sThat isn't a valid channel name.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
				RETURN_FAIL;
			}

			if(!(channel=find_channel(arg2)))
			{
				notify(player, "%sNo such channel.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
				RETURN_FAIL;
			}

			if(!on_channel(channel->players, player))
			{
				notify(player, "%sYou're not on channel %s.%s",ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT);
				RETURN_FAIL;
			}
		}
		notify(player, "%sYou have left channel %s.%s",ca[COLOUR_ERROR_MESSAGES], channel->name, COLOUR_REVERT);
		db[player].set_channel(NULL);
		remove_from_list(&channel->players, player);
		if(channel->players)
		{
			sprintf(scratch_buffer, "%s has left the channel.", db[player].get_name().c_str());
			send_chat_message(player, scratch_buffer, 1);
		}
		else
			delete_channel(channel);

		RETURN_SUCC;
	}

	notify(player, "%sUnknown @channel command.  Type @channel on its own for usage information.%s",ca[COLOUR_ERROR_MESSAGES],COLOUR_REVERT);
}


void channel_disconnect(dbref player)
{
	for(struct channel *current=channels; current; )
		if(on_channel(current->players, player))
		{
			remove_from_list(&current->players, player);
			if(current->players)
			{
				db[player].set_channel(current);
				sprintf(scratch_buffer, "%s has disconnected", db[player].get_name().c_str());
				send_chat_message(player, scratch_buffer, 1);
			}
			else
			{
				struct channel *old=current;
				current=current->next;
				delete_channel(old);
			}
		}
		else
			current=current->next;
	
	db[player].set_channel(NULL);
}

static char *chop_string(const char *string, int size)
{
	static char whostring[256];
	char *p=whostring;
	const char *q=string;
	int visible=0,
	    copied=0;
	int percent_primed =0;
	while (q && *q && (visible < size) && (copied < 255))
	{
		if (*q == '%')
		{
			if (percent_primed)
			{
				visible++;
				percent_primed=0;
			}
			else
				percent_primed=1;
		}
		else
		{
			if (percent_primed)
				percent_primed=0;
			else
				visible++;
		}
		*p++=*q++;
		copied++;
	}


	while ((visible++ < size) && (copied < 255))
	{
		copied++;
		*p++=' ';
	}
	*p='\0';
	return whostring;
}


void
context::do_channel_who
(
 const	char	*name,
 const	char	*
)
{
	int	channel_count = 0;
	time_t	interval;
	struct	channel *channel=db[player].get_channel();
	const colour_at&	ca = db[get_player()].get_colour_at();
	char	buf[200];

	/*
	 * They'd better be on a channel - otherwise we don't have much to offer.
	 */
	if(!db[player].get_channel())
	{
		notify(player, "%sYou are not on any channels.%s",ca[COLOUR_ERROR_MESSAGES], COLOUR_REVERT);
		return;
	}


	/*
	 * Print out the header: Players on channel: <name> [(Private)]   Idle
	 */
	sprintf(buf, "%s%s %s%s",
			ca[COLOUR_TITLES],
			channel->name,
			ca[COLOUR_ERROR_MESSAGES],
			channel->mode==CHANNEL_PRIVATE ? "(Private)" : "");

	notify(player, "%sPlayers on channel: %-30s                                    %sIdle%s",
			ca[COLOUR_MESSAGES],
			buf,
			ca[COLOUR_MESSAGES],
			COLOUR_REVERT);


	/*
	 * Process each player on the channel
	 */
	for(struct channel_player *cp=channel->players; cp; cp=cp->next)
	{

		int	firstchar;
		int	extra_space=0;

		/*
		 * if the are an operator, say so, otherwise put the right number of blanks there
		 */
		sprintf(scratch_buffer, "%s%8s %s%s",
				ca[COLOUR_WIZARDS],
				cp->flags==CHANNEL_PLAYER_OPERATOR ? "Operator" : "",
				ca[COLOUR_WHOSTRINGS],
				db[cp->player].get_name().c_str());

		interval = get_idle_time (cp->player);

		/*
		 * If who-string starts with 'special' character, don't put a space in
		 * (just like normal who)
		 */
		if (db[cp->player].get_who_string())
                {
			firstchar = db[cp->player].get_who_string().c_str()[0];

			if (!(firstchar == ' '
			      || firstchar == ','
			      || firstchar == ';'
			      || firstchar == ':'
			      || firstchar == '.'
			      || firstchar == '\''))
                              {
				strcat (scratch_buffer, " ");
				extra_space=1;
			      }

			strcat(scratch_buffer, chop_string(db[cp->player].get_who_string ().c_str(), 60 - extra_space - strlen(db[cp->player].get_name().c_str())));
		}
		else
		{
			strcat(scratch_buffer, chop_string("", 60 - extra_space - strlen(db[cp->player].get_name().c_str())));
		}

		strcat(scratch_buffer, ca[COLOUR_WHOSTRINGS]);

		/*
		 * Say how idle they are - perhaps it should be a constant for Dragor!! ;-)
		 */
		if (interval)
		{
			if (interval < 5)
				sprintf (buf, "   Active");
			else if (interval < 60)
				sprintf (buf, "      %02lds", (long int)interval);
			else if (interval < 60 * 60)
				sprintf (buf, "  %2ldm %02lds", (long int)interval / 60, (long int)interval % 60);
			else if (interval < 90 * 60)
				sprintf (buf, "    Aeons");
			else if (interval < 120 * 60)
				sprintf (buf, "  Eoghans");
			else if (interval < 150 * 60)
				sprintf (buf, "   IntMax");
			else if (interval < 180 * 60)
				sprintf (buf, "  Forever");
			else
				sprintf (buf, "   Always");
		}
		else
			sprintf (buf, "   Active");

		strcat (scratch_buffer, buf);
		

		notify(player, "%s%s", scratch_buffer, COLOUR_REVERT);
		channel_count++;
	}

	notify(player, "%sPlayers: %s%i%s", ca[COLOUR_MESSAGES], ca[COLOUR_TITLES], channel_count, COLOUR_REVERT);
}


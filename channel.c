#include "copyright.h"
/*
 * channel.c (or will be one day)
 *
 * A complete re-write of the old channel code, because it was a mess.
 *
 * This time, we'll make it object-oriented.
 */

#include "config.h"

#include <string.h>
#include <ctype.h>
#include <time.h>

#include "db.h"
#include "externs.h"
#include "interface.h"
#include "colour.h"
#include "context.h"

class ChannelPlayer
{
	dbref			Player;
	bool			Controller;
	time_t			Timestamp;

	ChannelPlayer*		Next;
	ChannelPlayer**		PPrev;

	ChannelPlayer();
	ChannelPlayer& operator=(const ChannelPlayer&);
	ChannelPlayer(const ChannelPlayer&);
public:
	ChannelPlayer(dbref p, ChannelPlayer** pprev)
	{
		Player = p;
		Controller = false;
		Timestamp = 0;
		Next = *pprev;
		if(Next)
		{
			Next->PPrev = &Next;
		}
		*pprev = this;
		PPrev = pprev;
	}
	~ChannelPlayer()
	{
		*PPrev = Next;
		if(Next)
		{
			Next->PPrev = PPrev;
		}
	}

	ChannelPlayer*	OnList(dbref p)
	{
		if(Player == p)
		{
			return this;
		}
		if(Next)
		{
			return Next->OnList(p);
		}
		return 0;
	}
	dbref			player()	{ return Player; }
	bool			controller()	{ return Controller; }
	void			set_controller(bool con) { Controller = con; }
	time_t			timestamp()	{ return Timestamp; }
	void			set_timestamp(time_t t) { Timestamp = t; }
	ChannelPlayer*		next()		{ return Next; }
};

class Channel
{
	String		Name;
	bool		Private;
	bool		Censored;

	ChannelPlayer*	Players;
	ChannelPlayer*	Invites;
	ChannelPlayer*	Bans;

	Channel*	Next;
	Channel**	PPrev;

	static Channel*	Head;

	bool	remove_player_fromlist(dbref p, ChannelPlayer* list);
public:
	Channel(const CString& n);
	~Channel();
	static Channel*	head() { return Head; }
	static bool	ok_name(const CString& n)
	{
		if((n.length() < 2) || (n.length() > 14))
			return false;
		if(n.c_str()[0] != CHANNEL_MAGIC_COOKIE)
			return false;
		return true;
	}
	static Channel* find(const CString& n)
	{
		if(!ok_name(n))
		{
			return 0;
		}
		for(Channel* iter = head(); iter != 0; iter = iter->next())
		{
			if(string_compare(n, iter->name()) == 0)
			{
				return iter;
			}
		}
		return 0;
	}
		
	const String&	name() { return Name; }
	void		set_name(const CString& n) { Name = n; }
	bool	get_private() { return Private; }
	void	set_private(bool s) { Private = s; }
	bool	get_censored() { return Censored; }
	void	set_censored(bool s) {Censored = s; }

	bool	add_invite(dbref p);
	bool	remove_invite(dbref p);
	bool	add_ban(dbref p);
	bool	remove_ban(dbref p);
	bool	add_player(dbref p);
	bool	remove_player(dbref p);
	bool	remove_player_and_send(dbref p, dbref op, const CString& msg)
	{
		if(!find_player(p))
		{
			return false;
		}
		send(op, msg, true);
		remove_player(p);
		if(!Players)
		{
			delete this;
		}
		return true;
	}
	bool	player_banned(dbref p);
	bool	player_invited(dbref p);
	bool	player_connected(dbref p);

	ChannelPlayer*	find_player(dbref p) { if(Players) return Players->OnList(p); return 0; }
	ChannelPlayer*	find_invite(dbref p) { if(Invites) return Invites->OnList(p); return 0; }
	ChannelPlayer*	find_ban(dbref p) { if(Bans) return Bans->OnList(p); return 0; }
	ChannelPlayer*	players() { return Players; }
	ChannelPlayer*	invites() { return Invites; }
	ChannelPlayer*	bans() { return Bans; }
	Channel*	next() { return Next; }

	void	send(dbref player, const CString& msg, bool system = false);

	static Channel*	find_and_join_player(dbref player, const CString& cname, bool can_force);
	bool		join(dbref player, bool can_force);
};

Channel* Channel::Head = 0;

Channel::Channel(const CString& n)
{
	Name = n;
	Censored = false;
	Private = false;
	Players = 0;
	Invites = 0;
	Bans = 0;

	Next = Head;
	Head = this;
	if(Next)
	{
		Next->PPrev = &Next;
	}
	PPrev = &Head;
}

Channel::~Channel()
{
	*PPrev = Next;
	if(Next)
	{
		Next->PPrev = PPrev;
	}
	if(Players)
	{
		Trace("BUG: Channel destroyed whilst players connected to it");
		while(Players)
		{
			delete Players;
		}
	}
	while(Bans)
	{
		delete Bans;
	}
	while(Invites)
	{
		delete Invites;
	}
}

bool
Channel::remove_player_fromlist(dbref p, ChannelPlayer* list)
{
	for(; list != NULL; list = list->next())
	{
		if(list->player() == p)
		{
			delete list;
			return true;
		}
	}
	return false;
}

// Add a player to the channel, no matter if they're banned or !invited
bool
Channel::add_player(dbref p)
{
	if(!player_connected(p))
	{
		new ChannelPlayer(p, &Players);
	}
	return true;
}

bool
Channel::add_invite(dbref p)
{
	if(player_banned(p))
	{
		remove_ban(p);
	}
	if(!player_invited(p))
	{
		new ChannelPlayer(p, &Invites);
	}
	return true;
}

bool
Channel::add_ban(dbref p)
{
	if(player_invited(p))
	{
		remove_invite(p);
	}
	if(!player_banned(p))
	{
		new ChannelPlayer(p, &Bans);
	}
	return true;
}

bool
Channel::remove_player(dbref p)
{
	return remove_player_fromlist(p, Players);
}

bool
Channel::remove_invite(dbref p)
{
	return remove_player_fromlist(p, Invites);
}

bool
Channel::remove_ban(dbref p)
{
	return remove_player_fromlist(p, Bans);
}

bool
Channel::player_connected(dbref p)
{
	if(Players)
	{
		return Players->OnList(p);
	}
	return false;
}

bool
Channel::player_invited(dbref p)
{
	if(Invites)
	{
		return Invites->OnList(p);
	}
	return false;
}

bool
Channel::player_banned(dbref p)
{
	if(Bans)
	{
		return Bans->OnList(p);
	}
	return false;
}

void
Channel::send(dbref player, const CString& msg, bool system)
{
ChannelPlayer* iter;
bool emote = false;
const char* msgstart = msg.c_str();

	if(msgstart[0] == ':')
	{
		emote = true;
	}
	if(get_censored())
	{
		msgstart = censor(msg);
	}

	for(iter = Players; iter != 0; iter = iter->next())
	{
		const colour_at& ca = db[iter->player()].get_colour_at();
		if(system)
		{
			notify_censor(iter->player(), player, "%s[%s]%s  %s%s%s", ca[COLOUR_CHANNEL_NAMES], name().c_str(), COLOUR_REVERT, ca[COLOUR_CHANNEL_MESSAGES], msgstart, COLOUR_REVERT);
		}
		else if(emote)
		{
			notify_censor(iter->player(), player, "%s[%s]%s  %s%s %s%s", ca[COLOUR_CHANNEL_NAMES], name().c_str(), COLOUR_REVERT, ca[COLOUR_CHANNEL_MESSAGES], db[player].get_name().c_str(), msgstart+1, COLOUR_REVERT);
		}
		else
		{
			notify_censor(iter->player(), player, "%s[%s]%s  %s%s says \"%s\"%s", ca[COLOUR_CHANNEL_NAMES], name().c_str(), COLOUR_REVERT, ca[COLOUR_CHANNEL_MESSAGES], db[player].get_name().c_str(), msgstart, COLOUR_REVERT);
		}
	}
}

Channel*
Channel::find_and_join_player(dbref player, const CString& cname, bool can_force)
{
	Channel* channel = 0;
	bool created = false;
	channel = find(cname);
	if(!channel)
	{
		if(ok_name(cname))
		{
			channel = new Channel(cname);
			created = true;
		}
	}

	if(!channel)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unable to create channel %s", cname.c_str());
		return 0;
	}
	if(channel->join(player, can_force))
	{
		if(created)
		{
			channel->find_player(player)->set_controller(true);
		}
		return channel;
	}
	return 0;
}

bool
Channel::join(dbref player, bool can_force)
{
bool forced = false;
	if(player_connected(player))
	{
		return true;
	}
	else if(player_banned(player))
	{
		if(!can_force)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You have been banned from channel %s.", name().c_str());
			return false;
		}
		forced = true;
	}
	else if(get_private() && !player_invited(player))
	{
		if(!can_force)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You haven't been invited to the private channel %s", name().c_str());
			return false;
		}
		forced = true;
	}
	add_player(player);
	if(forced)
	{
		sprintf(scratch_buffer, "%s has overridden permission and joined the channel", db[player].get_name().c_str());
	}
	else
	{
		sprintf(scratch_buffer, "%s has joined the channel", db[player].get_name().c_str());
	}
	send(player, scratch_buffer, true);
	return true;
}

void
context::do_chat(const CString& arg1, const CString& arg2)
{
	if(Typeof(player) != TYPE_PLAYER)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "NPCs aren't allowed to join channels.");
		RETURN_FAIL;
	}

	if(!arg1)
	{
		if(db[player].get_channel() == 0)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You aren't on any channels");
		}
		else
		{
			notify_colour(player, player, COLOUR_CHANNEL_NAMES, "Default channel: %s", db[player].get_channel()->name().c_str());
			sprintf(scratch_buffer, "On channels: ");
			for(Channel* iter = Channel::head(); iter != NULL; iter=iter->next())
			{
				if(iter->player_connected(player))
				{
					strcat(scratch_buffer, iter->name().c_str());
					strcat(scratch_buffer, ", ");
				}
			}
			scratch_buffer[strlen(scratch_buffer)-2] = 0;
			notify_colour(player, player, COLOUR_CHANNEL_NAMES, "%s", scratch_buffer);
		}
		RETURN_SUCC;
	}

	if(arg1.c_str()[0] == CHANNEL_MAGIC_COOKIE)
	{
		bool forced = false;
		if(Wizard(get_effective_id()) && (string_compare(arg2, "force") == 0))
		{
			forced = true;
		}
		Channel* channel = Channel::find_and_join_player(player, arg1, forced);
		if(!channel)
		{
			RETURN_FAIL;
		}
		db[player].set_channel(channel);
		if(arg2 && !forced)
			channel->send(player, arg2);
		RETURN_SUCC;
	}
	if(!db[player].get_channel())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You aren't on a channel");
		RETURN_FAIL;
	}
	db[player].get_channel()->send(player, reconstruct_message(arg1, arg2));
	RETURN_SUCC;
}

/*
 * Queries on the channnel state.
 */
void
context::do_query_channel (const CString& arg1, const CString& arg2)
{

	return_status = COMMAND_FAIL;
	set_return_string (error_return_string);

	enum { Unknown, Primary, All, Members, Operators } query = Unknown;

	if(!arg1)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES,
			"Usage:  @?channel <command> [ = <victim> ]\n"
			"	Commands: primary, all, members, operators");
		RETURN_FAIL;
	}

	if(string_compare(arg1, "primary") == 0)
	{
		query = Primary;
	}
	else if(string_compare(arg1, "all") == 0)
	{
		query = All;
	}
	else if(string_compare(arg1, "members") == 0)
	{
		query = Members;
	}
	else
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Unknown @?channel option '%s'", arg1.c_str());
		RETURN_FAIL;
	}

	dbref victim = player;

	if((Primary == query) || (All == query))
	{
		if (arg2)
		{
			if((victim = lookup_player(player, arg2.c_str())) == NOTHING)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Player '%s' not found", arg2.c_str());
				RETURN_FAIL;
			}
		}
	}

	Channel *interested_channel = 0;

	if((Members == query) || (Operators == query))
	{
		if (arg2)
			interested_channel = Channel::find (arg2);
		else
			interested_channel = db[player].get_channel();
		if (interested_channel == NULL)
		{
			RETURN_FAIL;
		}
	}

	if(Primary == query)
	{
		Channel *ch = db[victim].get_channel();
		if(ch)
		{
			return_status = COMMAND_SUCC;
			set_return_string (ch->name().c_str());
		}
	}
	else if(All == query)
	{
		scratch_buffer[0] = '\0';
		for(Channel *current=Channel::head(); current; current=current->next())
		{
			for(ChannelPlayer *current_player=current->players(); current_player; current_player=current_player->next())
			{
				if (current_player->player() == victim)
				{
					if (scratch_buffer[0] != '\0')
						strcat (scratch_buffer, ";");
					strcat (scratch_buffer, current->name().c_str());
				}
			}
		}
		return_status = COMMAND_SUCC;
		set_return_string (scratch_buffer);
	}
	else if(Members == query)
	{
		scratch_buffer[0] = '\0';
		for(ChannelPlayer *current_player=interested_channel->players(); current_player; current_player=current_player->next())
		{
			if (scratch_buffer[0] != '\0')
				strcat (scratch_buffer, ";");
			strcat (scratch_buffer, db[current_player->player()].get_name().c_str());
		}
		return_status = COMMAND_SUCC;
		set_return_string (scratch_buffer);
	}
	else if(Operators == query)
	{
		scratch_buffer[0] = '\0';
		for(ChannelPlayer *current_player=interested_channel->players(); current_player; current_player=current_player->next())
		{
			if (current_player->controller())
			{
				if (scratch_buffer[0] != '\0')
					strcat (scratch_buffer, ";");
				strcat (scratch_buffer, db[current_player->player()].get_name().c_str());
			}
		}
		return_status = COMMAND_SUCC;
		set_return_string (scratch_buffer);
	}
	else
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Something went wrong in @?channel.");
		Trace("BUG: Something went wrong in @?channel");
	}
}

void
context::do_channel (const CString& arg1, const CString& arg2)
{
	if(Typeof(player) != TYPE_PLAYER)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "NPC's can't use channels.");
		RETURN_FAIL;
	}
	if(!arg1)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, 
			"Usage:  @channel list\n"
			"        @channel join = <name>\n"
			"        @channel status\n"
			"        @channel who\n"
			"        @channel leave [ = <name> or 'all']\n"
			"        @channel mode = public / private (Operator only)\n"
			"        @channel opadd = <player> (Operator only)\n"
			"        @channel opdel = <player> (Operator only)\n"
			"        @channel invite = <player> (Operator only)\n"
			"        @channel uninvite = <player> (Operator only)\n"
			"        @channel ban = <player> (Operator only)\n"
			"        @channel unban = <player> (Operator only)\n"
			"        @channel boot = <player> (Operator only)\n"
			"        @channel rename = <name> (Operator only)\n"
			"\nAll commands (except 'list' and 'join') apply to your current channel.\n"
			"Type \"help @channel\" for more information.");
		RETURN_FAIL;
	}

	enum ChannelCommand { Unknown, List, Join, Status, Who, Leave, Mode, Opadd, Opdel, Invite, Uninvite, Ban, Unban, Boot, Rename } command = Unknown;
	static struct CommandEntry
	{
		const char*	cmd;
		ChannelCommand	cmdtag;
	}
	Commands[] = {
		{ "list",	List },
		{ "join",	Join },
		{ "status",	Status },
		{ "who",	Who },
		{ "leave",	Leave },
		{ "mode",	Mode },
		{ "opadd",	Opadd },
		{ "opdel",	Opdel },
		{ "invite",	Invite },
		{ "uninvite",	Uninvite },
		{ "ban",	Ban },
		{ "unban",	Unban },
		{ "boot",	Boot },
		{ "rename",	Rename },
		{ "name",	Rename }
	};
	for(unsigned int i = 0; i < (sizeof(Commands) / sizeof(Commands[0])); i++)
	{
		if(string_compare(arg1, Commands[i].cmd) == 0)
		{
			command = Commands[i].cmdtag;
			break;
		}
	}

	if(Unknown == command)
	{
		if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "'%s' is not a valid @channel command", arg1.c_str());
		RETURN_FAIL;
	}

	if(List == command)
	{
		if(!Channel::head())
		{
			notify_colour(player, player, COLOUR_MESSAGES, "There are no channels to list.");
		}
		else
		{
			Channel* iter = Channel::head();
			int total = 0;
			for(; iter != 0; iter = iter->next())
			{
				total++;
				sprintf(scratch_buffer, "%-15.15s%s%s  ", iter->name().c_str(), iter->get_private()?"P":" ",iter->get_censored()?"C":" ");
				for(ChannelPlayer* cit = iter->players(); cit != 0; cit = cit->next())
				{
					if(cit->controller())
						strcat(scratch_buffer, "@");
					strcat(scratch_buffer, db[cit->player()].get_name().c_str());
					strcat(scratch_buffer, ", ");
				}
				scratch_buffer[strlen(scratch_buffer) - 2] = 0;
				notify_colour(player, player, COLOUR_MESSAGES, "%s", scratch_buffer);
			}
			notify_colour(player, player, COLOUR_MESSAGES, "%d channel%s listed.", total, PLURAL(total));
		}
		RETURN_SUCC;
	}

	if (Join == command)
	{
		if(!arg2)
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Which channel do you want to join?");
			RETURN_FAIL;
		}
		bool forced = false;
		if(Wizard(get_effective_id()))
		{
			forced = true;
		}
		if(!Channel::find_and_join_player(player, arg2, forced))
		{
			RETURN_FAIL;
		}
		notify_colour(player, player, COLOUR_MESSAGES, "Channel %s joined.", arg2.c_str());
		RETURN_SUCC;
	}

	if (Leave == command)
	{
		Channel* channel = 0;
		if(!arg2)
		{
			if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Which channel do you want to leave?");
			RETURN_FAIL;
		}
		if(string_compare(arg2, "all")==0)
		{
			if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are no longer on any channels.",COLOUR_REVERT);
			channel_disconnect(player, true);
			RETURN_SUCC;
		}

		if(!Channel::ok_name(arg2))
		{
			if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That isn't a valid channel name.",COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!(channel=Channel::find(arg2)))
		{
			if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No such channel.",COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!channel->find_player(player))
		{
			if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You're not on channel %s.", channel->name().c_str());
			RETURN_FAIL;
		}
			if(!gagged_command())
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You have left channel %s.", channel->name().c_str());
		if(db[player].get_channel() == channel)
			db[player].set_channel(NULL);
		sprintf(scratch_buffer, "%s has left the channel.", db[player].get_name().c_str());
		channel->remove_player_and_send(player, player, scratch_buffer);
		RETURN_SUCC;
	}

	/*
	 * At this point, all further commands require you to be on a channel.
	 */
	Channel *channel=db[player].get_channel();

	if(!channel)
	{
			if(!gagged_command())
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must belong to a channel to use this command.");
		RETURN_FAIL;
	}

	ChannelPlayer *channel_player=channel->find_player(player);

	if(!channel_player)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Bug in code - player missing from default channel.");
		Trace( "BUG: player missing from default channel\n");
		RETURN_FAIL;
	}

	if(Status == command)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Channel %s is %s and %s.", channel->name().c_str(), channel->get_private()? "private":"public",channel->get_censored()?"censored":"uncensored");

		if(channel->invites())
		{
			sprintf(scratch_buffer, "Channel %s outstanding invites:  ", channel->name().c_str());
			for(ChannelPlayer *cp=channel->invites(); cp; cp=cp->next())
			{
				strcat(scratch_buffer, db[cp->player()].get_name().c_str());
				if(cp->next())
					strcat(scratch_buffer, ", ");
			}

			notify_colour(player, player, COLOUR_MESSAGES, "%s", scratch_buffer,COLOUR_REVERT);
		}

		if(channel->bans())
		{
			sprintf(scratch_buffer, "Channel %s banned players:  ", channel->name().c_str());
			for(ChannelPlayer *cp=channel->bans(); cp; cp=cp->next())
			{
				strcat(scratch_buffer, db[cp->player()].get_name().c_str());
				if(cp->next())
					strcat(scratch_buffer, ", ");
			}

			notify_colour(player, player, COLOUR_MESSAGES, "%s", scratch_buffer,COLOUR_REVERT);
		}
	}

	if ((Who == command) || (Status == command))
	{
		sprintf(scratch_buffer, "Channel %s players:  ", channel->name().c_str());
		for(ChannelPlayer *cp=channel->players(); cp; cp=cp->next())
		{
			if(cp->controller())
				strcat(scratch_buffer, "@");
			strcat(scratch_buffer, db[cp->player()].get_name().c_str());
			if(cp->next())
				strcat(scratch_buffer, ", ");
		}
		notify_colour(player, player, COLOUR_MESSAGES, "%s", scratch_buffer,COLOUR_REVERT);
			RETURN_SUCC;
	}

	/*
	 * Once we get here, all commands are operator only.
	 */

	if((!channel_player->controller()) && !Wizard(get_effective_id()))
	{
		if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are not an operator on channel %s.", channel->name().c_str());
		RETURN_FAIL;
	}

	/*
	 * Put all commands that don't take a player as 2nd argument here.
	 */
	if (Mode == command)
	{
		if(!arg2)
		{
			notify(player, "You must specify whether to make the channel public or private.");
			RETURN_FAIL;
		}

		if(strncasecmp(arg2.c_str(), "pub", 3)==0)
		{
			if(!channel->get_private())
			{
				notify(player, "Channel %s is already public.", channel->name().c_str());
				RETURN_FAIL;
			}

			channel->set_private(false);
			sprintf(scratch_buffer, "%s sets the channel public", db[player].get_name().c_str());
			channel->send(player, scratch_buffer, 1);
			RETURN_SUCC;
		}

		if(strncasecmp(arg2.c_str(), "pri", 3)==0)
		{
			if(channel->get_private())
			{
				if(!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Channel %s is already private.", channel->name().c_str());
				RETURN_FAIL;
			}

			channel->set_private(true);
			sprintf(scratch_buffer, "%s sets the channel private", db[player].get_name().c_str());
			channel->send(player, scratch_buffer, 1);
			RETURN_SUCC;
		}

		if(strncasecmp(arg2.c_str(), "cen", 3)==0)
		{
			if(channel->get_censored())
			{
				if(!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Channel %s is already censored.", channel->name().c_str());
				RETURN_FAIL;
			}

			channel->set_censored(true);
			sprintf(scratch_buffer, "%s sets the channel censored", db[player].get_name().c_str());
			channel->send(player, scratch_buffer, 1);
			RETURN_SUCC;
		}

		if(strncasecmp(arg2.c_str(), "unc", 3)==0)
		{
			if(!channel->get_censored())
			{
				if(!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Channel %s is already uncensored.", channel->name().c_str());
				RETURN_FAIL;
			}

			channel->set_censored(false);
			sprintf(scratch_buffer, "%s sets the channel uncensored", db[player].get_name().c_str());
			channel->send(player, scratch_buffer, 1);
			RETURN_SUCC;
		}
		if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must specify either \"private\", \"public\", \"censored\" or \"uncensored\".",COLOUR_REVERT);
		RETURN_FAIL;
	}

	if (Rename == command)
	{
		if(!arg2)
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must give a new name for the channel.",COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!Channel::ok_name(arg2))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That isn't a valid channel name.",COLOUR_REVERT);
			RETURN_FAIL;
		}

		// Check the channel is not already in use.
		if (Channel::find (arg2))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That channel name is already taken.",COLOUR_REVERT);
			RETURN_FAIL;
		}

		sprintf(scratch_buffer, "%s has changed the channel name to \"%s\"", db[player].get_name().c_str(), arg2.c_str());
		channel->send(player, scratch_buffer, 1);

		channel->set_name(arg2);

		RETURN_SUCC;
	}
	/*
	 * All remaining commands require a player as the 2nd argument.
	 */
	if(!arg2)
	{
		if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must specify a player to do that.");
		RETURN_FAIL;
	}

	dbref victim;

	if((victim=lookup_player(player, arg2))==NOTHING)
	{
		if(!gagged_command())
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No such player \"%s\".", arg2.c_str());
		RETURN_FAIL;
	}

	if (Opadd == command)
	{
		ChannelPlayer *vc;

		if(!(vc=channel->find_player(victim)))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not on channel %s.", db[victim].get_name().c_str(), channel->name().c_str());
			RETURN_FAIL;
		}

		if(vc->controller())
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is already an operator on channel %s.", db[victim].get_name().c_str(), channel->name().c_str());
			RETURN_FAIL;
		}

		vc->set_controller(true);

		// add himself etc here one day.
		if (player != victim)
		{
			sprintf(scratch_buffer, ":makes %s an operator", db[victim].get_name().c_str());
			channel->send(player, scratch_buffer);
		}
		else
		{
			channel->send(player, ":gets a promotion to operator");
		}

		RETURN_SUCC;
	}


	if(Opdel == command)
	{
		ChannelPlayer *vc;

		if(!(vc=channel->find_player(victim)))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not on channel %s.",  db[victim].get_name().c_str(), channel->name().c_str(),COLOUR_REVERT);
			RETURN_FAIL;
		}

		if(!vc->controller())
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not an operator on channel %s.", db[victim].get_name().c_str(), channel->name().c_str(),COLOUR_REVERT);
			RETURN_FAIL;
		}

		vc->set_controller(false);

		if (player != victim)
			sprintf(scratch_buffer, "%s removes %s as an operator", db[player].get_name().c_str(), db[victim].get_name().c_str());
		else
			/* resigns from being an operator */
			sprintf(scratch_buffer, "%s resigns from being an operator", db[player].get_name().c_str());
		channel->send(player, scratch_buffer, 1);

		RETURN_SUCC;
	 
	}

	if ((Uninvite == command) || (Invite == command))
	{
		if(channel->find_player(victim))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is already on channel %s.",  db[victim].get_name().c_str(), channel->name().c_str());
			RETURN_FAIL;
		}

		if(channel->player_banned(victim))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is banned from channel %s.",  db[victim].get_name().c_str(), channel->name().c_str());
			RETURN_FAIL;
		}

		if(!Connected(victim))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not connected.", db[victim].get_name().c_str());
			RETURN_FAIL;
		}

		if (Uninvite == command)
		{
			if(channel->player_invited(victim))
			{
				channel->remove_invite(victim);
				notify_colour(victim, victim, COLOUR_MESSAGES, "[%s withdraws your invitation to channel %s]", db[player].get_name().c_str(), channel->name().c_str());
				sprintf(scratch_buffer, "%s withdraws %s's invitation", db[player].get_name().c_str(), db[victim].get_name().c_str());
				channel->send(player, scratch_buffer, 1);
			}
			else
			{
				if(!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That player is not currently invited.");
			}
		}
		else	// must be 'invite'
		{
			time_t now;
			ChannelPlayer *which;
			time(&now);
			if((which=channel->find_invite(victim)) && (now-which->timestamp() < CHANNEL_INVITE_FREQUENCY))
			{
				if(!gagged_command())
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That player is already invited (you can re-send the invite in %s).",time_string(CHANNEL_INVITE_FREQUENCY - now + which->timestamp()));
			}
			else
			{
				if (which)
				{
					which->set_timestamp(time(0));
					notify_colour(victim, victim, COLOUR_MESSAGES, "[%s re-invites you to channel %s (type \"chat %s\" to join)]", db[player].get_name().c_str(), channel->name().c_str(), channel->name().c_str());
					sprintf(scratch_buffer, "%s re-invites %s to the channel", db[player].get_name().c_str(), db[victim].get_name().c_str());
				}
				else
				{
					channel->add_invite(victim);
					notify_colour(victim, victim, COLOUR_MESSAGES, "[%s invites you to channel %s (type \"chat %s\" to join)]", db[player].get_name().c_str(), channel->name().c_str(), channel->name().c_str());
					sprintf(scratch_buffer, "%s invites %s to the channel", db[player].get_name().c_str(), db[victim].get_name().c_str());
				}
				channel->send(player, scratch_buffer, 1);
			}
		}
		RETURN_SUCC;
	}

	if (Boot == command)
	{
		if(player == victim)
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You cannot boot yourself from a channel. Use @channel leave instead");
			RETURN_FAIL;
		}
		if(!channel->find_player(victim))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not on channel %s.", db[victim].get_name().c_str(), channel->name().c_str(),COLOUR_REVERT);
			RETURN_FAIL;
		}

		sprintf(scratch_buffer, "%s has booted %s from the channel", db[player].get_name().c_str(), db[victim].get_name().c_str());
		channel->remove_player_and_send(victim, player, scratch_buffer);
		db[victim].set_channel(NULL);
		RETURN_SUCC;
	}

	if (Ban == command)
	{
		if(player == victim)
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You cannot ban yourself from a channel");
			RETURN_FAIL;
		}
		if(channel->find_ban(victim))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is already banned from channel %s.", db[victim].get_name().c_str(), channel->name().c_str());
			RETURN_FAIL;
		}

		notify_colour(victim, victim, COLOUR_MESSAGES, "[%s has banned you from channel %s]", db[player].get_name().c_str(), channel->name().c_str());
		sprintf(scratch_buffer, "%s has banned %s from the channel.", db[player].get_name().c_str(), db[victim].get_name().c_str());
		if(channel->find_player(victim))
		{
			channel->remove_player_and_send(victim, player, scratch_buffer);
			db[victim].set_channel(NULL);
		}
		else
		{
			channel->send(player, scratch_buffer, 1);
		}
		channel->add_ban(victim);

		RETURN_SUCC;
	}
		
	if (Unban == command)
	{
		if(!channel->player_banned(victim))
		{
			if(!gagged_command())
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not banned from channel %s.",  db[victim].get_name().c_str(), channel->name().c_str());
			RETURN_FAIL;
		}

		notify_colour(victim, victim, COLOUR_MESSAGES, "[%s has removed the ban on you from channel %s]", db[player].get_name().c_str(), channel->name().c_str());
			
		channel->remove_ban(victim);
		sprintf(scratch_buffer, "%s has removed the ban on %s", db[player].get_name().c_str(), db[victim].get_name().c_str());
		channel->send(player, scratch_buffer, 1);

		RETURN_SUCC;
	}

	notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Something went wrong with @channel.");
	Trace("BUG: Something went wrong with @channel");
}


void channel_disconnect(dbref player, bool just_leave)
{
	if(just_leave)
	{
		sprintf(scratch_buffer, "%s has left this channel", db[player].get_name().c_str());
	}
	else
	{
		sprintf(scratch_buffer, "%s has disconnected", db[player].get_name().c_str());
	}
	for(Channel *current=Channel::head(); current; )
	{
		Channel* next = current->next();
		if(current->player_connected(player))
		{
			current->remove_player_and_send(player, player, scratch_buffer);
		}
		current = next;
	}
	
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
context::do_channel_who(const CString& name, const CString& arg2)
{
	int	channel_count = 0;
	time_t	interval;
	Channel *channel=db[player].get_channel();
	const colour_at&	ca = db[get_player()].get_colour_at();
	char	buf[200];

	/*
	 * They'd better be on a channel - otherwise we don't have much to offer.
	 */
	if(!db[player].get_channel())
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are not on a channel.");
		RETURN_FAIL;
	}


	/*
	 * Print out the header: Players on channel: <name> [(Private)]   Idle
	 */
	sprintf(buf, "%s%s %s%s%s",
			ca[COLOUR_TITLES],
			channel->name().c_str(),
			ca[COLOUR_ERROR_MESSAGES],
			channel->get_private() ? "(Private)" : "",
			channel->get_censored() ? "(Censored)" : "");

	notify(player, "%sPlayers on channel: %-30s                                    %sIdle%s",
			ca[COLOUR_MESSAGES],
			buf,
			ca[COLOUR_MESSAGES],
			COLOUR_REVERT);


	/*
	 * Process each player on the channel
	 */
	for(ChannelPlayer *cp=channel->players(); cp; cp=cp->next())
	{

		int	firstchar;
		int	extra_space=0;

		/*
		 * if the are an operator, say so, otherwise put the right number of blanks there
		 */
		sprintf(scratch_buffer, "%s%8s %s%s",
				ca[COLOUR_WIZARDS],
				cp->controller() ? "Operator" : "",
				ca[COLOUR_WHOSTRINGS],
				db[cp->player()].get_name().c_str());

		interval = get_idle_time (cp->player());

		/*
		 * If who-string starts with 'special' character, don't put a space in
		 * (just like normal who)
		 */
		if (db[cp->player()].get_who_string())
                {
			firstchar = db[cp->player()].get_who_string().c_str()[0];

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

			strcat(scratch_buffer, chop_string(db[cp->player()].get_who_string ().c_str(), 60 - extra_space - strlen(db[cp->player()].get_name().c_str())));
		}
		else
		{
			strcat(scratch_buffer, chop_string("", 60 - extra_space - strlen(db[cp->player()].get_name().c_str())));
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

	notify_colour(player, player, COLOUR_MESSAGES, "Players: %i", channel_count);
}


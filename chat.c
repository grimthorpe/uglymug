/*
 * chat.c by Flup (October '95), to implement a chatting system.
 *
 */


#include <ctype.h>

#include "colour.h"
#include "context.h"
#include "command.h"
#include "interface.h"
#include "externs.h"
#include "objects.h"
#include "db.h"

#define	MAX_MORTAL_LISTS	MAX_MORTAL_DICTIONARY_ELEMENTS
#define	MAX_LIST_SIZE		64
#define	MAX_CHANNELS		64
#define	MAX_CHANNEL_SIZE	100
#define	MAX_MORTAL_CHANNELS	5	/* mortals can't initiate more than this number simultaneously */
#define	CLEANUP_INTERVAL	10
#define	CHANNEL_TIMEOUT_PERIOD	300	/* seconds */

#define OBJECTIVE_PRONOUN(p)	((Neuter((p)) || Unassigned((p)))? "its" : Male((p))? "his" : "her")

const char *list_dictionary=	".pleb";	/* Reaps's acronym */
const char *friends_list=	"friends";	/* just so we can word the messages in a special case */
const char *ignore_list=	"ignore";

static int channels_initialised;
static int victim_count;
static dbref victims[MAX_LIST_SIZE];

enum channel_state	/* one bit per state */
{
	CHANNEL_FREE=0x0,
	CHANNEL_BUSY=0x1,
	CHANNEL_NOTO=0x2	/* no timeout */
};

struct player_list
{
	dbref player;
	struct player_list *next;
	struct player_list *prev;
};

static struct
{
	dbref	owner;		/* the person who initiated this channel */
	time_t	last_use;	/* last time the channel was referenced */
	long	checksum;	/* all the player numbers added together */
	int	courtesy;	/* has the list of players been sent to the channel yet? */

	enum channel_state status;

	/* Although we could store cross-references to embedded lists here (e.g. "tell friends,gits=woo"),
	   they might disappear before the channel dies, so better just to copy them over. */

	struct player_list *list;

} channels[MAX_CHANNELS];


/* my_atoi returns 0 if there are any non-numeric characters in the string. */

static int my_atoi(const char *str)
{
	for(const char *c=str; *c; c++)
		if(!isdigit(*c))
			return 0;

	return atoi(str);
}


static void initialise_channels(void)
{
	for(int i=1; i<MAX_CHANNELS; i++)
		memset(&channels[i], 0, sizeof(channels[i]));

	channels_initialised=1;
}


/* adds a player to a player_list, returns the head of the list. */

static struct player_list *add_player_to_player_list(struct player_list *list, int player)
{
	struct player_list *new_player=(struct player_list *) malloc(sizeof(struct player_list));

	new_player->player=player;

	new_player->prev=NULL;
	new_player->next=list;

	if(list)
		list->prev=new_player;

	list=new_player;
	
	return list;
}


static struct player_list *find_player_in_player_list(struct player_list *list, int player, struct player_list **prev=NULL)
{
	struct player_list *previous=NULL;

	for(struct player_list *current=list; current; current=current->next)
	{
		if(current->player==player)
		{
			if(prev)
				*prev=previous;
			return current;
		}

		previous=current;
	}

	return NULL;
}


static dbref find_list_dictionary(dbref player)
{
	Matcher matcher(player, list_dictionary, TYPE_DICTIONARY, GOD_ID);
	matcher.match_variable();
	matcher.match_array_or_dictionary();

	return matcher.match_result();
}


static int singing_la_la_la_cant_hear_you(dbref player, dbref target)
{
	dbref dict;
	int element;

	if((dict=find_list_dictionary(target))==NOTHING)
		return 0;
	
	if(!(element=db[dict].exist_element(ignore_list)))
		return 0;
	
	strcpy(scratch_buffer, db[dict].get_element(element));

	for(char *number=strtok(scratch_buffer, ";"); number; number=strtok(NULL, ";"))
		if(atoi(number)==player)
			return 1;
	
	return 0;
}


static int lookup_players_and_put_their_numbers_in_an_array(dbref player, const char *str)
{
	char *names=strdup(str), *name;

	for(victim_count=0, name=strtok(names, ";,"); name; name=strtok(NULL, ";,"))
		if((victims[victim_count]=lookup_player(player, name))==NOTHING)
			notify_colour(player,player,COLOUR_ERROR_MESSAGES, "There's no player called \"%s\"", name);
		else
		{
			victim_count++;
			if(victim_count==MAX_LIST_SIZE)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can't specify that many players!");
				victim_count=0;
				break;
			}
		}

	free(names);

	return victim_count;
}


static void display_list_statistics(int player, const char *list)
{
	int element;
	dbref lists;

	if((lists=find_list_dictionary(player))==NOTHING)
		notify_colour(player, player, COLOUR_MESSAGES, "You don't have any lists.  Use 'tadd' to create one.");
	else
	{
		if(list)
		{
			if(!(element=db[lists].exist_element(list)))
				notify_colour(player,player, COLOUR_MESSAGES, "You don't have a list called \"%s\"", list);
			else
			{
				/*for(i=1, c=list; c; i++, c=strchr(c, ';')+1);
				notify_colour(player,player, COLOUR_MESSAGES, "List \"%s\" has %d members.", list, i);  TODO */
			}
		}

		notify_colour(player,player,COLOUR_MESSAGES, "You have %d list%s in total, from a maximum of %d.", db[lists].get_number_of_elements(), PLURAL(db[lists].get_number_of_elements()), MAX_MORTAL_LISTS);
	}
}


/*
 * tadd <listname> = <player> [;<player> [...]]
 */

void context::do_tadd(const char *arg1, const char *arg2)
{
	dbref lists;
	static char buf[32];

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if(blank(arg1) || blank(arg2))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Usage:  tadd <listname> = <list of players>");
		return;
	}

	/* Look up the player(s) we're adding - if none exist, return an error. */

	if(lookup_players_and_put_their_numbers_in_an_array(player, arg2)==0)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You didn't specify any valid player names to add to the list.");
		return;
	}

	/* Does the list dictionary exist?  If not, create it. */

	if((lists=find_list_dictionary(player))==NOTHING)
	{
		lists=db.new_object(*new (Dictionary));
		db[lists].set_name(list_dictionary);
		db[lists].set_location(player);
		db[lists].set_owner(GOD_ID);

		Settypeof(lists, TYPE_DICTIONARY);
		db[lists].set_flag(FLAG_WIZARD);
		db[lists].set_flag(FLAG_READONLY);
		db[lists].set_flag(FLAG_DARK);
		PUSH(lists, player, info_items);
	}

	/* Does the list exist in the dictionary?  If not, create it; if so, append to it, making
	   sure that we don't duplicate any of the numbers. */

	int element;

	if((element=db[lists].exist_element(arg1)))
	{
		char *list=strdup(db[lists].get_element(element));
		char *number;

		strcpy(scratch_buffer, list);

		for(int i=0; i<victim_count; i++)
		{
			for(number=strtok(scratch_buffer, ";"); number; number=strtok(NULL, ";"))
				if(atoi(number)==victims[i])
				{
					notify_colour(player, player, COLOUR_MESSAGES, "%s is already in this list.", db[victims[i]].get_name());
					break;
				}

			if(!number)
			{
				notify_colour(player, player, COLOUR_MESSAGES, "Added %s.", db[victims[i]].get_name());
				sprintf(buf, "%s%d", *list? ";":"", victims[i]);
				strcat(list, buf);
			}
		}

		db[lists].set_element(element, arg1, list);

		free(list);
	}
	else
	{
		*scratch_buffer='\0';

		for(int i=0; i<victim_count; i++)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "Added %s.", db[victims[i]].get_name());
			sprintf(buf, "%s%d", i==0? "":";", victims[i]);
			strcat(scratch_buffer, buf);
		}

		db[lists].set_element(0, arg1, scratch_buffer);
	}

	display_list_statistics(player, arg1);

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}


/*
 * tlist [<listname>]
 */

void context::do_tlist(const char *arg1, const char *)
{
	dbref lists;
	int element, channel;

	if(blank(arg1))	/* almost nothing can go wrong */
	{
		return_status=COMMAND_SUCC;
		set_return_string (ok_return_string);
	}
	else
	{
		return_status=COMMAND_FAIL;
		set_return_string (error_return_string);
	}


	lists=find_list_dictionary(player);

	/* List the lists if none was specified; otherwise just dump out that particular list. */

	if(blank(arg1))
	{
		if(lists==NOTHING)
		{
			notify_colour(player,player, COLOUR_ERROR_MESSAGES, "You don't have any lists.");
			return;
		}

		notify_colour(player, player, COLOUR_CONTENTS,"You have the following lists:");
		for(int i=1; i<=db[lists].get_number_of_elements(); i++)
		{
			char *ptr;

			sprintf(scratch_buffer, "   %s:  ", db[lists].get_index(i));
			ptr=strdup(db[lists].get_element(i));
			for(char *c=strtok(ptr, ";"); c; c=strtok(NULL, ";"))
			{
				if(Connected(atoi(c)))
					strcat(scratch_buffer, "*");

				strcat(scratch_buffer, db[atoi(c)].get_name());
				strcat(scratch_buffer, ", ");
			}
			free(ptr);

			scratch_buffer[strlen(scratch_buffer)-2]='\0';
			notify_colour(player, player, COLOUR_MESSAGES, scratch_buffer);
		}
	}
	else
	{
		/* Is this a channel we're talking about? */

		if((channel=my_atoi(arg1))>0)
		{
			/* Yes, it's a channel number. */

			if(!(channels[channel].status & CHANNEL_BUSY))
			{
				notify(player, "Channel %d is not being used.", channel);
				return_status=COMMAND_FAIL;
				set_return_string (error_return_string);
				return;
			}

			if(!find_player_in_player_list(channels[channel].list, player))
			{
				notify(player, "You are not a member of channel %d.", channel);
				return_status=COMMAND_FAIL;
				set_return_string (error_return_string);
				return;
			}

			notify(player, "Players on channel %d:  ", channel);

			*scratch_buffer='\0';
			for(struct player_list *current=channels[channel].list; current; current=current->next)
			{
				strcat(scratch_buffer, db[current->player].get_name());
				strcat(scratch_buffer, ", ");
			}
			scratch_buffer[strlen(scratch_buffer)-2]='\0';

			notify(player, scratch_buffer);

			return;
		}
		else
		{
			/* No, it's actually a list name. */

			if(lists==NOTHING)
			{
				notify(player, "You don't have any lists.");
				return;
			}

			if(!(element=db[lists].exist_element(arg1)))
			{
				notify(player, "You don't have a list called \"%s\".", arg1);
				return;
			}

			notify(player, "Your \"%s\" list contains:", arg1);
			strcpy(scratch_buffer, db[lists].get_element(element));
			for(char *c=strtok(scratch_buffer, ";"); c; c=strtok(NULL, ";"))
				notify(player, "   %s", db[atoi(c)].get_name());
		}
	}

	display_list_statistics(player, blank(arg1)? NULL:arg1);

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}


/*
 * tdelete <listname> [ = <player> [; <player> [...]]]
 * tdelete <channel>
 */

void context::do_tdelete(const char *arg1, const char *arg2)
{
	dbref lists;
	int element, delete_count=0;

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if(blank(arg1))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Usage:  tdelete <listname> [ = <players to delete> ]");
		return;
	}

	if((lists=find_list_dictionary(player))==NOTHING)
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You don't have any lists whatsoever, so you can't delete any.");
		return;
	}

	/* Now check if the list exists in the dictionary. */

	if(!(element=db[lists].exist_element(arg1)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have a list called \"%s\".", arg1);
		return;
	}

	/* If the second argument is blank, delete the entire list.  If not, just delete what's asked for. */

	if(blank(arg2))
	{
		db[lists].destroy_element(element);
		if(db[lists].get_number_of_elements()==0)
			db[lists].destroy(lists);
		
		notify_colour(player, player, COLOUR_MESSAGES, "List \"%s\" deleted.", arg1);
	}
	else
	{
		if(lookup_players_and_put_their_numbers_in_an_array(player, arg2)==0)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You didn't specify any valid player names to delete from the list.");
			return;
		}

		char *names=strdup(db[lists].get_element(element));
		*scratch_buffer='\0';
		int i;

		for(char *c=strtok(names, ";"); c; c=strtok(NULL, ";"))
		{
			for(i=0; i<victim_count; i++)
				if(victims[i]==atoi(c))
					break;

			if(i==victim_count)	/* don't delete this one */
			{
				strcat(scratch_buffer, c);
				strcat(scratch_buffer, ";");
			}
			else
			{
				notify_colour(player, player, COLOUR_MESSAGES, "Deleted %s.", db[victims[i]].get_name());
				delete_count++;
			}
		}

		scratch_buffer[strlen(scratch_buffer)-1]='\0';
		free(names);

		db[lists].set_element(element, arg1, scratch_buffer);

		/* If this list is now empty, delete it.  If there aren't any more lists, delete
		   the dictionary (as above). */

		if(!*scratch_buffer)
		{
			db[lists].destroy_element(element);
			if(db[lists].get_number_of_elements()==0)
				db[lists].destroy(lists);
		}

		notify_colour(player,player, COLOUR_MESSAGES, "%d player%s deleted from the list.", delete_count, PLURAL(delete_count));
	}

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}


static int allocate_new_channel(int player)
{
	for(int i=1, player_total=0; i<=MAX_CHANNELS; i++)
	{
		if((channels[i].status & CHANNEL_BUSY) && channels[i].owner==player)
		{
			player_total++;
			if(player_total==MAX_MORTAL_CHANNELS && !Wizard(player))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You've already started %d eric(chat) channels which still exist, so another one can't be allocated until one of these has timed out (channels time out after %d seconds of inactivity).  Your message will still be sent, but the recipients won't be able to reply by using a channel number.", MAX_MORTAL_CHANNELS, CHANNEL_TIMEOUT_PERIOD);
				return -1;
			}
		}
		else if(channels[i].status==CHANNEL_FREE)
			return i;
	}

	notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The maximum of %d eric(chat) channels are being used, so a new one couldn't be allocated to you (your message will still be sent, but the recipients won't be able to reply by using a channel number).", MAX_CHANNELS);
	return -1;
}


static void make_player_list_eat_keiths_chips(struct player_list *list)
{
	struct player_list *old;

	for(struct player_list *current=list; current; )
	{
		old=current;
		current=current->next;
		free(old);
	}
}


static void make_channel_listen_to_the_outhere_brothers(int channel)
{
	make_player_list_eat_keiths_chips(channels[channel].list);

	channels[channel].list=NULL;
	channels[channel].status=CHANNEL_FREE;
}


static void clobber_inactive_channels(void)
{
	time_t now;

	time(&now);

	for(int i=1; i<MAX_CHANNELS; i++)
		if((channels[i].status & CHANNEL_BUSY) && !(channels[i].status & CHANNEL_NOTO) && now-channels[i].last_use >= CHANNEL_TIMEOUT_PERIOD)
			make_channel_listen_to_the_outhere_brothers(i);
}


static char *generate_courtesy_string(dbref source, dbref destination, struct player_list *list)
{
	static char buf[256];

	*buf='\0';

	for(struct player_list *current=list; current; current=current->next)
		if(source!=current->player && destination!=current->player)
		{
			if(*buf)
				if(source!=destination)
					strcat(buf, ", ");
				else if(current->next)
					if((current->next->player==source || current->next->player==destination) && !current->next->next)
						strcat(buf, " and ");
					else
						strcat(buf, ", ");
				else
					strcat(buf, " and ");

			strcat(buf, db[current->player].get_name());
		}

	if(source!=destination)
		strcat(buf, " and you ");
	
	return buf;
}


static void notify_player_list(dbref player, struct player_list *list, int channel, const char *str)
{
	static char buf[16], zero='\0', *courtesy;

	if(channel)
		sprintf(buf, "(Chat %d) ", channel);

	else
		*buf='\0';

	courtesy=&zero; /*default*/

	if(*str!=':')
	{
		if(channel && !channels[channel].courtesy)
			courtesy=generate_courtesy_string(player, player, list);

		else
			courtesy=&zero;
		notify_colour(player, player, COLOUR_SAYS, "%sYou %s%s \"%s\"", buf, *courtesy? "tell ":"say", courtesy, str);
	}

	for(struct player_list *current=list; current; current=current->next)
	{
		if(!Eric(current->player) || !Connected(current->player) || singing_la_la_la_cant_hear_you(player, current->player))
			continue;

		if(*str==':')
			notify(current->player, "%s%s%s%s", buf, db[player].get_name(), str[1]=='\''? "":" ", str+1);
		else if(player!=current->player)
		{
			if(channel && !channels[channel].courtesy)
				courtesy=generate_courtesy_string(player, current->player, list);

			else
				courtesy=&zero;
			notify_colour(current->player, player, COLOUR_TELLS, "%s%s %s %s\"%s\"", buf, db[player].get_name(), *courtesy? "tells":"says", courtesy, str);
		}
	}
	if (channel && !channels[channel].courtesy)
		channels[channel].courtesy=1; /* They've seen the huge list of users now. */
}


static void notify_channel(dbref player, int channel, const char *str)
{
	notify_player_list(player, channels[channel].list, channel, str);

	time(&channels[channel].last_use);
}


/*
 * tell (<player> [; <player> [...]] | <listname> | <channel number>) = [:] <message>)
 *
 * List names take precedence (unless there aren't any!).
 *
 */

void context::do_tell(const char *arg1, const char *arg2)
{
	int channel, count, element;
	static int cleanup_countdown=CLEANUP_INTERVAL;
	struct player_list *targets=NULL;
	dbref target;
	long checksum;
	char *c;

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if(!channels_initialised)
		initialise_channels();

	if(--cleanup_countdown==0)
	{
		clobber_inactive_channels();
		cleanup_countdown=CLEANUP_INTERVAL;
	}

	if(in_command())
	{
		notify_colour(player, player, COLOUR_MESSAGES, "You can't use 'tell' inside a command.");
		return;
	}

	if(!Eric(player))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Your ERIC flag, which is needed to use 'tell' and 'chat', has been set due to this tell.");
		db[player].set_flag(FLAG_ERIC);
	}

	if(blank(arg1) || blank(arg2))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Usage:  tell <player(s)> = <message>");
		notify_colour(player, player, COLOUR_MESSAGES, " -or-   tell <list(s)> = <message>");
		notify_colour(player, player, COLOUR_MESSAGES, " -or-   tell <channel number> = <message>");

		return;
	}

	/* Is it a single channel number?  (if it's got commas in it, then we form a new channel out
	   of one or more old ones and possibly additional players and lists) */

	if((channel=my_atoi(arg1))!=0 && !strchr(arg1, ',') && !strchr(arg1, ';'))
	{
		   fprintf(stderr, "It's channel %d\n", channel);
		  

		if(channel>MAX_CHANNELS)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Channel numbers are from 1 to %d.\n", channel);
			return;
		}

		struct player_list *current;



		for(current=channels[channel].list; current; current=current->next)
			fprintf(stderr, "  Member of %d:  %s\n", channel, db[current->player].get_name());

		/* Is the channel in use? */

		if(!(channels[channel].status & CHANNEL_BUSY))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That channel is not in use.");
			return;
		}

		/* Don't allow this if we're not a member of the channel. */

		for(current=channels[channel].list; current; current=current->next)
			if(current->player==player)
				break;

		if(!current)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You are not a member of channel %d.", channel);
			return;
		}

		notify_channel(player, channel, arg2);

		return_status=COMMAND_SUCC;
		set_return_string (ok_return_string);

		return;
	}

	/* OK, so it's one or more players/lists.  If it's only one player, then we make a special case of it
	   and just notify that player.  If not, then we build a list of players to tell, and scan the channels
	   to see if there's already such a channel.  If there is, we use it.  This is so that people can type
	   "tell friends" (or shorthand form, e.g. "fchat") without creating a channel each time. */


	dbref lists=find_list_dictionary(player);

	strcpy(scratch_buffer, arg1);	/* strtok() is destructive */

	for(checksum=0, count=0, c=strtok(scratch_buffer, ",;"); c; c=strtok(NULL, ",;"))
	{
		char str[512], *ptr, *woo;

		/* Trim whitespace.  My variable names are the best on the planet. */

		strcpy(str, c);
		woo=str+strspn(str, " ");
		for(ptr=woo+strlen(woo)-1; ptr>=woo && isspace(*ptr); ptr--)
			*ptr='\0';

		if(lists!=NOTHING)
		{
			if((element=db[lists].exist_element(woo)))
			{
				/* Copy an entire list in. */
				char *list=strdup(db[lists].get_element(element));
				for(char *ptr=strtok(list, ";"); ptr; ptr=strtok(NULL, ";"))
				{
					if (!find_player_in_player_list(targets, atoi(ptr)))
					{
						targets=add_player_to_player_list(targets, atoi(ptr));
						checksum+=targets->player;
						count++;
					}	
				}
				free(list);

				continue;
			}
		}

		/* Are we adding an entire channel? */

		if( (channel=my_atoi(woo))>0)
		{
			if(!(channels[channel].status & CHANNEL_BUSY))
				notify_colour(player, player, COLOUR_MESSAGES, "There's no channel %d at the moment.", channel);
			else if(!find_player_in_player_list(channels[channel].list, player))
				notify_colour(player, player, COLOUR_MESSAGES, "You're not a member of channel %d.", channel);
			else
			{
				for(struct player_list *current=channels[channel].list; current; current=current->next)
					if(!find_player_in_player_list(targets, current->player))
					{
						targets=add_player_to_player_list(targets, current->player);

						checksum+=targets->player;
						count++;
					}
			}

			continue;
		}

		/* OK, it's just a single player. */

		if((target=lookup_player(player, woo))!=NOTHING)
		{
			if(target==player)
				notify_colour(player,player, COLOUR_MESSAGES, "You must have a very short memory to need to tell yourself things.");
			else if(!find_player_in_player_list(targets, target))
			{
				if(!Connected(target))
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is not currently connected.", db[target].get_name());
				else if(!Eric(target))
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s doesn't have %s eric flag set.", db[target].get_name(), OBJECTIVE_PRONOUN(target));
				else if(singing_la_la_la_cant_hear_you(player, target))
					notify_colour(player, player, COLOUR_ERROR_MESSAGES, "%s is ignoring you.", db[target].get_name());
				else
				{
					targets=add_player_to_player_list(targets, target);

					checksum+=target;
					count++;
				}
			}
		}
		else
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Can't find a list or a player called \"%s\".", woo);

		if(count>=MAX_CHANNEL_SIZE)
		{
			notify(player, "You can't tell that to more than %d people at once.", MAX_CHANNEL_SIZE);
			make_player_list_eat_keiths_chips(targets);

			return;
		}
	}
	if (!find_player_in_player_list(targets,player))
	    checksum+=player; /* Don't want to add them twice */

	if(count==0)	/* Nothing valid to send the message to. */
		return;

	if(count==1)
	{
		/* Don't bother making a channel. */

		if(*arg2==':')
		{
			notify_colour(player, player, COLOUR_TELLS, "%s%s%s", db[player].get_name(), arg2[1]=='\''? "":" ", arg2+1);
			notify_colour(targets->player, player, COLOUR_TELLS, "%s%s%s", db[player].get_name(), arg2[1]=='\''? "":" ", arg2+1);
		}
		else
		{
			notify_colour(player, player, COLOUR_TELLS, "You tell %s \"%s\"", db[targets->player].get_name(), arg2);
			notify_colour(targets->player, player, COLOUR_TELLS, "%s tells you \"%s\"", db[player].get_name(), arg2);
		}
	}
	else
	{
		/* Is there already a channel with the same people on it?  Keith reckons that checksumming the
		   player numbers is good enough, and so he gets blamed if it all goes pear-shaped. */

		int i;

		for(i=1; i<MAX_CHANNELS; i++)
		{
			if(!(channels[i].status & CHANNEL_BUSY))
				continue;

			if(channels[i].checksum==checksum)
				break;
		}

		if(i<MAX_CHANNELS)
			notify_channel(player, i, arg2);
		else
		{
			if((channel=allocate_new_channel(player))<0)
			{
				notify_player_list(player, targets, 0, arg2);
				make_player_list_eat_keiths_chips(targets);

				return_status=COMMAND_SUCC;
				set_return_string (ok_return_string);

				return;
			}

			/* Add ourselves to the channel list. */

			targets=add_player_to_player_list(targets, player);

			channels[channel].owner=player;
			channels[channel].status=CHANNEL_BUSY;
			channels[channel].checksum=checksum;
			channels[channel].list=targets;
			channels[channel].courtesy=0;
			time(&channels[channel].last_use);

			notify_channel(player, channel, arg2);
		}
	}

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}


/*
 * tleave <channel> [ = <reason> ]
 *
 * Remove yourself from a channel.
 *
 */

void context::do_tleave(const char *arg1, const char *arg2)
{
	int channel;
	struct player_list *old, *previous;

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if(blank(arg1))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "Usage:  tleave <channel>");
		return;
	}

	if(!channels_initialised)
		initialise_channels();

	if(!(channel=my_atoi(arg1)) || channel>MAX_CHANNELS)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That's not a valid channel number.");
		return;
	}

	if(!(channels[channel].status & CHANNEL_BUSY))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "That channel isn't in use.");
		return;
	}

	if(!(old=find_player_in_player_list(channels[channel].list, player, &previous)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You're not a member of that channel.");
		return;
	}

	/* All OK - do it. */

	if(blank(arg2))
		notify_channel(player, channel, ":has left the channel.");
	else
	{
		sprintf(scratch_buffer, ":has left the channel %s", arg2);
		notify_channel(player, channel, scratch_buffer);
	}

	notify_colour(player, player, COLOUR_MESSAGES, "You have left channel %d.", channel);

	if(old==channels[channel].list)
		channels[channel].list=old->next;
	else
		previous->next=old->next;
	
	free(old);

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}

/*
 * lists.c originally chat.c by Flup (October '95), to implement a chatting system.
 * Re-written totally by Luggage, March 1997 as this overlaps @channel too much.
 * There is now 1 main game list on which you can set flags like ignore,
 * inform, friend (TSK!), and also user-defined lists such as 'gits' or
 * 'mancunions'. Also an entire new class, 'Player_list' which is very sexy and
 * useful - it makes writing ranged commands dead easy and you can exclude 
 * players with flags set/unset, list values set/unset, etc. Cool eh?
 */

/* PS Most of this was written in a state of sleep-deprivation so apologies if it's crapper than usual. 
      Then again, if it's lobster, be amazed. 
 */

#define ASSIGN_STRING(d,s)      { if ((d)) free ((d));  if ((s && *s!=0)) (d)=strdup((s)); else (d)=NULL; }
#define OBJECTIVE_PRONOUN(p)    ((Neuter((p)) || Unassigned((p)))? "its" : Male((p))? "his" : "her")

#define MAX_IDLE_MESSAGE_LENGTH 70
#include <ctype.h>
#include <string.h>

#include "colour.h"
#include "context.h"
#include "command.h"
#include "db.h"
#include "externs.h"
#include "interface.h"
#include "lists.h"
#include "objects.h"

static char squiggles[]="%w%h+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=%z";
#define	MAX_MORTAL_LISTS	MAX_MORTAL_DICTIONARY_ELEMENTS

#define OBJECTIVE_PRONOUN(p)	((Neuter((p)) || Unassigned((p)))? "its" : Male((p))? "his" : "her")


// Playerlist is where we set flags like inform, ignore, friend
// list_dictionary is where we store our lists, eg manchester=satan, dunk, rincewind, scott

const char *list_dictionary=		".lists";
const char *playerlist_dictionary=	".playerlist";
const char *reverseplist_dictionary=	".reverseplist";
const char *reverseclist_dictionary=	".reverseclist";

static int victim_count;
static dbref victims[MAX_LIST_SIZE];


struct	list_flags	playerlist_flag_list [] =
{
	{ "Fblock",	PLIST_FBLOCK },
	{ "Friend",	PLIST_FRIEND },
	{ "Ignore",	PLIST_IGNORE },
	{ "Inform",	PLIST_INFORM },
	{ NULL,		0}
};


dbref
find_list_dictionary(dbref player, const char *which)
{
	Matcher matcher(player, which, TYPE_DICTIONARY, GOD_ID);
	matcher.match_variable();
	matcher.match_array_or_dictionary();

	int lists= matcher.match_result();
	if (lists==NOTHING)
	{
		lists=db.new_object(*new (Dictionary));
		db[lists].set_name(which);
		db[lists].set_location(player);
		db[lists].set_owner(GOD_ID);

		Settypeof(lists, TYPE_DICTIONARY);
		db[lists].set_flag(FLAG_WIZARD);
		db[lists].set_flag(FLAG_READONLY);
		db[lists].set_flag(FLAG_DARK);
		PUSH(lists, player, info_items);
	}
	if (lists==NOTHING)
		panic("Unable to create list dictionary!");
	return lists;
}

/* Any sexist variable names in the following function are duly apologised for - well you can hardly 'lad' a woman :) */

void
set_reverse_map(dbref bloke_being_ladded, dbref bloke_doing_ladd, int value)
{
	char smallbuf[10];
	char smallbuf2[10];
	dbref lists=find_list_dictionary(bloke_being_ladded, reverseplist_dictionary);
	sprintf(smallbuf,"%d", bloke_doing_ladd);
	dbref element=db[lists].exist_element(smallbuf);
	sprintf(smallbuf2, "%d", value);
	db[lists].set_element(element, smallbuf, smallbuf2);
}

void
delete_reverse_map(dbref bloke_being_deleted, dbref bloke_doing_delete)
{
	char smallbuf[10];
	dbref lists=find_list_dictionary(bloke_being_deleted, reverseplist_dictionary);
	sprintf(smallbuf,"%d", bloke_doing_delete);
	dbref element=db[lists].exist_element(smallbuf);
	if (element==0)
		fprintf(stderr, "BUG: Attempting to remove a reverse player mapping where none exists.\n");
	else
		db[lists].destroy_element(element);
}

void
add_clist_reference(dbref bloke_being_ladded, dbref bloke_doing_ladd)
{
	char smallbuf[10];
	char smallbuf2[10];
	int value;

	dbref lists=find_list_dictionary(bloke_being_ladded, reverseclist_dictionary);
	sprintf(smallbuf,"%d", bloke_doing_ladd);
	dbref element=db[lists].exist_element(smallbuf);
	value= element ? atoi(db[lists].get_element(element)) : 0;
	sprintf(smallbuf2, "%d", value + 1);
	db[lists].set_element(element, smallbuf, smallbuf2);
}

void
remove_clist_reference(dbref bloke_being_lremoved, dbref bloke_doing_lremove)
{
	char smallbuf[10];
	char smallbuf2[10];
	int value;

	dbref lists=find_list_dictionary(bloke_being_lremoved, reverseclist_dictionary);
	sprintf(smallbuf,"%d", bloke_doing_lremove);
	dbref element=db[lists].exist_element(smallbuf);
	if (element==0)
	{
		fprintf(stderr, "BUG: Trying to remove a reverse custom list reference when none exists.\n");
		return;
	}
	value= atoi(db[lists].get_element(element)) - 1;
	if (value)
	{
		sprintf(smallbuf2, "%d", value);
		db[lists].set_element(element, smallbuf, smallbuf2);
	}
	else
		db[lists].destroy_element(element);
}


static const char *playerlist_flags(int value)
{
	static char buf[128];
	buf[0]='\0';

	for (int i=0; playerlist_flag_list[i].string != NULL ; i++)
		if (value & playerlist_flag_list[i].flag)
		{
			if (*buf)
				strcat(buf, ", ");
			strcat(buf, playerlist_flag_list[i].string);
		}
	return buf;
}
void		
context::do_lset(const char *victims, const char *flag)
{
	return_status= COMMAND_FAIL;	
	set_return_string(error_return_string);
	dbref lists=0;
	int element,i,f;

	if (blank(victims) || (blank(flag)))
	{
		notify_colour(player,player,COLOUR_ERROR_MESSAGES,"Syntax: lset <playerlist>=flag");
		return;
	}

	// Now figure out what flag we're s'posed to be setting/unsetting
	const char *p;
	for (p=flag; p && (*p==NOT_TOKEN || isspace(*p)); p++);

	if ((p==NULL) || (*p=='\0'))
	{	
		notify_colour(player,player,COLOUR_ERROR_MESSAGES,"You must specify a valid playerlist flag.");
		return;
	}

	for (i=0; playerlist_flag_list[i].string != NULL; i++)
		if (string_prefix(playerlist_flag_list[i].string, p))
		{
			f=playerlist_flag_list[i].flag;
			break;
		}

	if (playerlist_flag_list[i].string == NULL)
	{
		notify_colour(player,player,COLOUR_ERROR_MESSAGES,"I don't recognise that player list flag.");
		return;
	}

	Player_list targets(player);
	targets.include_unconnected();
	targets.build_from_text(player, victims);

	lists=find_list_dictionary(player, playerlist_dictionary);

	char smallbuf[128];
	smallbuf[0]='\0';


	dbref target=targets.get_first();
	while (target != NOTHING)
	{
		sprintf(smallbuf,"%d", target);
		if ((element=db[lists].exist_element(smallbuf))==0)
		{
			db[lists].set_element(0, smallbuf, "0");
			if (!gagged_command())
				notify_censor_colour(player, player, COLOUR_MESSAGES, "Player %s added to your main list.", db[target].get_name());
			element=db[lists].exist_element(smallbuf);
		}
		int newflags;
		if (*flag == NOT_TOKEN)
			newflags=atoi(db[lists].get_element(element)) & ~f;
		else
			newflags=atoi(db[lists].get_element(element)) | f;

		sprintf(smallbuf,"%d", newflags);
		db[lists].set_element(element, NULL, smallbuf);
		set_reverse_map(target, player, newflags);
		target=targets.get_next();
	}

	if (!gagged_command())	
	{
		if (*flag == NOT_TOKEN)
			notify_colour(player, player, COLOUR_MESSAGES, "%s flag reset on %s.", playerlist_flag_list[i].string, targets.generate_courtesy_string(player,player,True));
		else
			notify_colour(player, player, COLOUR_MESSAGES, "%s flag set on %s.", playerlist_flag_list[i].string, targets.generate_courtesy_string(player,player,True));
	}
}

int
Player_list::get_first()
{
	if ((list==NULL) || (filtered_size==0))
		return NOTHING;

	current=list;
	while (current && (current->included==False))
		current=current->next;

	if (current)
		return current->player;
	else
		return NOTHING; // Should never happen
}

int 
Player_list::get_next()
{
	if (!current)
		return NOTHING;
	do
		current=current->next;
	while (current && (current->included==False));

	return (current)? (current->player):NOTHING;
}

void
Player_list::beep()
{
	PLE *current=list;
	while (current)
	{
		if (current->included==True)
			::beep(current->player);
		current=current->next;
	}
}

void
Player_list::trigger_command(const char *command, context &c)
{
	PLE *current=list;
	dbref automatic;
	while (current)
	{
		if (current->included == False)
		{
			current=current->next;
			continue;
		}
		Matcher matcher (current->player, command, TYPE_COMMAND, originator);
		matcher.check_keys ();
		matcher.match_player_command ();
		if ((automatic = matcher.match_result ()) != NOTHING && automatic != AMBIGUOUS)
		{
			if (db [automatic].get_owner () != current->player)
			{
				notify_colour (current->player, current->player, COLOUR_ERROR_MESSAGES, "WARNING: %s not owned by you. Not triggered.", command);
				current=current->next;
				continue;
			}

			context	*new_context =new context(current->player);

			new_context->set_depth_limit (c.get_depth_limit () - 1);
			// Maybe need to set commands_executed here...
			if (!Dark (automatic) && could_doit (*new_context, automatic))
			{
				new_context->do_compound_command (automatic, command, getname_inherited (c.get_player()), "");
				delete mud_scheduler.push_express_job(new_context);
			}
		}
	current=current->next;
	}
}

void
Player_list::warn_me_if_idle()
{
	PLE	*current=list;
	while (current)
	{
		int my_idletime=0, time_index=0, message_index=0;
		dbref automatic;
		*scratch_buffer='\0';

		Matcher matcher (current->player, ".message", TYPE_DICTIONARY, originator);
		matcher.check_keys ();
		matcher.match_variable();
		matcher.match_array_or_dictionary();
		if (((automatic = matcher.match_result ()) != NOTHING)
			&& (db[automatic].get_owner() == current->player))
		{
			if ((time_index= db[automatic].exist_element("Idle-Time")))
			{
				if ((my_idletime=atoi(value_or_empty(db[automatic].get_element(time_index)))) < 180)
				{
					db[automatic].set_element(time_index, "Idle-Time", "180");
					notify_colour(current->player, current->player, COLOUR_MESSAGES, "WARNING: Your .message[Idle-Time] (minimum idle time) has been changed to 3 minutes.");
				}
			}

			if ((message_index= db[automatic].exist_element("Idle-Message")))
			{
				char *check;
				strcpy(scratch_buffer, value_or_empty(db[automatic].get_element(message_index)));
				if ((check = strchr(scratch_buffer, '\n')))
				{	
					*check = '\0';
					notify_colour(current->player, current->player, COLOUR_ERROR_MESSAGES, "WARNING: Idle message truncated at newline.");
					db[automatic].set_element(message_index, "Idle-Message", scratch_buffer);
				}

				if(strlen(scratch_buffer) > MAX_IDLE_MESSAGE_LENGTH)
				{
					notify_colour(current->player,current->player, COLOUR_ERROR_MESSAGES, "WARNING: Idle message truncated to %d characters.", MAX_IDLE_MESSAGE_LENGTH);
					scratch_buffer[MAX_IDLE_MESSAGE_LENGTH] = '\0';
					db[automatic].set_element(message_index, "Idle-Message", scratch_buffer);
				}
			}
		}

		if (my_idletime < 180)
			my_idletime=180;
		if (my_idletime < get_idle_time(current->player))
			if (*scratch_buffer)
				notify_censor_colour (originator, current->player, COLOUR_MESSAGES, "Idle message from %s who has been inactive for %d minutes:\n%s", getname_inherited (current->player), get_idle_time(current->player)/60, scratch_buffer);
			else
				notify_colour(originator, current->player, COLOUR_MESSAGES, "%s has been idle for %d minutes.",  getname_inherited (current->player), get_idle_time(current->player)/60);
	current=current->next;
	}
}

void
Player_list::set_included(PLE *player, Boolean state, const char *message = NULL)
{
	int index;
	dbref automatic = NOTHING;

	if (!player)
	{
		fprintf(stderr,"BUG: Trying to set the included state of NULL.\n");
		return;
	}
	filtered_size+=(state==player->included)?0:(state==True)?1:-1;

	if ((player->included==True) && (message) && (state==False))
	{
		if ((player->from_a_list==False) && ((*message==HACKY_INSERT_HAVENMESSAGE) || (*message==HACKY_INSERT_SLEEPINGMESSAGE)))
		{
			Matcher matcher (player->player, ".message", TYPE_DICTIONARY, originator);
			matcher.check_keys ();
			matcher.match_variable();
			matcher.match_array_or_dictionary();
			automatic = matcher.match_result ();
			if ((automatic != NOTHING) && (db[automatic].get_owner() != player->player))
				automatic=NOTHING;
		}

		switch (*message)
		{
			case HACKY_INSERT_PLAYERNAME:
				notify_censor(originator, originator,"%s %s", db[player->player].get_name(), message+1);
				break;

			case HACKY_INSERT_HAVENMESSAGE:
				if ((automatic!=NOTHING) && (index=db[automatic].exist_element("Haven")))
					notify_censor_colour (originator, player->player, COLOUR_MESSAGES, "Haven message from %s:  %s", getname_inherited (player->player), db[automatic].get_element(index));
				else
					notify_colour (originator, player->player, COLOUR_MESSAGES, "%s does not wish to be disturbed.", getname_inherited (player->player));
				break;

			case HACKY_INSERT_SLEEPINGMESSAGE:
				if (player->from_a_list == True)
					break;
				if ((automatic!=NOTHING) && (index=db[automatic].exist_element("Sleep")))
					notify_censor_colour (originator, player->player, COLOUR_MESSAGES, "Sleeping message from %s:  %s", getname_inherited (player->player), db[automatic].get_element(index));
				else
					notify_colour (originator, player->player, COLOUR_MESSAGES, "%s is not connected.", getname_inherited (player->player));
				break;
			default:
				notify_censor(originator, originator, message);
		}
	}
	player->included=state;
}

Player_list::~Player_list()
{
	eat_keiths_chips();
}

void
Player_list::eat_keiths_chips()   // Kills anything still alive
{
	PLE	*old,
		*current=list;

	while (current)
	{
		old=current;
		current=current->next;
		free(old);
	}
	filtered_size=0;
	count=0;
	list=NULL;
	while (listcount > 0)
		free(listnames[--listcount]);

}

PLE *
Player_list::find_player(dbref player)
{
	PLE	*current=list;

	while (current)
	{
		if(current->player==player)
			return current;
		current=current->next;
	}

	return NULL;
}

int
Player_list::include_if_unset(const int f)
{
	PLE *temp;
	for (int i=0; i<db.get_top(); i++)
	{
		if ( (Typeof(i) == TYPE_PLAYER) && !(db[i].get_flag(f)))
			if ((temp=find_player(i)))	
				set_included(temp, True);
			else
				add_player(i, True);
	}
	return filtered_size;
}

int
Player_list::include_if_set(const int f)
{
	PLE *temp;
	for (int i=0; i<db.get_top(); i++)
	{
		if ( (Typeof(i) == TYPE_PLAYER) && (db[i].get_flag(f)))
			if ((temp=find_player(i)))
				set_included(temp, True);
			else
				add_player(i, True);
	}
	return filtered_size;
}

int
Player_list::filter_out_if_unset(const int f, const char *message=NULL)
{
	PLE *current=list;
	while (current)
	{
		if (!(db[current->player].get_flag(f)))
			set_included(current, False, message);
		current=current->next;
	}
	return filtered_size;
}

int
Player_list::filter_out_if_set(const int f, const char *message=NULL)
{
	PLE *current=list;
	while (current)
	{
		if (db[current->player].get_flag(f))
			set_included(current, False, message);
		current=current->next;
	}
	return filtered_size;
}

int
Player_list::exclude(int player, const char *message=NULL)
{
	PLE *temp;
	if ((temp=find_player(player)))
		set_included(temp, False, message);
	return filtered_size;
}

int
Player_list::include_from_list(dbref player, int f)
{
	dbref lists=find_list_dictionary(player, playerlist_dictionary);
	if (db[lists].get_number_of_elements() == 0)
		return filtered_size;

	for (int i=1; i<=db[lists].get_number_of_elements(); i++)
		if ((atoi (db[lists].get_element(i)) & f) && (!find_player (atoi (db[lists].get_index(i)))))
			add_player(atoi(db[lists].get_index(i)), True);
	return filtered_size;
}

int
Player_list::include_from_reverse_list(dbref player, int f)
{
	dbref lists=find_list_dictionary(player, reverseplist_dictionary);
	if (db[lists].get_number_of_elements() == 0)
		return filtered_size;

	for (int i=1; i<=db[lists].get_number_of_elements(); i++)
		if ((atoi (db[lists].get_element(i)) & f) && (!find_player (atoi (db[lists].get_index(i)))))
			add_player(atoi(db[lists].get_index(i)), True);
	return filtered_size;
}
int
Player_list::exclude_from_list(dbref player, int f, const char *message=NULL)
{
	PLE *temp;
	dbref lists=find_list_dictionary(player, playerlist_dictionary);
	if (db[lists].get_number_of_elements() == 0)
		return filtered_size;

	for (int i=1; i<=db[lists].get_number_of_elements(); i++)
		if ((atoi (db[lists].get_element(i)) & f) && (temp=find_player (atoi (db[lists].get_index(i)))))
			set_included(temp, False, message);

	return filtered_size;
}

int
Player_list::exclude_from_reverse_list(dbref player, int f, const char *message=NULL)
{
	PLE *temp;
	dbref lists=find_list_dictionary(player, reverseplist_dictionary);
	if (db[lists].get_number_of_elements() == 0)
		return filtered_size;

	for (int i=1; i<=db[lists].get_number_of_elements(); i++)
		if ((atoi (db[lists].get_element(i)) & f) && (temp=find_player (atoi (db[lists].get_index(i)))))
			set_included(temp, False, message);
	return filtered_size;
}

		
int
Player_list::include(int player)
{
	PLE *temp;
	if (!(temp=find_player(player)))
		temp=add_player(player);

	set_included(temp, True);

	return filtered_size;
}

/* adds a player to a player_list, returns the head of the list. */

PLE *
Player_list::add_player(int player, Boolean fromlist=False)
{
	if (Typeof(player) != TYPE_PLAYER)
	{
		fprintf(stderr, "BUG: Non-player %d on a player list.\n",player);
		return list;
	}

	if ((_include_unconnected == False) && (!Connected(player)))
		return list;
	PLE *new_player=(PLE *) malloc(sizeof(PLE));

	new_player->player=player;
	new_player->included=True;
	new_player->prev=NULL;
	new_player->next=list;
	new_player->from_a_list=fromlist;

	if(list)
		list->prev=new_player;

	list=new_player;
	filtered_size++;
	count++;
	return list;
}

const char *
Player_list::generate_courtesy_string(dbref source, dbref destination, Boolean inform_me=False)
{
	int		done=0,
			remaining=0;
	char		buf[256];
	static char	listbuf[256];
	char		workspace[16*1024];
	PLE		*current=list;
	Boolean		include_you=False;
	*listbuf='\0';
	*buf='\0';
	workspace[0]='\0';


	while (current)
	{
		if ((current->player != destination) && (current->included == True) && (current->from_a_list == False))
			remaining++;
		if ((current->player == destination) && (current->included == True) && (current->from_a_list==False))
			include_you=True;
		current=current->next;
	}

	/* First copy in any list names: */
	int thislist=0;
	int lists_remaining=listcount;

	if (listcount > 4)
		sprintf(listbuf, "several of %s lists", (source==destination) ? "your":OBJECTIVE_PRONOUN(source));
	else
	{
		if (listcount > 0)
			sprintf(listbuf, "%s ", (source==destination) ? "your":OBJECTIVE_PRONOUN(source));		

		while (thislist < listcount)
		{
			sprintf(workspace, "%s", listnames[thislist++]);
			strcat(listbuf, workspace);
			lists_remaining--;
			if (lists_remaining > 1)
				strcat(listbuf, ", ");
			else if (lists_remaining == 1)
				strcat(listbuf, " and ");
		}

		if (listcount == 1)
			strcat(listbuf," list");
		else if (listcount > 1)
			strcat(listbuf," lists");

	}

	if (listcount > 0)
	{
		if ((source==destination) && (remaining > 0))
			strcat(listbuf, ", and ");
		else if ((include_you==True) || (remaining == 1 ))
			strcat(listbuf, ", and to ");
		else if (remaining > 1)
			strcat(listbuf, ", and to the players ");
	}

	current=list;

	while ((done < 4) && current && remaining)
	{
		if ((current->included == False) || (current->from_a_list == True))
		{
			current=current->next;
			continue;
		}

		if (destination == current->player)
		{
			current=current->next;
			continue;
		}

		done++;
		remaining--;
		if((*buf) || (include_you==True))
		{
			if (remaining == 0)
				strcat(buf, " and ");
			else
				strcat(buf, ", ");
		}

		strcat(buf, db[current->player].get_name());
		current=current->next;
	}

	if (include_you == True)
	{
		sprintf(workspace, "%s%s", inform_me ? "yourself":"you", buf);
		strcpy(buf,workspace);
	}
		
	// Now add on how many are left

	if (remaining)
	{
		sprintf(workspace, " and %d other %s", remaining, (remaining==1) ? "person":"people");
		strcat(buf, workspace);
	}
	strcat(listbuf, buf);
	return listbuf;
}

void
Player_list::notify(
  dbref player
, const char *prefix
, const char *suffix
, const char *str)
{
	static char zero='\0';
	const char *courtesy;

	courtesy=&zero; /*default*/

	for(PLE *current=list; current; current=current->next)
	{
		if (current->included==False)
			break;

		if (Haven(current->player))
		{
			current->included=False;
			int index;
			dbref automatic;

			Matcher matcher (current->player, ".message", TYPE_DICTIONARY, originator);
			matcher.check_keys();
			matcher.match_variable();
			matcher.match_array_or_dictionary();
			if (((automatic=matcher.match_result())!=NOTHING) &&
			    (db[automatic].get_owner() == current->player) &&
			    (index=db[automatic].exist_element("Haven")))
				notify_censor_colour(originator, current->player, COLOUR_MESSAGES, "Haven message from %s:  %s", getname_inherited(current->player), db[automatic].get_element(index));
			else
				notify_censor_colour(originator, current->player, COLOUR_MESSAGES, "%s does not wish to be disturbed.", getname_inherited(current->player));
			continue;
		}

		courtesy=generate_courtesy_string(player, current->player);
		if(*str==':')
			notify_censor_colour(current->player, player, COLOUR_TELLMESSAGES, "To %s: %s%s%s%s", courtesy, prefix, db[player].get_name(), str[1]=='\''? "":" ", str+1);
		else
			notify_censor_colour(current->player, player, COLOUR_TELLMESSAGES, "%s%s%s %s \"%s\"", prefix, db[player].get_name(), suffix, courtesy, str);
	}
}

void
Player_list::add_list(
const char *lname)
{
	if ( listcount < (MAX_LIST_TELLS -1))
		listnames[listcount++]= strdup(lname);
}

int	
Player_list::build_from_text(
dbref player,
const char *arg1)
{
	dbref	lists=find_list_dictionary(player, list_dictionary),
		element,
		target;
	char	*c;

	if (list!=NULL)
		eat_keiths_chips();

	strcpy(scratch_buffer, arg1);	/* strtok() is destructive */

	// Parse the form tell admin, luggage, friends = I love you.

	for(c=strtok(scratch_buffer, ",;"); c; c=strtok(NULL, ",;"))
	{
		char str[512], *ptr, *woo;

		/* Trim whitespace.  My variable names are the best on the planet. */

		strcpy(str, c);
		woo=str+strspn(str, " ");
		for(ptr=woo+strlen(woo)-1; ptr>=woo && isspace(*ptr); ptr--)
			*ptr='\0';

		// Are we looking at a user-defined list?
		if((*woo != '*') && (element=db[lists].exist_element(woo)))
		{
			/* Copy an entire list in. */
			char *listinfo=strdup(db[lists].get_element(element));
			ptr=listinfo;
			do
			{
				if (!find_player(atoi(ptr)))
					add_player(atoi(ptr), True);
				while(*ptr && isdigit(*ptr))
					ptr++;
				if (*ptr)
					ptr++;
			}
			while (*ptr);
			
			free(listinfo);
			char wspace[1024];
			sprintf(wspace, "<%s>", woo);
			add_list(wspace);
			continue;
		}


		if (!strcmp(woo, "me"))
		{
			include(player);
			continue;
		}

#ifdef	ANNOY_THE_ADMIN
		if (!strcmp(woo, ADMIN_SPECIFIER))
		{
			include_if_set(FLAG_WIZARD);
			include_if_set(FLAG_APPRENTICE);
			add_list("The Admin");
			continue;
		}
#endif	// ANNOY_THE_ADMIN

		if (!strcmp(woo, FRIENDS_SPECIFIER))
		{
			Player_list temp(player);
			temp.include_from_list(player, PLIST_FRIEND);
			temp.include_from_reverse_list(player, PLIST_FRIEND);
			temp.exclude_from_reverse_list(player, PLIST_FBLOCK, "!does not wish to receive 'friends' messages from you.");
			if (temp.exclude_from_reverse_list(player, PLIST_IGNORE, "!has set the 'ignore' flag on you.") > 0)
			{
				add_list("friends");
				dbref	mate= temp.get_first();
				while (mate != NOTHING)
				{
					add_player(mate, True);
					mate=temp.get_next();
				}
			}
			continue;
		}

		// Must be just a single player
		if (*woo=='*')
			woo++;
		target=match_connected_player(woo);

		if (target==AMBIGUOUS)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "The name '%s' is ambiguous. Please be more specific.", woo);
			continue;
		}
		if (target==NOTHING)
			target=lookup_player(player, woo);

		if(target!=NOTHING)
		{
			if(!find_player(target))
				add_player(target);
		}
		else
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Can't find a list or a player called \"%s\".", woo);

	}
	return count;
}

/* Not used, might as well remove warning.
 * 
 * static int singing_la_la_la_cant_hear_you(dbref player, dbref target)
 * {
 * 	dbref dict;
 * 	int element;
 * 
 * 	dict=find_list_dictionary(target, playerlist_dictionary);
 * 		return 0;
 * 	
 * 	if(!(element=db[dict].exist_element(player)))
 * 		return 0;
 * 	
 * 	if ( (atoi(db[dict].get_element(element))) & PLIST_IGNORE)
 * 		return 1;
 * 
 * 	return 0;
 * }
 */

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
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You can only have %d people in a list.", MAX_LIST_SIZE);
				victim_count=0;
				break;
			}
		}

	free(names);

	return victim_count;
}


/*
 * ladd <playerlist>			- Adds to main player list
 * ladd <listname> = <playerlist>	- Adds to custom lists
 */

void context::do_ladd(const char *arg1, const char *arg2)
{
	dbref	lists;
	char	buf[32];

	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if (in_command() && !Wizard(get_current_command()))
	{
		notify_colour(player,player, COLOUR_ERROR_MESSAGES, "You can only use 'ladd' within a command if the command has its Wizard flag set.");
		return;
	}

	if(blank(arg1))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "%%r%%hUsage:  %%wladd %%g<listname> %%r= %%g<playerlist> %%rto create/add to custom lists");
		notify_colour(player, player, COLOUR_MESSAGES, "%%r%%h -or-:  %%wladd %%g<playerlist> %%rto add to main list");
		return;
	}

	if (lookup_players_and_put_their_numbers_in_an_array(player, blank(arg2)?arg1:arg2)==0)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You didn't specify any valid player names to add to the list.");
		return;
	}

	if (blank(arg2)) 
	{
		// Add to main list
		lists=find_list_dictionary(player, playerlist_dictionary);
		char smallbuf[10];
		int  added=0;
		for(int i=0; i<victim_count; i++)
		{
			sprintf(smallbuf, "%d", victims[i]);
			if(db[lists].exist_element(smallbuf))
			{
				if (!in_command())
					notify_colour(player, player, COLOUR_MESSAGES, "%s is already on your main list.", db[victims[i]].get_name());
				continue;
			}
			db[lists].set_element(0, smallbuf, "0");
			set_reverse_map(victims[i], player, 0);
			added++;
		}
		if (added && !in_command())
			notify_colour(player,player, COLOUR_MESSAGES, "Player%s added to your main list.", (added==1)?"":"s");
		return_status=COMMAND_SUCC;
		set_return_string (ok_return_string);
		return;
	}

	// They want to add / amend a custom list

	lists=find_list_dictionary(player, list_dictionary);

	/* Does the list exist in the dictionary?  If not, create it; if so, append to it, making
	sure that we don't duplicate any of the numbers. */

	int element;
	const char *const *const colour_at=db[player].get_colour_at();
	if((element=db[lists].exist_element(arg1)))
	{
		// Copy element and leave room for char buf[] to be strcat'ed 
		char mylist[MAX_LIST_SIZE*8]; // Room for '0123456;0123457;' etc
		mylist[0]='\0';
		char *number;
		
		strcpy(mylist,db[lists].get_element(element));
		
		for(int i=0; i<victim_count; i++)
		{
			strcpy(scratch_buffer, mylist);
			for(number=strtok(scratch_buffer, ";"); number; number=strtok(NULL, ";"))
				if(atoi(number)==victims[i])
				{
					notify_colour(player, player, COLOUR_MESSAGES, "%s is already in this list.", db[victims[i]].get_name());
					break;
				}
			if(!number)
			{
				notify_public(player, player, "%sAdded %s%s %sto list %%w'%s'%s.", colour_at[COLOUR_MESSAGES], colour_at[rank_colour(victims[i])], db[victims[i]].get_name(), colour_at[COLOUR_MESSAGES], arg1, colour_at[COLOUR_MESSAGES]);
				sprintf(buf, "%s%d", *mylist? ";":"", victims[i]);
				strcat(mylist, buf);
				add_clist_reference(victims[i], player);
			}
		}
		db[lists].set_element(element, arg1, mylist);
	}
	else
	{
		*scratch_buffer='\0';

		for(int i=0; i<victim_count; i++)
		{
			notify_public(player, player, "%sAdded %s%s %sto list %%w'%s'%s.", colour_at[COLOUR_MESSAGES], colour_at[rank_colour(victims[i])], db[victims[i]].get_name(), colour_at[COLOUR_MESSAGES], arg1, colour_at[COLOUR_MESSAGES]);
			add_clist_reference(victims[i], player);
			sprintf(buf, "%s%d", i==0? "":";", victims[i]);
			strcat(scratch_buffer, buf);
		}

		db[lists].set_element(0, arg1, scratch_buffer);
	}

	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}


/*
 * llist [<listname>]
 */

void context::do_llist(const char *arg1, const char *)
{
	const char *const *const colour_at=db[player].get_colour_at();
	dbref lists;
	dbref target;
	int element;

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

	if (blank(arg1))
	{
		lists=find_list_dictionary(player, playerlist_dictionary);

		notify_colour(player, player, COLOUR_MESSAGES, "%s", "%w%hYour main player list has the following entries:");
		terminal_underline(player, squiggles);

		if (db[lists].get_number_of_elements()==0)
			notify_colour(player, player, COLOUR_MESSAGES, "     ** NO ONE LISTED **");
		else
		{
			for(int i=1; i<=db[lists].get_number_of_elements(); i++)
			{
				target=atoi(db[lists].get_index(i));
				if (Typeof(target) != TYPE_PLAYER)
					fprintf(stderr,"BUG: Non-player on player list.\n");
				else	
					notify_colour(player, player, COLOUR_MESSAGES, "     %s%s%-20s%%w%%h: %s", colour_at[rank_colour(target)], Connected(target)?"*":" ",  db[target].get_name(), playerlist_flags(atoi(db[lists].get_element(i))));

			}
		}

		terminal_underline(player, squiggles);
		notify(player, "");
		if (db[lists= find_list_dictionary(player, list_dictionary)].get_number_of_elements()==0)
			notify_colour(player, player, COLOUR_MESSAGES, "You have no custom player lists.");
		else
		{
			notify_colour(player, player, COLOUR_MESSAGES, "You have the following custom player lists:");
			*scratch_buffer='\0';
			for(int i=1; i<=db[lists].get_number_of_elements(); i++)
			{
				if (*scratch_buffer)
					strcat(scratch_buffer,", ");
				strcat(scratch_buffer, db[lists].get_index(i));
			}
			notify(player, "%%w%%h%s%%z", scratch_buffer);
			notify_colour(player,player, COLOUR_MESSAGES, "Use 'llist <listname>' to see the contents of a custom list.");
		}
		return;
	}

	lists=find_list_dictionary(player, list_dictionary);

	/* Must be asking about a named list */

	if(!(element=db[lists].exist_element(arg1)))
	{
	notify_censor(player, player, "You don't have a list called \"%s\".", arg1);
		return;
	}

	notify_censor(player, player, "Your list \"%s\" contains:", arg1);
	terminal_underline(player, squiggles);
	strcpy(scratch_buffer, db[lists].get_element(element));
	for(char *c=strtok(scratch_buffer, ";"); c; c=strtok(NULL, ";"))
	{
		if (Connected(atoi(c)))
			notify_censor(player, player, "  *%s%s", colour_at[rank_colour(atoi(c))], db[atoi(c)].get_name());
		else
			notify_censor(player, player, "   %s%s", colour_at[rank_colour(atoi(c))], db[atoi(c)].get_name());
	}
	terminal_underline(player, squiggles);
	notify(player, "");
	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}


/*
 * lremove <listname> = <playerlist>
 * lremove <playerlist>
 */

void context::do_lremove(const char *arg1, const char *arg2)
{
	dbref lists;
	int element, delete_count=0;
	dbref target;
	char smallbuf[16];
	return_status=COMMAND_FAIL;
	set_return_string (error_return_string);

	if(blank(arg1))
	{
		notify_colour(player, player, COLOUR_MESSAGES, "%%r%%hUsage:  %%wlremove %%g<listname>");
		notify_colour(player, player, COLOUR_MESSAGES, "%%r%%h -or-   %%wlremove %%g<listname> %%r= %%g<playerlist>");
		notify_colour(player, player, COLOUR_MESSAGES, "%%r%%h -or-   %%wlremove %%g<playerlist>%%z");
		return;
	}

	Player_list targets(player);
	targets.include_unconnected();

	if (blank(arg2))
	{
		// Do they want to remove a clist?

		lists=find_list_dictionary(player, list_dictionary);
		if ((element=db[lists].exist_element(arg1)))
		{
			db[lists].destroy_element(element);
			notify_censor_colour(player, player, COLOUR_MESSAGES, "List \"%s\" deleted.", arg1);
			return;
		}

		lists= find_list_dictionary(player, playerlist_dictionary);
		if (targets.build_from_text(player, arg1) ==0)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You didn't specify anybody to remove!");
			return;
		}
	}
	else
	{
		lists=find_list_dictionary(player, list_dictionary);
		if (targets.build_from_text(player, arg2) ==0)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You didn't specify anybody to remove!");
			return;
		}
	}

	if (blank(arg2))
	{
		target=targets.get_first();
		while (target != NOTHING)
		{
			sprintf(smallbuf, "%d", target);
			if ((element= db[lists].exist_element(smallbuf)))
			{
				db[lists].destroy_element(element);
				notify_censor(player, player, "%%g%%h%s%%w removed from your main list.%%z", db[target].get_name());
				delete_reverse_map(target, player);
			}
			else
				notify_censor(player, player, "%%g%%h%s%%w is not on your list.%%z", db[target].get_name());

			target= targets.get_next();
		}
		return_status=COMMAND_SUCC;
		set_return_string (ok_return_string);
		return;
	}

	/* Ok so they want to remove players from a player list */

	if(!(element=db[lists].exist_element(arg1)))
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You don't have a list called \"%s\".", arg1);
		return;
	}

	char *names=strdup(db[lists].get_element(element));
	*scratch_buffer='\0';

	for(char *c=strtok(names, ";"); c; c=strtok(NULL, ";"))
	{
		target= targets.get_first();
		while (target != NOTHING)
		{
			if (target== atoi(c))
				break;
			target=targets.get_next();
		}

		if(target == NOTHING)	/* don't delete this one */
		{
			strcat(scratch_buffer, c);
			strcat(scratch_buffer, ";");
		}
		else
		{
			notify_censor(player, player, "%%g%%h%s%%w removed from list \"%s\".%%z", db[target].get_name(), arg1);
			remove_clist_reference(target, player);
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
		notify_colour(player,player, COLOUR_MESSAGES, "All players removed - list deleted.");
		if(db[lists].get_number_of_elements()==0)
			db[lists].destroy(lists);
	}
	else
		notify_colour(player,player, COLOUR_MESSAGES, "%d player%s deleted from the list.", delete_count, PLURAL(delete_count));


	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
}

void
context::do_fwho(const char *, const char *)
{
	return_status=COMMAND_SUCC;
	set_return_string (ok_return_string);
	dbref rev_list=find_list_dictionary(player, reverseplist_dictionary);
	dbref	list=find_list_dictionary(player, playerlist_dictionary);
	int   element;
	char workspace[128];
	char smallbuf[10];
	char lt, rt;
	const char *const *const colour_at=db[player].get_colour_at();

	int ac=0,
	    target,
	    howmany,
	    blocking=0, listening=0, ignore=0, billy=0,
	    hisflags,
	    myflags;
	static char spaces[]="                     "; // Hacky but fast
	Player_list friends(player);
	friends.include_from_list(player, PLIST_FRIEND);
	friends.include_from_reverse_list(player, PLIST_FRIEND);
	friends.filter_out_if_unset(FLAG_CONNECTED);
	howmany= friends.exclude(player);
	notify(player, "The following friends are connected:");
	terminal_underline(player, squiggles);
	target=friends.get_first();
	*scratch_buffer='\0';

	while (target!=NOTHING)
	{
		sprintf(smallbuf, "%d", target);
		lt=' ';
		rt=' ';

		if ((element= db[list].exist_element(smallbuf)))
			myflags=atoi(db[list].get_element(element));
		else
			myflags=0;

		if ((element=db[rev_list].exist_element(smallbuf)))
			hisflags=atoi(db[rev_list].get_element(element));
		else
			hisflags=0;

		if (hisflags & PLIST_IGNORE)
		{
			lt='!';
			rt=' ';
			ignore++;
		}
		else if ( (hisflags & PLIST_FRIEND) && (!(myflags & PLIST_FRIEND)))
		{
			lt='[';
			rt=']';
			billy++;
		}
		else if (hisflags & PLIST_FBLOCK)
		{
			lt='#';
			rt=' ';
			blocking++;
		}
		else if (!Fchat(target))
		{
			lt='<';
			rt='>';
		}
		else
			listening++;

		sprintf(workspace, "     %%w%%h%c%s%s%%w%c", 
			lt, colour_at[rank_colour(target)], db[target].get_name(), rt);

		strncat(workspace, spaces, 24 - colour_strlen(workspace));
		strcat(scratch_buffer, workspace);
		if (++ac == 3)
		{
			ac=0;
			notify_censor(player, player, "%s", scratch_buffer);
			*scratch_buffer='\0';
		}

		target=friends.get_next();
	}
	if (*scratch_buffer)
	{
		notify_censor(player, player, "%s", scratch_buffer);
	}
	if (howmany==0)
		notify_colour(player, player, COLOUR_MESSAGES, "     ** NO FRIENDS CONNECTED, BILLY **");
	terminal_underline(player, squiggles);

	notify(player, "%d listed (%d listening, %d #blocking, %d !ignoring, %d [billy-no-mates])", howmany, listening, blocking, ignore, billy);
	notify(player, "%%z");
}

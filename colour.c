/*
 * Colour stuff, by Reaps and Flup, at some point in 1995.
 *
 * Any similarity between the colour codes herein and the ANSI
 * codes is purely intentional.
 */
/* Okay, here is what the code should do...

	When the player types @colour to add a new colour code
	it is stored in the string (for db writeout purposes).
	The changed attribute is then set on the player colour
	struct.

	When a player connects, a struct is created on them
	which contains all the colour codes for that player.
	It is a duplicate of what is in the string but
	allows faster access.

	A sorted, array of the players and their colour
	codes is built up and searched at output time
	using bsearch. It might just be an extension of
	the colour array...

	When the database is written out, it doesn't
	look at the struct, it just writes the string out.

	The colour % symbols are written out at the lowest level.
	The higher level code will just insert the appropriate
	codes out of the player colour struct and allow the
	lower levels to convert them.

	When examines are done, all %'s have to be protected
	so that people can see what is actually in the fields.
*/

#include "db.h"
#include "colour.h"
#include "context.h"
#include "command.h"
#include "externs.h"
#include "interface.h"

#include "config.h"
#include "log.h"


static	void	output_player_colours(const dbref player, const dbref victim);
static	void	output_attribute_colours(const dbref player, const dbref victim);

/* %s, %o, %p and %n are all pronouns. */

struct colour_table_type colour_table[] =

{
	/*Unparse	Ansi code	Ascii code*/
	{"Undefined",	""},		/* A */
	{"Back Blue",	"\033[44m"},	/* B */
	{"Back Cyan",	"\033[46m"},	/* C */
	{"Undefined",	""},		/* D */
	{"Undefined",	""},		/* E */
	{"Undefined",	""},		/* F */
	{"Back Green",	"\033[42m"},	/* G */
	{"Undefined",	""},		/* H */
	{"Undefined",	""},		/* I */
	{"Undefined",	""},		/* J */
	{"Back Key",	"\033[40m"},	/* K */
	{"Undefined",	""},		/* L */
	{"Back Magenta","\033[45m"},	/* M */
	{"Undefined",	""},		/* N */ /* Noun */
	{"Undefined",	""},		/* O */ /* Objective */
	{"Undefined",	""},		/* P */ /* Possesive */
	{"Undefined",	""},		/* Q */
	{"Back Red",	"\033[41m"},	/* R */
	{"Undefined",	""},		/* S */ /* Subjective */
	{"Undefined",	""},		/* T */
	{"Undefined",	""},		/* U */
	{"Undefined",	""},		/* V */
	{"Back White",	"\033[47m"},	/* W */
	{"Undefined",	""},		/* X */
	{"Back Yellow",	"\033[43m"},	/* Y */
	{"Revert",	"\033[0m", true},	/* Z */
	{"Undefined",	""},		/* [ */
	{"Undefined",	""},		/* \ */
	{"Undefined",	""},		/* ] */
	{"Undefined",	""},		/* ^ */
	{"Underline",	"\033[4m", true},	/* _ */ /* Underline */
	{"Undefined",	""},		/* ` */
	{"Undefined",	""},		/* a */
	{"Blue",	"\033[34m"},	/* b */
	{"Cyan",	"\033[36m"},	/* c */
	{"Dim",		"\033[2m", true},	/* d */
	{"Undefined",	""},		/* e */
	{"Flashing",	"\033[5m", true},	/* f */
	{"Green",	"\033[32m"},	/* g */
	{"Highlight",	"\033[1m", true},	/* h */
	{"Inverse",	"\033[7m", true},	/* i */
	{"Undefined",	""},		/* j */
	{"Black (Key)",	"\033[30m"},	/* k */
	{"Undefined",	""},		/* l */
	{"Magenta",	"\033[35m"},	/* m */
	{"Undefined",	""},		/* n */ /* Noun */
	{"Undefined",	""},		/* o */ /* Objective */
	{"Undefined",	""},		/* P */ /* Possesive */
	{"Undefined",	""},		/* q */
	{"Red",		"\033[31m"},	/* r */
	{"Undefined",	""},		/* s */ /* Subjective */
	{"Undefined",	""},		/* t */
	{"Undefined",	""},		/* u */
	{"Undefined",	""},		/* v */
	{"White",	"\033[37m"},	/* w */
	{"Undefined",	""},		/* x */
	{"Yellow",	"\033[33m"},	/* y */
	{"Revert",	"\033[0m"},	/* z */
	{"Undefined",	""},		/* { */
	{"Undefined",	""},		/* | */
	{"Undefined",	""},		/* } */
	{"CancelEffects",	"\033[m", true},	/* ~ */ /* Cancel Effects */
	{NULL,		NULL}
};

int rank_colour(dbref thing)
{
	if (thing == GOD_ID)
		return COLOUR_GOD;
	if (Wizard(thing))
		return COLOUR_WIZARDS;
	if (Apprentice(thing))
		return COLOUR_APPRENTICES;
	if (Welcomer(thing))
		return COLOUR_WELCOMERS;
	if (XBuilder(thing))
		return COLOUR_XBUILDERS;
	if (Builder(thing))
		return COLOUR_BUILDERS;
	return COLOUR_MORTALS;
}

static struct
{
	const char	*cia;	/* Colour Information Attribute */
	char		code;	/* storage code */
	const char	*fbi;	/* First Brightness Information */
} cia_table[NUMBER_OF_COLOUR_ATTRIBUTES] =
{
	{"Rooms",		'A',	"%z%h%y"},
	{"Players",		'B',	"%z%w"},
	{"Things",		'C',	"%z%g"},
	{"Npcs",		'D',	"%z%y"},
	{"Commands",		'E',	"%z%r"},
	{"Properties",		'F',	"%z%c"},
	{"Dictionaries",	'G',	"%z%c"},
	{"Arrays",		'H',	"%z%c"},
	{"Fuses",		'I',	"%z%r"},
	{"Alarms",		'J',	"%z%r"},

	{"Weapons",		'K',	"%z%w"},
	{"Armour",		'L',	"%z%w"},
	{"Ammunition",		'M',	"%z%w"},

	{"Underlines",		'N',	"%z%w"},

	{"Roomname",		'O',	"%z%h%y"},
	{"Thingname",		'P',	"%z%g"},
	{"Roomdescription",	'Q',	"%z%h%w"},
	{"Thingdescription",	'R',	"%z%w"},
	{"Playerdescription",	'S',	"%z%w"},
	{"Whostrings",		'T',	"%z%w"},
	{"Host",		'U',	"%z%w"},

	{"Says",		'V',	"%z%w"},
	{"Pages",		'W',	"%z%w"},
	{"Emotes",		'X',	"%z%w"},
	{"TellMessages",	'Y',	"%z%y"},

	{"Success",		'Z',	"%z%g"},
	{"Failure",		'[',	"%z%g"},
	{"CSuccess",		'\\',	"%z%h%r"},
	{"CFailure",		']',	"%z%h%r"},
	{"OSuccess",		'^',	"%z%h%r"},
	{"OFailure",		'_',	"%z%m"},
	{"Drop",		'`',	"%z%g"},
	{"ODrop",		'a',	"%z%m"},

	{"Mass",		'b',	"%z%w"},
	{"Volume",		'c',	"%z%w"},

	{"Connects",		'd',	"%h%c"},
	{"Disconnects",		'e',	"%h%c"},
	{"Boots",		'f',	"%h%r"},

	{"Shouts",		'g',	"%z%r%h"},
	{"Natters",		'h',	"%z%h%w"},
	{"NatterTitles",	'i',	"%z%h%g"},


	{"Whispers",		'j',	"%z%w"},
	{"Exits",		'k',	"%z%c"},
	{"ContentsStrings",	'l',	"%z%g%h"},

	{"Wizards",		'm',	"%z%y"},
	{"Apprentices",		'n',	"%z%g"},
	{"Builders",		'o',	"%z%w"},
	{"Mortals",		'p',	"%z%w"},
	{"XBuilders",		'q',	"%z%m"},
	{"Welcomers",		'r',	"%z%m"},
	{"God",			's',	"%z%h%r"},

	{"Errors",		't',	"%z%h%r"},
	{"Messages",		'u',	"%z%c"},

	{"LastConnected",	'v',	"%z%y"},
	{"TotalConnected",	'w',	"%z%y"},

	{"Titles",		'x',	"%z%h%r"},
	{"Tracing",		'y',	"%z%g"},

	{"Descriptions",	'z',	"%z%w"},

	{"ChannelNames",	'{',	"%z%h%y"},
	{"ChannelMessages",	'|',	"%z%w"},
	{"Tells",		'}',	"%z%c"},

	{"WelcomerTitles",	'~',	"%g%h"},

	{NULL,		0}
};

colour_at	default_colour_at;	// This is the default used in db.h

static char *find_cia(const char *colour_string, char* cia)
{
	char *ptr;

	if(blank(colour_string))
		return(NULL);
	if(!(ptr=strstr(colour_string, cia)))
		return(NULL);
	
	return ptr;
}

#ifdef DEBUG_COLOUR
static void
output_array(cplay * array, int size)
{
	log_debug("size of array: %d", size);
	for(int x=0;x<size;x++)
	{
		log_debug("element [%d]: %s", x, array[x]/output_string);
	}
}
#endif

void
context::do_at_listcolours(
const	String& target,
const	String& type)

{
	dbref victim; // For Wizards

	set_return_string (ok_return_string);
	return_status=COMMAND_SUCC;
	
	victim = player;	// Default to listing current player

	if((target && *target.c_str()) && !string_prefix(target, "players"))
	{
		if((victim = lookup_player(player, target)) == NOTHING)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "No such player");
			set_return_string(error_return_string);
			return_status = COMMAND_FAIL;
			return;
		}
		if (!controls_for_read(victim))
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, permission_denied.c_str());
			set_return_string(error_return_string);
			return_status = COMMAND_FAIL;
			return;
		}

		if(type && *type.c_str() && string_prefix(type, "players"))
			output_player_colours(player, victim);
		else
			output_attribute_colours(player, victim);
	}
	else
	{
		if(target && *target.c_str() && string_prefix(target, "players"))
			output_player_colours(player, victim);
		else
			output_attribute_colours(player, victim);
	}

}


static void
output_attribute_colours (
const	dbref	player,
const	dbref	victim)

{
	String		colour_string;
	char	search_buf[3];
	char	colour_store[BUFFER_LEN];
	char	*cia_store;
	int	header = 0;
	int	x = 0;
	int	comma = 0;

	colour_string = db[victim].get_colour();

	search_buf[0] = ' ';
	search_buf[2] = '\0';

	while(cia_table[x].cia)
	{
		if(cia_table[x].cia)
		{
			search_buf[1] = cia_table[x].code;
			cia_store = find_cia(colour_string.c_str(), search_buf);
			if(cia_store)
			{
				if(!header)
				{
					header = 1;
					notify_colour(player, player, COLOUR_CONTENTS, "\nAttribute           Set colour(s)\n---------           -------------");
				}

				cia_store+=2;

				colour_store[0] = '\0';
				comma = 0;

				while((*cia_store != ' ') && *cia_store)
				{
					if(*cia_store == '%')
					{
						cia_store++;
						continue;
					}
					else
					{
						if(comma)
							strcat(colour_store, ", ");
						else
							comma = 1;

						strcat(colour_store, colour_table[*cia_store - 65].name);
						cia_store++;
					}
				}
				notify_colour(player, player, COLOUR_MESSAGES, "%-20.20s%s", cia_table[x].cia, colour_store);
			}
		}
		x++;
	}
	if(header == 0)
	{
		if(player == victim)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "None of your colour attributes have been set");
		}
		else
		{
			notify_colour(player, player, COLOUR_MESSAGES, "None of %s's colour attributes have been set", db[victim].get_name().c_str());
		}
	}
}

static void output_player_colours(
const dbref player,
const dbref victim
)
{
	const	cplay	*cplay_store = db[victim].get_colour_play();
		char	colour_store[BUFFER_LEN];
	const	char	*cia_store;
		int	header = 0;
		int	comma = 0;
		int	no_of_players = db[victim].get_colour_play_size();
		int	player_count = 0;

	if(no_of_players == 0)
	{
		if(player == victim)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "You have no colours set for any players");
		}
		else
		{
			notify_colour(player, player, COLOUR_MESSAGES, "%s has no colours set for any players", db[victim].get_name().c_str());
		}
		return;
	}

	while(no_of_players - player_count)
	{
		if(!header)
		{
			header = 1;
				notify_colour(player, player, COLOUR_CONTENTS, "\nPlayer                 Set colour(s)\n---------              -------------");
		}

		colour_store[0] = '\0';
		comma = 0;
		cia_store = cplay_store[player_count].output_string.c_str();

		while((*cia_store != ' ') && *cia_store)
		{
			if(*cia_store == '%')
			{
				cia_store++;
				continue;
			}
			else
			{
				if(comma)
					strcat(colour_store, ", ");
				else
					comma = 1;

				strcat(colour_store, colour_table[*cia_store - 65].name);
				cia_store++;
			}
		}
		notify_colour(player, player, COLOUR_MESSAGES, "%-23.23s%s",
			db[cplay_store[player_count].player].get_name().c_str(),
			colour_store);
		player_count++;
	}
}

void
context::do_at_colour (
const	String& cia,
const	String& colour_codes)

{
		int	i;
		char	*ptr;
		char	*endptr;
		char	new_colour[2048];
	const	char	*colour_string;
		dbref	colour_player = NOTHING;
		char	little_buffer[16];

	set_return_string (error_return_string);
	return_status=COMMAND_FAIL;

	if(!cia)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@colour requires a first argument (type 'help @colour'.)");
		return;
	}



	for(i=0; cia_table[i].cia; i++)
		if(string_prefix(cia_table[i].cia, cia)!=0)
			break;

	/* If this has failed check for player name/id */
	
	if(!cia_table[i].cia)
	{
		String target = cia;
		if(target.c_str()[0] == '*')
		{
			target=cia.c_str()+1;
		}
		if((colour_player = lookup_player(player,target)) == NOTHING)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "There isn't an attribute or player called \"%s\".", cia.c_str());
			return;
		}
		if(colour_player == player)
		{
			notify_colour(player, player, COLOUR_MESSAGES, "Warning: Setting a colour attribute on yourself will affect lots of output.");
		}
	}


	/* This has gone in here because we need to pass messages
	   to the player if the codes they gave us are wrong and
	   there is no easy way of testing them without doing
	   the whole test twice */

	colour_string = db[player].get_colour().c_str();

	/* We trust the CIA (probably foolish). */

	/* The cia we have been passed has been removed now */
	/* Now we either set it or add it back onto the end */
	if(colour_codes)
	{
		/* Check for the colour code length */
		if(colour_codes.length() > 64)
		{
			notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, there were too many colour settings\n");
			return;
		}

		/* Validate the CIA */
		/* Now we need to validate the colour_codes and add
		   them to the new_colour string */
	
		int x = 0;

		while(colour_codes.c_str()[x])
		{
			if((colour_codes.c_str()[x] != '%') || (colour_codes.c_str()[x+1] == '\0'))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Colour string incorrectly formatted (type 'help @colour')");
				return;
			}
			x++;
			if((colour_codes.c_str()[x] < 'A') || (colour_codes.c_str()[x] > 'z'))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The colour code '%%%c' is not valid (type 'help @colour')",colour_codes.c_str()[x]);
				return;
			}
			x++;
		}
	}


	/* Look for the CIA code in the string */
	new_colour[0] = '\0';
	if(colour_player == NOTHING)
		sprintf(little_buffer, " %c", cia_table[i].code);
	else
		sprintf(little_buffer, " %d%%", colour_player);

	if((ptr=find_cia(colour_string, little_buffer)) == NULL)
	{
		if(colour_string != NULL)
			strcpy(new_colour, colour_string);
	}
	else
	{
		strncpy(new_colour, colour_string, (int)(ptr-colour_string));
		new_colour[ptr-colour_string] = '\0';
		if((endptr = strchr(ptr+1,' ')) != NULL)
			strcat(new_colour, endptr);
	}

	if(colour_codes)
	{
		if(colour_player == NOTHING)
			sprintf(new_colour+strlen(new_colour), " %c%s",cia_table[i].code, colour_codes.c_str());
		else
			sprintf(new_colour+strlen(new_colour), " %d%s",colour_player, colour_codes.c_str());
	}
	

	if(strlen(new_colour) > 1024)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Sorry, your colour information has grown too large. Please see a wizard.");
		notify_wizard("%s's colour string has grown too long. Could be trying to crash it.", player);
		return;
	}

	/* Set the storage string on the player */
	db[player].set_colour(new_colour);
#ifdef DEBUG_COLOUR
	log_debug("colour string is: %s", new_colour);
#endif

	/* Update the memory, colour array */
	/* We delete the whole array and make a new one
	   so that we know what attributes to override */
	db[player].set_colour_at (new colour_at(new_colour));

	/* And do the same for the player colour array */
#ifdef DEBUG_COLOUR
	output_array(db[player].get_colour_play(),
		db[player].get_colour_play_size());
#endif
	db[player].set_colour_play(make_colour_play(player, new_colour));

	if(colour_player ==NOTHING)
		notify_colour(player, player, COLOUR_MESSAGES, "Attribute \"%s\" set%s.", cia_table[i].cia, (colour_codes)?"":" to default");
	else
		notify_colour(player, player, COLOUR_MESSAGES, "Colour for \"%s\" set%s.", db[colour_player].get_name().c_str(), (colour_codes)?"":" to default");

	set_return_string (ok_return_string);
	return_status=COMMAND_SUCC;
}

const int
find_number_of_players (
const String& cs)

{
	int 	count = 0;
	const char* colour_string = cs.c_str();

	while (colour_string)
	{
		colour_string = strchr(colour_string, ' ');
		if (colour_string)
		{
			if ((colour_string[1] < 58) && (colour_string[1] > 47))
				count++;
			colour_string++;
		}
	}

	return count;
}


colour_at::colour_at (
const String&	colour_string)

{
	int	x = 0;
	char*	store;
	char*	store_end;
	char	search_buf[3] = "  ";
	
	/* This while loop will set the colour attributes (underlines, rooms etc) */
	while(cia_table[x].cia)
	{
		search_buf[1] = x+65;
		store = find_cia(colour_string.c_str(), search_buf);

		if((store == NULL) || (!colour_string) || (colour_string.c_str()[0] == '\0'))
			colours[x] = cia_table[x].fbi;
		else
		{
			store+=2;
			store_end = strchr(store, ' ');
			if(store_end == NULL)
			{
				/* We're at the end of the colour string */
				colours[x] = store;
			}
			else
			{
				*store_end = 0;
				colours[x] = store;
				*store_end = ' ';
			}
		}
		x++;
	}
}


int
compare (
const	void	*a,
const	void	*b)
{
	return(((const cplay *)a)->player - ((const cplay *)b)->player);
}


cplay *
make_colour_play (
const	dbref		player,
const String&	cs)

{
	if((!cs) || (cs.c_str()[0] == '\0'))
		return(NULL);

	char*	colour_string = strdup (cs.c_str());
	char	*point = colour_string;
	char	*begin;
	int	count=0;
	int	no_of_players;
	cplay	 *return_array;

	/* Find out how many players there are so we can calloc enough space */
	no_of_players = find_number_of_players(colour_string);
	db[player].set_colour_play_size(no_of_players);

	/* Create the array that the player will use */
	return_array = new cplay[no_of_players];

	/* Now we add a list of players and their colours */
	while(point)
	{
		point++;
		if((*point < 58) && (*point > 47))
		{
			/* Add it to the array, since it
			   starts with a decimal digit */
			sscanf(point, "%d", &return_array[count].player);
			begin = strchr(point, '%');
			point = strchr(point, ' ');
			if(point)
				*point = '\0';
			return_array[count].output_string = begin;
			count++;
		}
		else
			point = strchr(point, ' ');

	}


	/*Free the copy of the colour string we made*/
	free(colour_string);

	/* sort the array so that we can bsearch it */
	qsort(return_array, no_of_players, sizeof(cplay), &compare);

	return (return_array);
}


/*
 * Returns the colour string for a given player
 */

const char*
player_colour (
dbref	player,
dbref	victim,
int	colour)

{
		cplay	*item=NULL;
		cplay	tester;
	const	cplay	*colour_play = db[player].get_colour_play();

	tester.player = victim;

	if (colour_play)
	{
		if ((item = (cplay *)bsearch(&victim,
			colour_play,
			db[player].get_colour_play_size(),
			sizeof(cplay),
			&compare)) != 0)
			return(item->output_string.c_str());
	}

	if(colour == NO_COLOUR)
		return "";
	else
		return(db[player].get_colour_at()[colour]);
}


void
context::do_query_colour(
const	String& arg1,
const	String& arg2)

{
	/* @?colour <player>
	   @?colour <attribute>
	   @?colour <player> = <player>
	   @?colour <player> = <attribute>
	*/
		dbref	victim;
		dbref	colour_player = NOTHING;
		int	i;
	const colour_at& ca = db[player].get_colour_at();

	set_return_string (error_return_string);
	return_status=COMMAND_FAIL;
	
	if(!arg1)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "You must give an attribute or a player. Type 'help @?colour' for help");
		return;
	}

	if(!arg2)
	{
		for(i=0; cia_table[i].cia; i++)
			if(string_prefix(cia_table[i].cia, arg1)!=0)
				break;

		if(!cia_table[i].cia)
			if((colour_player = lookup_player(player,arg1)) == NOTHING)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't an attribute or player called \"%s\".", arg1.c_str());
				return;
			}

		if(colour_player !=NOTHING)
			set_return_string (player_colour(player, colour_player, NO_COLOUR));
		else
			set_return_string (ca[i]);
	}
	else
	{
		if((victim = lookup_player(player,arg1)) == NOTHING)
		{
		   notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't a player called \"%s\".", arg1.c_str());
			return;
		}

		for(i=0; cia_table[i].cia; i++)
			if(string_prefix(cia_table[i].cia, arg2)!=0)
				break;

		if(!cia_table[i].cia)
			if((colour_player = lookup_player(player,arg2)) == NOTHING)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't an attribute or player called \"%s\".", arg2.c_str());
				return;
			}

		if(colour_player !=NOTHING)
			set_return_string (player_colour(player, colour_player, NO_COLOUR));
		else 
			set_return_string (ca[i]);
	}

	return_status=COMMAND_SUCC;
}

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

#include <ctype.h>
#include "db.h"
#include "colour.h"
#include "context.h"
#include "command.h"
#include "externs.h"
#include "interface.h"

#include "objects.h"

#include "config.h"
#include "log.h"

#define countof(x) (sizeof(x) / sizeof(x[0]))

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
	{"CancelEffects",	"\033[m", true}	/* ~ */ /* Cancel Effects */
};

ColourAttribute rank_colour(dbref thing)
{
	if (Retired(thing))
		return COLOUR_RETIRED;
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
	String		cia;	/* Colour Information Attribute */
	char		code;	/* storage code */
	String		fbi;	/* First Brightness Information */
} cia_table[] =
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

	{"Retired",		'\177',	"%z%b%h"},

	{"Timestamps",		'\200',	"%z%y"},
};

colour_at	default_colour_at;	// This is the default used in db.h
colour_pl	default_colour_players;

void
context::do_at_listcolours(
const	String& target,
const	String& type)

{
	dbref victim; // For Wizards

	set_return_string (ok_return_string);
	return_status=COMMAND_SUCC;
	
	victim = player;	// Default to listing current player

	if(target && !string_prefix(target, "players"))
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

		if(type && string_prefix(type, "players"))
			output_player_colours(player, victim);
		else
			output_attribute_colours(player, victim);
	}
	else
	{
		if(target && string_prefix(target, "players"))
			output_player_colours(player, victim);
		else
			output_attribute_colours(player, victim);
	}

}

String unparse_colour_string(const String& colours)
{
	String retval;

	const char* c = colours.c_str();

	while(*c)
	{
		if(*c != '%')
		{
			int i = (*c - 'A');
			if((i >= 0) && (i < (int)countof(colour_table)))
			{
				if(retval)
					retval += ", ";
				retval += colour_table[i].name;
			}
		}
		c++;
	}

	return retval;
}

static void
output_attribute_colours (
const	dbref	player,
const	dbref	victim)

{
	String		colour_string;
	String	colour_store;
	bool	header = false;

	const colour_at& colour_table= db[victim].get_colour_at();

	for(unsigned int i = 0; i < countof(cia_table); i++)
	{
		const String c = colour_table[(ColourAttribute)i];
		if(c != default_colour_at.get_colour((ColourAttribute)i))
		{
			if(!header)
			{
				header = true;
				notify_colour(player, player, COLOUR_CONTENTS, "Attribute           Set colour(s)\n---------           -------------");
			}

			notify_colour(player, player, COLOUR_MESSAGES, "%-20.20s %s", cia_table[i].cia.c_str(), unparse_colour_string(c).c_str());
		}
	}

	if(!header)
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
	const	colour_pl& cplay = db[victim].get_colour_players();
	String	colour_store;
	bool	header = false;

	colour_pl::const_iterator it;
	for(it = cplay.begin(); it != cplay.end(); it++)
	{
		if(!header)
		{
			header = true;
			notify_colour(player, player, COLOUR_CONTENTS, "Player                 Set colour(s)\n---------              -------------");
		}

		notify_colour(player, player, COLOUR_MESSAGES, "%-23.23s%s",
			db[it->first].get_name().c_str(),
			unparse_colour_string(it->second).c_str());
	}

	if(!header)
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
}

void
context::do_at_colour (
const	String& cia,
const	String& colour_codes)

{
	int	i;
	dbref	colour_player = NOTHING;

	set_return_string (error_return_string);
	return_status=COMMAND_FAIL;

	if(!cia)
	{
		notify_colour(player, player, COLOUR_ERROR_MESSAGES, "@colour requires a first argument (type 'help @colour'.)");
		return;
	}

	for(i=COLOUR_FIRSTCOLOUR; i < COLOUR_LASTCOLOUR; i++)
		if(string_prefix(cia_table[i].cia, cia)!=0)
			break;

	/* If this has failed check for player name/id */

	if(i >= COLOUR_LASTCOLOUR)
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

		const char* cstr = colour_codes.c_str();
		while(cstr[x])
		{
			if((cstr[x] != '%') || (cstr[x+1] == '\0'))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "Colour string incorrectly formatted (type 'help @colour')");
				return;
			}
			x++;
			if((cstr[x] < 'A') || (cstr[x] > 'z'))
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "The colour code '%%%c' is not valid (type 'help @colour')",colour_codes.c_str()[x]);
				return;
			}
			x++;
		}
	}


	if(colour_player != NOTHING)
	{
		db[player].set_colour_player(colour_player, colour_codes);
		notify_colour(player, player, COLOUR_MESSAGES, "Colour for \"%s\" set%s.", db[colour_player].get_name().c_str(), (colour_codes)?"":" to default");
	}
	else
	{
		db[player].set_colour_attr((ColourAttribute)i, colour_codes);
		notify_colour(player, player, COLOUR_MESSAGES, "Attribute \"%s\" set%s.", cia_table[i].cia.c_str(), (colour_codes)?"":" to default");
	}

	set_return_string (ok_return_string);
	return_status=COMMAND_SUCC;
}

colour_at::colour_at()
{
	for(int i = 0; i < COLOUR_LASTCOLOUR; i++)
	{
		colours[i] = cia_table[i].fbi;
	}
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
	unsigned int	i;
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
		for(i = 0; i < countof(cia_table); i++)
			if(string_prefix(cia_table[i].cia, arg1)!=0)
				break;

		if(i == countof(cia_table))
			if((colour_player = lookup_player(player,arg1)) == NOTHING)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't an attribute or player called \"%s\".", arg1.c_str());
				return;
			}

		if(colour_player !=NOTHING)
			set_return_string (player_colour(player, colour_player, NO_COLOUR));
		else
			set_return_string (ca[(ColourAttribute)i]);
	}
	else
	{
		if((victim = lookup_player(player,arg1)) == NOTHING)
		{
		   notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't a player called \"%s\".", arg1.c_str());
			return;
		}

		for(i = 0; i < countof(cia_table); i++)
			if(string_prefix(cia_table[i].cia, arg2)!=0)
				break;

		if(i == countof(cia_table))
			if((colour_player = lookup_player(player,arg2)) == NOTHING)
			{
				notify_colour(player, player, COLOUR_ERROR_MESSAGES, "There isn't an attribute or player called \"%s\".", arg2.c_str());
				return;
			}

		if(colour_player !=NOTHING)
			set_return_string (player_colour(player, colour_player, NO_COLOUR));
		else 
			set_return_string (ca[(ColourAttribute)i]);
	}

	return_status=COMMAND_SUCC;
}

void
Player::set_colour_string(const String& colours)
{
	const char* c = colours.c_str();
	while(*c)
	{
		while(*c && (*c == ' '))
			c++;
		if(!*c)
			break;
		if(isdigit(*c))
		{
			// Player id.
			dbref id = atoi(c);
			while(isdigit(*c))
				c++;
			String str;
			while(*c && (*c != ' '))
				str += *(c++);
			if(id > 0) // Can't check for valid players yet, because they might not be loaded.
				set_colour_player(id, str);
		}
		else
		{
			// Colour code.
			char id = *c++;
			String str;
			while(*c && (*c != ' '))
				str += *(c++);
			for(unsigned int i = 0; i < countof(cia_table); i++)
			{
				if(cia_table[i].code == id)
				{
					set_colour_attr((ColourAttribute)i, str);
					break;
				}
			}
		}
	}
}

String
Player::get_colour_string() const
{
	String retval;

	char tmp[2048];

	// Get player colour string.
	for(colour_pl::const_iterator it = get_colour_players().begin();
			it != get_colour_players().end();
			it++)
	{
		dbref player = it->first;

		if((player >= 0) && (player < db.top()) && (Typeof(player) == TYPE_PLAYER))
		{
			sprintf(tmp, " %d%s", it->first, it->second.c_str());
			retval += tmp;
		}
	}

	// Now add in the colour attributes.
	for(int i = 0; i < COLOUR_LASTCOLOUR; i++)
	{
		if(colour_attrs.get_colour((ColourAttribute)i) != cia_table[i].fbi)
		{
			retval += " ";
			retval += cia_table[i].code;
			retval += colour_attrs.get_colour((ColourAttribute)i);
		}
	}

	return retval;
}

void
Player::set_colour_attr(ColourAttribute a, const String& col)
{
	if(col)
	{
		colour_attrs.set_colour(a, col);
	}
	else
	{
		colour_attrs.set_colour(a, cia_table[a].fbi);
	}
}

const String&
Player::get_colour_attr(ColourAttribute a) const
{
	return colour_attrs.get_colour(a);
}

void
Player::set_colour_player(dbref player, const String& col)
{
	if(col)
	{
		colour_players[player] = col;
	}
	else
	{
		colour_players.erase(player);
	}
}

const String&
Player::get_colour_player(dbref victim) const
{
	colour_pl::const_iterator it = colour_players.find(victim);
	if(it == colour_players.end())
	{
		return NULLSTRING;
	}
	return it->second;
}

const char*
player_colour(dbref player, dbref victim, ColourAttribute colour)
{
	const String& s = db[player].get_colour_player(victim);
	if(s)
	{
		return s.c_str();
	}
	return db[player].get_colour_attr(colour).c_str();
}


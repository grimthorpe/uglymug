#ifndef COLOUR_H_INCLUDED
#define COLOUR_H_INCLUDED

#include "mudstring.h"
#include "db.h"

/*
 * The string to revert back to normal.
 */
#define COLOUR_REVERT			"%z"

/*
 * These #defines _MUST_ be kept in track with the cia_table
 * in colour.c - order is significant!
 * These values are used to index directly into the array stored
 * on the connected players. They are also used to indicate
 * to queue_string what is being output.
 */

enum ColourAttribute
{
	NO_COLOUR		= -1,
	COLOUR_FIRSTCOLOUR	= 0,
	COLOUR_ROOMS		= 0,
	COLOUR_PLAYERS		= 1,
	COLOUR_THINGS		= 2,
	COLOUR_NPCS		= 3,
	COLOUR_COMMANDS		= 4,
	COLOUR_PROPERTIES	= 5,
	COLOUR_DICTIONARIES	= 6,
	COLOUR_ARRAYS		= 7,
	COLOUR_FUSES		= 8,
	COLOUR_ALARMS		= 9,

	COLOUR_UNUSED1		= 10,
	COLOUR_UNUSED2		= 11,
	COLOUR_UNUSED3		= 12,

	COLOUR_UNDERLINES	= 13,

	COLOUR_ROOMNAME		= 14,
	COLOUR_THINGNAME	= 15,
	COLOUR_ROOMDESCRIPTION	= 16,
	COLOUR_THINGDESCRIPTION	= 17,
	COLOUR_PLAYERDESCRIPTION	= 18,
	COLOUR_WHOSTRINGS	= 19,
	COLOUR_HOST		= 20,

	COLOUR_SAYS		= 21,
	COLOUR_PAGES		= 22,
	COLOUR_EMOTES		= 23,
	COLOUR_TELLMESSAGES	= 24,

	COLOUR_SUCCESS		= 25,
	COLOUR_FAILURE		= 26,
	COLOUR_CSUCCESS		= 27,
	COLOUR_CFAILURE		= 28,
	COLOUR_OSUCCESS		= 29,
	COLOUR_OFAILURE		= 30,
	COLOUR_DROP		= 31,
	COLOUR_ODROP		= 32,

	COLOUR_MASS		= 33,
	COLOUR_VOLUME		= 34,

	COLOUR_CONNECTS		= 35,
	COLOUR_DISCONNECTS	= 36,
	COLOUR_BOOTS		= 37,

	COLOUR_SHOUTS		= 38,
	COLOUR_NATTERS		= 39,
	COLOUR_NATTER_TITLES	= 40,

	COLOUR_WHISPERS		= 41,
	COLOUR_EXITS		= 42,
	COLOUR_CONTENTS		= 43,

	COLOUR_WIZARDS		= 44,
	COLOUR_APPRENTICES	= 45,
	COLOUR_BUILDERS		= 46,
	COLOUR_MORTALS		= 47,
	COLOUR_XBUILDERS	= 48,
	COLOUR_WELCOMERS	= 49,
	COLOUR_GOD		= 50,

	COLOUR_ERROR_MESSAGES	= 51,
	COLOUR_MESSAGES		= 52,

	COLOUR_LAST_CONNECTED	= 53,
	COLOUR_TOTAL_CONNECTED	= 54,

	COLOUR_TITLES		= 55,
	COLOUR_TRACING		= 56,

	COLOUR_DESCRIPTIONS	= 57,

	COLOUR_CHANNEL_NAMES	= 58,
	COLOUR_CHANNEL_MESSAGES	= 59,

	COLOUR_TELLS		= 60,
	COLOUR_WELCOMER_TITLES	= 61,

	COLOUR_RETIRED		= 62,

	COLOUR_TIMESTAMPS	= 63,

	// Put all colour numbers above this point.
	COLOUR_LASTCOLOUR
};

struct colour_table_type
{
	const String	name;
	const String	ansi;
	bool		is_effect;	// Set true if not really a colour, but something like highlight
};

extern struct colour_table_type	colour_table[];

typedef std::map<dbref, String>	colour_pl;

class colour_at
{
	String colours[COLOUR_LASTCOLOUR];

	colour_at(const colour_at&);
	colour_at& operator=(const colour_at& c);
public:
	colour_at();

	const char* operator[](ColourAttribute i) const
	{
		if(i < COLOUR_LASTCOLOUR)
		{
			return colours[i].c_str();
		}
		return "";
	}
	void set_colour(ColourAttribute i, const String& c)
	{
		colours[i] = c;
	}

	const String& get_colour(ColourAttribute i) const
	{
		return colours[i];
	}
};

extern colour_at default_colour_at;
extern colour_pl default_colour_players;
#endif


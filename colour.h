#ifndef COLOUR_H_INCLUDED
#define COLOUR_H_INCLUDED

#include "mudstring.h"

/* The next number is the *MAXIMUM* number of colour attributes in
 * the cia_table. Remember that arrays start at index 0!
 */
#define NUMBER_OF_COLOUR_ATTRIBUTES	70 /* Make room for expansion */
//#define COLOUR_REVERT			"\033[0m"
#define COLOUR_REVERT			"%z"

/*
 * These #defines _MUST_ be kept in track with the cia_table
 * in colour.c - order is significant!
 * These values are used to index directly into the array stored
 * on the connected players. They are also used to indicate
 * to queue_string what is being output.
 */

#define NO_COLOUR		-1
#define COLOUR_ROOMS		0
#define COLOUR_PLAYERS		1
#define COLOUR_THINGS		2
#define COLOUR_NPCS		3
#define COLOUR_COMMANDS		4
#define COLOUR_PROPERTIES	5
#define COLOUR_DICTIONARIES	6
#define COLOUR_ARRAYS		7
#define COLOUR_FUSES		8
#define COLOUR_ALARMS		9

#define COLOUR_UNUSED1		10
#define COLOUR_UNUSED2		11
#define COLOUR_UNUSED3		12

#define COLOUR_UNDERLINES	13

#define COLOUR_ROOMNAME		14
#define COLOUR_THINGNAME	15
#define COLOUR_ROOMDESCRIPTION	16
#define COLOUR_THINGDESCRIPTION	17
#define COLOUR_PLAYERDESCRIPTION	18
#define COLOUR_WHOSTRINGS	19
#define COLOUR_HOST		20

#define COLOUR_SAYS		21
#define COLOUR_PAGES		22
#define COLOUR_EMOTES		23
#define COLOUR_TELLMESSAGES	24

#define COLOUR_SUCCESS		25
#define COLOUR_FAILURE		26
#define COLOUR_CSUCCESS		27
#define COLOUR_CFAILURE		28
#define COLOUR_OSUCCESS		29
#define COLOUR_OFAILURE		30
#define COLOUR_DROP		31
#define COLOUR_ODROP		32

#define COLOUR_MASS		33
#define COLOUR_VOLUME		34

#define COLOUR_CONNECTS		35
#define COLOUR_DISCONNECTS	36
#define COLOUR_BOOTS		37

#define COLOUR_SHOUTS		38
#define COLOUR_NATTERS		39
#define COLOUR_NATTER_TITLES	40

#define COLOUR_WHISPERS		41
#define COLOUR_EXITS		42
#define COLOUR_CONTENTS		43

#define COLOUR_WIZARDS		44
#define COLOUR_APPRENTICES	45
#define COLOUR_BUILDERS		46
#define COLOUR_MORTALS		47
#define COLOUR_XBUILDERS	48
#define COLOUR_WELCOMERS	49
#define COLOUR_GOD		50

#define COLOUR_ERROR_MESSAGES	51
#define COLOUR_MESSAGES		52

#define COLOUR_LAST_CONNECTED	53
#define COLOUR_TOTAL_CONNECTED	54

#define COLOUR_TITLES		55
#define COLOUR_TRACING		56

#define COLOUR_DESCRIPTIONS	57

#define COLOUR_CHANNEL_NAMES	58
#define COLOUR_CHANNEL_MESSAGES	59

#define COLOUR_TELLS		60
#define	COLOUR_WELCOMER_TITLES	61

#define	COLOUR_RETIRED		62

#define	COLOUR_TIMESTAMPS	63

struct colour_table_type
{
	const char	*name;
	const char	*ansi;
	bool		is_effect;	// Set true if not really a colour, but something like highlight
};

extern struct colour_table_type	colour_table[];


struct cplay
{
	cplay() : player(-1), output_string() {}
		int		player;
		String		output_string;
};

class colour_at
{
	String colours[NUMBER_OF_COLOUR_ATTRIBUTES];
	colour_at(const colour_at&);
	colour_at& operator=(const colour_at& c);
public:
	colour_at() {}
	colour_at(const String&);

	const char* operator[](unsigned int i) const
	{
		if(i < NUMBER_OF_COLOUR_ATTRIBUTES)
		{
			return colours[i].c_str();
		}
		return "";
	}

};

extern colour_at default_colour_at;

#endif


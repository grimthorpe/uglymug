#include "copyright.h"
#ifndef _DB_H
#define _DB_H
#pragma interface
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "mudstring.h"
#include "config.h"
#include "colour.h"

#ifdef DBREF_BUG_HUNTING
class dbref
{
private:
	int	_ref;
	void	assign(int i);
public:
	dbref(int i = -1) { assign(i); }
	operator int() const { return _ref; }
	dbref& operator=(int i) { assign(i); return *this; }

	dbref operator++() { return dbref(++_ref); }
	dbref operator++(int) { return dbref(_ref++); }

	bool operator<(const dbref& other) const { return _ref < other._ref; }
	bool operator<(const int other) const { return _ref < other; }
	bool operator>(const dbref& other) const { return _ref > other._ref; }
	bool operator>(const int other) const { return _ref > other; }
	bool operator==(const dbref& other) const { return _ref == other._ref; }
	bool operator==(const int other) const { return _ref == other; }
};
#else // DBREF_BUG_HUNTING
typedef	int	dbref;		/* offset into db */
#endif // DBREF_BUG_HUNTING
typedef int	typeof_type;
typedef unsigned char	flag_type;
typedef int object_flag_type;

class	Channel;

#define FLAGS_WIDTH 8

/*These defines will be used to store the flags in the database
  so they must NOT be changed without a db trawl or a special
  load function*/

/* Type definitions, TYPE_FREE musn't be 0 since it is used
   to indicate whether or not the member of the array has
   an object associated with it */

#define TYPE_NO_TYPE			1
#define TYPE_FREE			2

#define	TYPE_PLAYER			3
#define	TYPE_PUPPET			4

#define TYPE_ROOM			5
#define TYPE_THING			6
#define TYPE_EXIT			7

#define TYPE_COMMAND			8

#define TYPE_VARIABLE			9
#define TYPE_PROPERTY			10
#define TYPE_ARRAY			11
#define TYPE_DICTIONARY			12

#define TYPE_ALARM			13
#define TYPE_FUSE			14

#define TYPE_TASK			15
#define TYPE_SEMAPHORE			16

#define TYPE_WEAPON			17
#define TYPE_ARMOUR			18
#define TYPE_AMMUNITION			19


/* Flag definitions */
#define FLAG_WIZARD			1
#define FLAG_APPRENTICE 		2
#define FLAG_BUILDER			3
#define FLAG_LISTEN			4
#define FLAG_CONNECTED			5
#define FLAG_ERIC			6

/* If these are both unset it is unassigned. If they 
   are both set then it is Neuter */
#define FLAG_MALE			7
#define FLAG_FEMALE			8
#define FLAG_NEUTER			9

#define FLAG_DARK			10
#define FLAG_SILENT			11
#define FLAG_STICKY			12
#define FLAG_VISIBLE			13
#define FLAG_OPENABLE			14
#define FLAG_OPEN			15
#define FLAG_OPAQUE			16
#define FLAG_INHERITABLE		17
#define FLAG_LOCKED			18
#define FLAG_CHOWN_OK			19
#define FLAG_FIGHTING			20
#define FLAG_HAVEN			21
#define FLAG_TRACING			22
#define FLAG_NUMBER			23

#define FLAG_ABORT			24
#define FLAG_TOM			25

#define	FLAG_ASHCAN			26
#define FLAG_REFERENCED			27
#define FLAG_READONLY			28

#define FLAG_ARTICLE_NOUN		29	/* This will be the default */
#define FLAG_ARTICLE_PLURAL		30
#define FLAG_ARTICLE_SINGULAR_VOWEL	31
#define FLAG_ARTICLE_SINGULAR_CONS	32

#define	FLAG_ABODE_OK			33
#define	FLAG_LINK_OK			34
#define	FLAG_JUMP_OK			35

#define FLAG_COLOUR			36
#define FLAG_HOST			37

#define FLAG_OFFICER			38
#define FLAG_WELCOMER			39
#define FLAG_XBUILDER			40

#define FLAG_PRETTYLOOK			41

#define FLAG_BACKWARDS			42

#define FLAG_LINENUMBERS		43

#define FLAG_CENSORALL			44
#define FLAG_CENSORED			45
#define FLAG_CENSORPUBLIC		46
#define FLAG_PUBLIC			47

#define FLAG_FCHAT			48
#define FLAG_NOHUHLOGS			49

#define FLAG_DEBUG			50

#define FLAG_DONTANNOUNCE		51

/* JPK */
/* Flags to allow specific privs */
#define FLAG_NATTER			52

/* Used inside the code and not stored in the db */
/* Lee, says so and I belive him !*/
#define ARTICLE_LOWER_INDEFINITE  0x0
#define ARTICLE_LOWER_DEFINITE    0x1
#define ARTICLE_UPPER_INDEFINITE  0x2
#define ARTICLE_UPPER_DEFINITE	  0x3
#define ARTICLE_IRRELEVANT	  0x4 /* Not everything has an article */

#define VALUE_ARTICLE_NOUN		0x0	/* This will be the default */
#define VALUE_ARTICLE_PLURAL		0x1
#define VALUE_ARTICLE_SINGULAR_VOWEL	0x2
#define VALUE_ARTICLE_SINGULAR_CONS	0x3

#define TOM_FUSE	2	/* This is for count_down_fuses last parameter - tom_state */



#ifdef	ABORT_FUSES
#define Abort(x)	(db[(x)].get_flag(FLAG_ABORT) != 0)
#endif
#define Apprentice(x)	(db[(x)].get_flag(FLAG_APPRENTICE) != 0)
#define Natter(x)	(db[(x)].get_flag(FLAG_NATTER) != 0)

#define Articleof(x)	((db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_CONS))?FLAG_ARTICLE_SINGULAR_CONS:(db[(x)].get_flag(FLAG_ARTICLE_PLURAL))?FLAG_ARTICLE_PLURAL:(db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_VOWEL))?FLAG_ARTICLE_SINGULAR_VOWEL:FLAG_ARTICLE_NOUN)
#define Backwards(x)	(db[(x)].get_flag(FLAG_BACKWARDS))
#define ConvertArticle(x)	((db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_CONS))?VALUE_ARTICLE_SINGULAR_CONS:(db[(x)].get_flag(FLAG_ARTICLE_PLURAL))?VALUE_ARTICLE_PLURAL:(db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_VOWEL))?VALUE_ARTICLE_SINGULAR_VOWEL:VALUE_ARTICLE_NOUN)
#define	Abode(x)	(db[(x)].get_flag(FLAG_ABODE_OK))
#define Ashcan(x)	(db[(x)].get_flag(FLAG_ASHCAN) != 0)
#ifdef RESTRICTED_BUILDING
#define Builder(x)	((db[(x)].get_flag(FLAG_WIZARD)||db[(x)].get_flag(FLAG_BUILDER)) != 0)
#endif /* RESTRICTED_BUILDING */
#define Censorall(x)	(db[(x)].get_flag(FLAG_CENSORALL) !=0)
#define Censored(x)	(db[(x)].get_flag(FLAG_CENSORED) !=0)
#define Censorpublic(x)	(db[(x)].get_flag(FLAG_CENSORPUBLIC) !=0)
#define Colour(x)	(db[(x)].get_flag(FLAG_COLOUR) !=0)
#define Connected(x)	(db[(x)].get_flag(FLAG_CONNECTED) !=0)
#define Container(x)	((db[(x)].get_flag(FLAG_OPENABLE) || db[(x)].get_flag(FLAG_OPEN)) && (Typeof (x) == TYPE_THING))
#define Dark(x)		(db[(x)].get_flag(FLAG_DARK))
#define Debug(x)	(db[(x)].get_flag(FLAG_DEBUG))
#define Eric(x)		(db[(x)].get_flag(FLAG_ERIC))
#define Fchat(x)	(db[(x)].get_flag(FLAG_FCHAT))
#define Female(x)	(db[(x)].get_flag(FLAG_FEMALE))
#define Fighting(x)	(db[(x)].get_flag(FLAG_FIGHTING))
#define Number(x)	(db[(x)].get_flag(FLAG_NUMBER))
#define Haven(x)	(db[(x)].get_flag(FLAG_HAVEN))
#define Inheritable(x)	(db[(x)].get_flag(FLAG_INHERITABLE))
#define	Jump(x)		(db[(x)].get_flag(FLAG_JUMP_OK))
#define Linenumbers(x)	(db[(x)].get_flag(FLAG_LINENUMBERS))
#define	Link(x)		(db[(x)].get_flag(FLAG_LINK_OK))
#define	Listen(x)	(db[(x)].get_flag(FLAG_LISTEN))
#define Locked(x)	(db[(x)].get_flag(FLAG_LOCKED))
#define Male(x)		(db[(x)].get_flag(FLAG_MALE))
#define Neuter(x)	(db[(x)].get_flag(FLAG_NEUTER))
#define NoHuhLogs(x)	(db[(x)].get_flag(FLAG_NOHUHLOGS))
#define Officer(x)	(db[(x)].get_flag(FLAG_OFFICER))
#define	Opaque(x)	(db[(x)].get_flag(FLAG_OPAQUE))
#define	Open(x)		(db[(x)].get_flag(FLAG_OPEN))
#define	Openable(x)	(db[(x)].get_flag(FLAG_OPENABLE))
#define Prettylook(x)	(db[(x)].get_flag(FLAG_PRETTYLOOK))
#define Public(x)	(db[(x)].get_flag(FLAG_PUBLIC))
#define	Puppet(x)	(db [(x)].get_controller () != (x))
#define Readonly(x)	(db[(x)].get_flag(FLAG_READONLY))
#define Referenced(x)	(db[(x)].get_flag(FLAG_REFERENCED))
#define Silent(x)       (db[(x)].get_flag(FLAG_SILENT))
#define Sticky(x)	(db[(x)].get_flag(FLAG_STICKY))
#define Tom(x)		(db[(x)].get_flag(FLAG_TOM))
#define Tracing(x)	(db[(x)].get_flag(FLAG_TRACING))
#define Typeof(x)	(db.get_typeof((x)))
#define Unassigned(x)	(!((db[(x)].get_flag(FLAG_NEUTER)) || (db[(x)].get_flag(FLAG_MALE)) || (db[(x)].get_flag(FLAG_FEMALE))))
#define Settypeof(x,y)	(db.set_typeof((x),(y)))
#define Visible(x)	(db[(x)].get_flag(FLAG_VISIBLE))
#define Welcomer(x)	(db[(x)].get_flag(FLAG_WELCOMER))
#define Wizard(x)	(db[(x)].get_flag(FLAG_WIZARD))
#define XBuilder(x)	(db[(x)].get_flag(FLAG_XBUILDER))


#define CombatItem(x)	((Typeof((x)) == TYPE_WEAPON) || (Typeof((x)) == TYPE_ARMOUR) || (Typeof((x)) == TYPE_AMMUNITION))



/* special dbref's */
#define	NOTHING			(-1)		/* null dbref */
#define	AMBIGUOUS		(-2)		/* multiple possibilities, for matchers */
#define	HOME			(-3)		/* virtual	room, represents mover's home */


#define PLAYER_PID_STACK_SIZE 16


typedef	char			boolexp_type;

#define	BOOLEXP_AND		0
#define	BOOLEXP_OR		1
#define	BOOLEXP_NOT		2
#define	BOOLEXP_CONST		3
#define BOOLEXP_FLAG		4

/* This little lot following is so that the database load/save knows the number
 * of the field to save for each field on each object. When adding fields to 
 * objects you need to make sure that all classes dependant on that also 
 * have the field added with the _same_ number (for obvious reasons). */

/* When adding new fields you will need to add the appropriate field read to
 * each switch statement for every object class dependant on it. Adding a 
 * new object class to be read requires a lot of lines of code for the new
 * switch statement necessary. I know this sounds a very bad way of doing
 * things but it enables object fields to be read in any order. Hand
 * patching databases should be simple. */

/* Beta(tagged) db format completed 1/3/96, Dale Thompson */

enum object_fields { /* The base object fields */
OBJECT_FLAGS	= 128,
OBJECT_NAME	= 129,
OBJECT_LOCATION = 130,
OBJECT_NEXT	= 131,
OBJECT_OWNER	= 132
};


enum desc_object_fields { /* An extension of class object */
DOBJECT_FLAGS    = OBJECT_FLAGS,
DOBJECT_NAME     = OBJECT_NAME,
DOBJECT_LOCATION = OBJECT_LOCATION,
DOBJECT_NEXT     = OBJECT_NEXT,
DOBJECT_OWNER    = OBJECT_OWNER,
DOBJECT_DESC	 = 133,
};

enum lock_object_fields { /* An extension of Describable Object */
LOBJECT_FLAGS    = OBJECT_FLAGS,
LOBJECT_NAME     = OBJECT_NAME,
LOBJECT_LOCATION = OBJECT_LOCATION,
LOBJECT_NEXT     = OBJECT_NEXT,
LOBJECT_OWNER    = OBJECT_OWNER,
LOBJECT_DESC      = DOBJECT_DESC,
LOBJECT_KEY       = 134,
LOBJECT_FAIL      = 135,
LOBJECT_SUCC      = 136,
LOBJECT_DROP      = 137,
LOBJECT_OFAIL     = 138,
LOBJECT_OSUCC     = 139,
LOBJECT_ODROP     = 140
};

enum inherit_object_fields { /* An extension of Lockable Object */
IOBJECT_FLAGS    = OBJECT_FLAGS,
IOBJECT_NAME     = OBJECT_NAME,
IOBJECT_LOCATION = OBJECT_LOCATION,
IOBJECT_NEXT     = OBJECT_NEXT,
IOBJECT_OWNER    = OBJECT_OWNER,
IOBJECT_DESC      = DOBJECT_DESC,
IOBJECT_KEY       = LOBJECT_KEY,
IOBJECT_FAIL      = LOBJECT_FAIL,
IOBJECT_SUCC      = LOBJECT_SUCC,
IOBJECT_DROP      = LOBJECT_DROP,
IOBJECT_OFAIL     = LOBJECT_OFAIL,
IOBJECT_OSUCC     = LOBJECT_OSUCC,
IOBJECT_ODROP     = LOBJECT_ODROP,
IOBJECT_PARENT	  = 141 
};

enum old_object_fields { /* An extension of Inheritable Object */
OOBJECT_FLAGS    = OBJECT_FLAGS,
OOBJECT_NAME     = OBJECT_NAME,
OOBJECT_LOCATION = OBJECT_LOCATION,
OOBJECT_NEXT     = OBJECT_NEXT,
OOBJECT_OWNER    = OBJECT_OWNER,
OOBJECT_DESC      = DOBJECT_DESC,
OOBJECT_KEY       = LOBJECT_KEY,
OOBJECT_FAIL      = LOBJECT_FAIL,
OOBJECT_SUCC      = LOBJECT_SUCC,
OOBJECT_DROP      = LOBJECT_DROP,
OOBJECT_OFAIL     = LOBJECT_OFAIL,
OOBJECT_OSUCC     = LOBJECT_OSUCC,
OOBJECT_ODROP     = LOBJECT_ODROP,
OOBJECT_PARENT	= IOBJECT_PARENT,
OOBJECT_COMMANDS	= 142,
OOBJECT_CONTENTS	= 143,
OOBJECT_DESTINATION = 144,
OOBJECT_EXITS	= 145,
OOBJECT_FUSES	= 146,
OOBJECT_INFO	= 147
};

enum massy_object_fields { /* An extension of Old Object */
MOBJECT_FLAGS    = OBJECT_FLAGS,
MOBJECT_NAME     = OBJECT_NAME,
MOBJECT_LOCATION = OBJECT_LOCATION,
MOBJECT_NEXT     = OBJECT_NEXT,
MOBJECT_OWNER    = OBJECT_OWNER,
MOBJECT_DESC      = DOBJECT_DESC,
MOBJECT_KEY       = LOBJECT_KEY,
MOBJECT_FAIL      = LOBJECT_FAIL,
MOBJECT_SUCC      = LOBJECT_SUCC,
MOBJECT_DROP      = LOBJECT_DROP,
MOBJECT_OFAIL     = LOBJECT_OFAIL,
MOBJECT_OSUCC     = LOBJECT_OSUCC,
MOBJECT_ODROP     = LOBJECT_ODROP,
MOBJECT_PARENT  = IOBJECT_PARENT,
MOBJECT_COMMANDS        = OOBJECT_COMMANDS,
MOBJECT_CONTENTS        = OOBJECT_CONTENTS,
MOBJECT_DESTINATION = OOBJECT_DESTINATION,
MOBJECT_EXITS   = OOBJECT_EXITS,
MOBJECT_FUSES   = OOBJECT_FUSES,
MOBJECT_INFO    = OOBJECT_INFO,
MOBJECT_GF        = 148,
MOBJECT_MASS      = 149,
MOBJECT_VOLUME    = 150,
MOBJECT_MASSLIM   = 151,
MOBJECT_VOLUMELIM = 152 
};

enum Puppet_fields { /* An extended Massy Object */
PUPPET_FLAGS     = OBJECT_FLAGS,
PUPPET_NAME      = OBJECT_NAME,
PUPPET_LOCATION  = OBJECT_LOCATION,
PUPPET_NEXT      = OBJECT_NEXT,
PUPPET_OWNER     = OBJECT_OWNER,
PUPPET_DESC      = DOBJECT_DESC,
PUPPET_KEY       = LOBJECT_KEY,
PUPPET_FAIL      = LOBJECT_FAIL,
PUPPET_SUCC      = LOBJECT_SUCC,
PUPPET_DROP      = LOBJECT_DROP,
PUPPET_OFAIL     = LOBJECT_OFAIL,
PUPPET_OSUCC     = LOBJECT_OSUCC,
PUPPET_ODROP     = LOBJECT_ODROP,
PUPPET_PARENT    = IOBJECT_PARENT,
PUPPET_COMMANDS  = OOBJECT_COMMANDS,
PUPPET_CONTENTS  = OOBJECT_CONTENTS,
PUPPET_DESTINATION = OOBJECT_DESTINATION,
PUPPET_EXITS     = OOBJECT_EXITS,
PUPPET_FUSES     = OOBJECT_FUSES,
PUPPET_INFO      = OOBJECT_INFO,
PUPPET_GF        = MOBJECT_GF,
PUPPET_MASS      = MOBJECT_MASS,
PUPPET_VOLUME    = MOBJECT_VOLUME,
PUPPET_MASSLIM   = MOBJECT_MASSLIM,
PUPPET_VOLUMELIM = MOBJECT_VOLUMELIM,
PUPPET_PENNIES	 = 153,
PUPPET_SCORE	= 154,
PUPPET_RACE	= 155,
PUPPET_WHOSTRING = 156,
PUPPET_EMAIL	= 157,
PUPPET_BUILDID	= 160,
PUPPET_MONEY	= 163
};

enum Player_fields { /* An extension of puppet */
PLAYER_FLAGS     = OBJECT_FLAGS,
PLAYER_NAME      = OBJECT_NAME,
PLAYER_LOCATION  = OBJECT_LOCATION,
PLAYER_NEXT      = OBJECT_NEXT,
PLAYER_OWNER     = OBJECT_OWNER,
PLAYER_DESC      = DOBJECT_DESC,
PLAYER_KEY       = LOBJECT_KEY,
PLAYER_FAIL      = LOBJECT_FAIL,
PLAYER_SUCC      = LOBJECT_SUCC,
PLAYER_DROP      = LOBJECT_DROP,
PLAYER_OFAIL     = LOBJECT_OFAIL,
PLAYER_OSUCC     = LOBJECT_OSUCC,
PLAYER_ODROP     = LOBJECT_ODROP,
PLAYER_PARENT    = IOBJECT_PARENT,
PLAYER_COMMANDS  = OOBJECT_COMMANDS,
PLAYER_CONTENTS  = OOBJECT_CONTENTS,
PLAYER_DESTINATION = OOBJECT_DESTINATION,
PLAYER_EXITS     = OOBJECT_EXITS,
PLAYER_FUSES     = OOBJECT_FUSES,
PLAYER_INFO      = OOBJECT_INFO,
PLAYER_GF	 = MOBJECT_GF,
PLAYER_MASS	 = MOBJECT_MASS,
PLAYER_VOLUME	 = MOBJECT_VOLUME,
PLAYER_MASSLIM	 = MOBJECT_MASSLIM,
PLAYER_VOLUMELIM = MOBJECT_VOLUMELIM,
PLAYER_PENNIES	 = PUPPET_PENNIES,
PLAYER_SCORE	= PUPPET_SCORE,
PLAYER_RACE	= PUPPET_RACE,
PLAYER_WHOSTRING = PUPPET_WHOSTRING,
PLAYER_EMAIL	= PUPPET_EMAIL,
PLAYER_PASSWORD	= 158,
PLAYER_CONTROLLER = 159,
PLAYER_BUILDID	= 160,
PLAYER_COLOUR	= 161,
PLAYER_ALIASES	= 162,
PLAYER_MONEY	= PUPPET_MONEY
};

enum Room_fields { /* An extended Massy Object */
ROOM_FLAGS    = OBJECT_FLAGS,
ROOM_NAME     = OBJECT_NAME,
ROOM_LOCATION = OBJECT_LOCATION,
ROOM_NEXT     = OBJECT_NEXT,
ROOM_OWNER    = OBJECT_OWNER,
ROOM_DESC      = DOBJECT_DESC,
ROOM_KEY       = LOBJECT_KEY,
ROOM_FAIL      = LOBJECT_FAIL,
ROOM_SUCC      = LOBJECT_SUCC,
ROOM_DROP      = LOBJECT_DROP,
ROOM_OFAIL     = LOBJECT_OFAIL,
ROOM_OSUCC     = LOBJECT_OSUCC,
ROOM_ODROP     = LOBJECT_ODROP,
ROOM_PARENT  = IOBJECT_PARENT,
ROOM_COMMANDS        = OOBJECT_COMMANDS,
ROOM_CONTENTS        = OOBJECT_CONTENTS,
ROOM_DESTINATION = OOBJECT_DESTINATION,
ROOM_EXITS   = OOBJECT_EXITS,
ROOM_FUSES   = OOBJECT_FUSES,
ROOM_INFO    = OOBJECT_INFO,
ROOM_GF        = MOBJECT_GF,
ROOM_MASS      = MOBJECT_MASS,
ROOM_VOLUME    = MOBJECT_VOLUME,
ROOM_MASSLIM   = MOBJECT_MASSLIM,
ROOM_VOLUMELIM = MOBJECT_VOLUMELIM,
ROOM_LASTENTRY	= 153, 
ROOM_CONTSTR	= 154
};

enum Thing_fields { /* An extended Massy Object */
THING_FLAGS    = OBJECT_FLAGS,
THING_NAME     = OBJECT_NAME,
THING_LOCATION = OBJECT_LOCATION,
THING_NEXT     = OBJECT_NEXT,
THING_OWNER    = OBJECT_OWNER,
THING_DESC      = DOBJECT_DESC,
THING_KEY       = LOBJECT_KEY,
THING_FAIL      = LOBJECT_FAIL,
THING_SUCC      = LOBJECT_SUCC,
THING_DROP      = LOBJECT_DROP,
THING_OFAIL     = LOBJECT_OFAIL,
THING_OSUCC     = LOBJECT_OSUCC,
THING_ODROP     = LOBJECT_ODROP,
THING_PARENT  = IOBJECT_PARENT,
THING_COMMANDS        = OOBJECT_COMMANDS,
THING_CONTENTS        = OOBJECT_CONTENTS,
THING_DESTINATION = OOBJECT_DESTINATION,
THING_EXITS   = OOBJECT_EXITS,
THING_FUSES   = OOBJECT_FUSES,
THING_INFO    = OOBJECT_INFO,
THING_GF        = MOBJECT_GF,
THING_MASS      = MOBJECT_MASS,
THING_VOLUME    = MOBJECT_VOLUME,
THING_MASSLIM   = MOBJECT_MASSLIM,
THING_VOLUMELIM = MOBJECT_VOLUMELIM,
THING_CONTSTR	= 153,
THING_KEYNOFREE	= 154 
};

enum Dictionary_fields { /* An extension of class object */
DICTIONARY_FLAGS     = OBJECT_FLAGS,
DICTIONARY_NAME      = OBJECT_NAME,
DICTIONARY_LOCATION  = OBJECT_LOCATION,
DICTIONARY_NEXT      = OBJECT_NEXT,
DICTIONARY_OWNER     = OBJECT_OWNER,
DICTIONARY_ELEMENTS	= 133 
};

enum Array_storage_fields { /* Things that store their descs as arrays (everything) */
ARRAY_STORAGE_ELEMENTS = 170
};

enum Array_fields { /* An extension of class object */
ARRAY_FLAGS     = OBJECT_FLAGS,
ARRAY_NAME      = OBJECT_NAME,
ARRAY_LOCATION  = OBJECT_LOCATION,
ARRAY_NEXT      = OBJECT_NEXT,
ARRAY_OWNER     = OBJECT_OWNER,
ARRAY_ELEMENTS	= 133 
};

class	context;
class	Pending_alarm;
class	Pending_fuse;
class	Matcher;


class	boolexp
{
    private:
	boolexp(const boolexp&); // DUMMY
	boolexp& operator=(const boolexp&); // DUMMY

	boolexp_type		type;
	boolexp			*sub1;
	boolexp			*sub2;
	dbref			thing;
	const	bool		eval_internal		(const context &c, Matcher &matcher)		const;
		void		unparse_internal	(context &c, boolexp_type outer_type)	const;
		void		unparse_for_return_internal	(context &c, const boolexp_type outer_type)	const;
	friend	boolexp		*parse_boolexp_E	(const dbref player);
	friend	boolexp		*parse_boolexp_T	(const dbref player);
	friend	boolexp		*parse_boolexp_F	(const dbref player);
	friend	void		putbool_subexp		(FILE *, const boolexp *, char **);
	friend	boolexp		*getboolexp1		(char **);
    public:
				boolexp			(boolexp_type t);
				~boolexp		();
	const	bool		eval			(const context &c, Matcher &matcher)		const;
	const	bool		contains		(const dbref thing)		const;
	const	char		*unparse		(context &c)		const;
	const	char		*unparse_for_return	(context &c)		const;
	boolexp			*sanitise		();
	void			valid_key		(const dbref)	const;
	void			set_references		()		const;
};

extern		char		*alloc_string		(const char *);

#define	TRUE_BOOLEXP		((boolexp *) 0)


class	object
{
    private:
	String			name;			
	dbref			location;	/* pointer to container */
	dbref			next;		/* pointer to next in contents/exits/etc chain */
	unsigned char		flags[FLAGS_WIDTH];	/* Flag list (bits) */
//	typeof_type		type;		/*Type of object*/
	dbref			owner;		/* who controls this object */
#ifdef ALIASES
			void	set_alias			(const int which, const String& what);
			int	look_at_aliases			(const String&)	const	{ return 0; }
#endif /* ALIASES */
    public:
				object				();
	virtual			~object				();

	virtual		void	add_recall_line			(const String& string);
	virtual		void	output_recall			(const int lines, const context * con);
	virtual		void	ditch_recall			();
//	virtual	const	bool	write_array_elements		(FILE *)		const;
//	virtual	const	bool	read_array_elements		(FILE *, const int, const int);

	virtual	const	bool	write				(FILE *)		const;
	virtual	const	bool	read				(FILE *);
	virtual	const	bool	destroy				(const dbref);
	/* set functions */

			void	set_flag			(const int f)			{ flags[f/8] |= (1<<(f%8)); }
			void	clear_flag			(const int f)			{ flags[f/8] &= ~(1<<(f%8)); }
			int	get_flag			(const int f)		const	{ return(flags[f/8] & (1<<(f%8))); }
			void	set_flag_byte			(const int c, const flag_type v){ flags[c] = v; }
			unsigned char	get_flag_byte		(const int c)		const	{ return (flags[c]); }


			void	set_referenced			()				{ set_flag(FLAG_REFERENCED); }
			void	set_name			(const String& str);
			void	set_location			(const dbref o)			{ location = o; }
			void	set_next			(const dbref o)			{ next = o; }
			void	set_owner_no_check		(const dbref o)			{ owner = o; }
			void	set_owner			(const dbref o);
	virtual		void	empty_object			()				{ return;}
	/* Describable_object */
	virtual		void	set_description			(const String& str);
	/* Lockable_object */
	virtual		void	set_key				(boolexp *);
	virtual		void	set_key_no_free			(boolexp *k);
	virtual		void	set_fail_message		(const String& str);
	virtual		void	set_drop_message		(const String& str);
	virtual		void	set_succ_message		(const String& str);
	virtual		void	set_ofail			(const String& str);
	virtual		void	set_osuccess			(const String& str);
	virtual		void	set_odrop			(const String& str);
	/* Inheritable_object */
	virtual		void	set_parent			(const dbref p);
	virtual		void	set_parent_no_check		(const dbref p);
	virtual	const String& 	get_inherited_element		(const int)	const;
	virtual	const int	exist_inherited_element		(const int)	const;
	virtual const unsigned int	get_inherited_number_of_elements(void)	const;
	/* Old_object */
	virtual		void	set_destination			(const dbref o);
	virtual		void	set_destination_no_check	(const dbref o);
	virtual		void	set_contents			(const dbref o);
	virtual		void	set_exits			(const dbref o);
	virtual		void	set_commands			(const dbref o);
	virtual		void	set_info_items			(const dbref o);
	virtual		void	set_fuses			(const dbref o);
	/* Command */
	virtual		void	set_csucc			(const dbref o);
	virtual		void	set_cfail			(const dbref o);
	virtual		void	set_size			(const int);
	virtual		int	get_size			()			const	{return 0;}
	virtual unsigned int	inherited_lines_in_cmd_blk(const unsigned) const;
	virtual	unsigned int	reconstruct_inherited_command_block(char *const command_block, const unsigned max_length, const unsigned start_line)	const;
	/* Massy_object */
	virtual		void	set_gravity_factor		(const double g);
	virtual		void	set_mass			(const double m);
	virtual		void	set_volume			(const double v);
	virtual		void	set_mass_limit			(const double m);
	virtual		void	set_volume_limit		(const double v);
	/* Player */
	virtual		Channel	*get_channel		()			const	{return NULL;}
	virtual		void	set_channel			(Channel *);
	virtual	const	int	get_money			()			const	{return 0;}
	virtual		void	set_money			(const int);
	virtual		void	set_colour			(const char *);
	virtual		String	get_colour			()			const	{return NULLSTRING;}
	virtual		void	set_colour_at			(colour_at*);
	virtual	const	colour_at&	get_colour_at	()			const	{return default_colour_at;}
	virtual		void	set_colour_play			(cplay *);
	virtual	const	cplay 	*get_colour_play		()			const	{return NULL;}
	virtual		void	set_colour_play_size		(const int);
	virtual	const	int	get_colour_play_size		()			const	{return 0;}
	virtual		void	set_pennies			(const int i);
	virtual		void	add_pennies			(const int i);
	virtual		void	set_build_id			(const dbref c);
	virtual		void	reset_build_id			(const dbref c);
	virtual		void	set_controller			(const dbref c);
	virtual		void	set_email_addr			(const String&);
	virtual		void	set_password			(const String&);
	virtual		void	set_race			(const String&);
	virtual		void	set_score			(const long v);
	virtual		void	set_last_name_change		(const long v);
	virtual		void	set_who_string			(const String&);
#if 0	/* PJC 24/1/97 */
	virtual		void	event				(const dbref player, const dbref npc, const char *e);
#endif

	/* Thing */
	virtual		void	set_contents_string		(const String&);
	virtual		void	set_lock_key			(boolexp *k);
	virtual		void	set_lock_key_no_free		(boolexp *k);

		const	String& get_name		()			const	{ return (name); }
		const	String&	get_inherited_name	()			const;
		const	dbref	get_location			()			const	{ return location; }
		const	dbref	get_next			()			const	{ return next; }
		const	dbref	get_real_next			()			const	{ return next; }
		const	dbref	get_owner			()			const	{ return owner; }
	/* Describable_object */
	virtual	const	String& get_description		()			const	{ return NULLSTRING; }
		const	String& get_inherited_description	()			const;
	/* Lockable_object */
	virtual	const	boolexp	*get_key			()			const	{ return (TRUE_BOOLEXP); }
	virtual		boolexp	*get_key_for_edit		()			const	{ return (TRUE_BOOLEXP); }
		const	boolexp	*get_inherited_key		()			const;
	virtual	const	String&	get_fail_message		()			const	{ return (NULLSTRING); }
		const	String&	get_inherited_fail_message	()			const;
	virtual	const	String&	get_drop_message		()			const	{ return (NULLSTRING); }
		const	String&	get_inherited_drop_message	()			const;
	virtual	const	String&	get_succ_message		()			const	{ return (NULLSTRING); }
		const	String&	get_inherited_succ_message	()			const;
	virtual	const	String&	get_ofail			()			const	{ return (NULLSTRING); }
		const	String&	get_inherited_ofail		()			const;
	virtual	const	String&	get_osuccess			()			const	{ return (NULLSTRING); }
		const	String&	get_inherited_osuccess		()			const;
	virtual	const	String&	get_odrop			()			const	{ return (NULLSTRING); }
		const	String&	get_inherited_odrop		()			const;
	/* Inheritable_object */
	virtual	const	dbref	get_parent			()			const	{ return (NOTHING); }
	/* Old_object */
	virtual	const	dbref	get_commands			()			const	{ return NOTHING; }
	virtual	const	dbref	get_contents			()			const	{ return NOTHING; }
	virtual	const	dbref	get_destination			()			const	{ return NOTHING; }
	virtual	const	dbref	get_exits			()			const	{ return NOTHING; }
	virtual	const	dbref	get_fuses			()			const	{ return NOTHING; }
	virtual	const	dbref	get_info_items			()			const	{ return NOTHING; }
	virtual	const	dbref	get_variables			()			const	{ return NOTHING; }
	virtual	const	dbref	get_properties			()			const	{ return NOTHING; }
	virtual	const	dbref	get_arrays			()			const	{ return NOTHING; }
	virtual	const	dbref	get_dictionaries		()			const	{ return NOTHING; }
	/* Command */
	virtual	const	dbref	get_csucc			()			const	{ return NOTHING; }
		const	dbref	get_inherited_csucc		()			const;
	virtual	const	dbref	get_cfail			()			const	{ return NOTHING; }
		const	dbref	get_inherited_cfail		()			const;
	virtual	const	unsigned short	get_parse_helper	(const unsigned int)		const	{ return 0; }
	virtual	const	bool	alloc_parse_helper		();
	virtual		void	flush_parse_helper		();
	virtual		void	set_parse_helper		(const unsigned int index, const unsigned short value);
	/* Massy_object */
	virtual	const	double	get_gravity_factor		()			const	{ return (1.0); }
	virtual	const	double	get_inherited_gravity_factor	()			const;
	virtual	const	double	get_mass			()			const	{ return (0.0); }
	virtual	const	double	get_inherited_mass		()			const;
	virtual	const	double	get_volume			()			const	{ return (0.0); }
	virtual	const	double	get_inherited_volume		()			const;
	virtual	const	double	get_mass_limit			()			const	{ return (0.0); }
	virtual	const	double	get_inherited_mass_limit	()			const;
	virtual	const	double	get_volume_limit		()			const	{ return (0.0); }
	virtual	const	double	get_inherited_volume_limit	()			const;
	/* Player */
#ifdef ALIASES
	virtual	const	String&	get_alias			(int)			const	{ return NULLSTRING; }
	virtual	const	int	remove_alias			(const String&	)		{ return 0; }
	virtual	const	int	add_alias			(const String&	)		{ return 0; }
	virtual	const	int	has_alias			(const String&	)		const	{ return 0; }
	virtual	const	int	has_partial_alias		(const String&	)		const	{ return 0; }
#endif /* ALIASES */
	virtual	const	int	get_pennies			()			const	{ return (0); }
	virtual	const	dbref	get_controller			()			const	{ return (NOTHING); }
	virtual	const	dbref	get_build_id			()			const	{ return (NOTHING); }
	virtual	const	String&	get_email_addr		()			const	{ return (NULLSTRING); }
	virtual	const	String&	get_password			()			const	{ return (NULLSTRING); }
	virtual	const	String&	get_race		()			const	{ return (NULLSTRING); }
	virtual	const	long	get_score			()			const	{ return (0); }
	virtual	const	long	get_last_name_change		()			const	{ return (0); }
	virtual	const	String&	get_who_string		()			const	{ return (NULLSTRING); }
	/* Thing */
	virtual	const	String&	get_contents_string		()			const	{ return (NULLSTRING); }
	virtual	const	boolexp	*get_lock_key			()			const	{ return (TRUE_BOOLEXP); }
	virtual		boolexp	*get_lock_key_for_edit		()			const	{ return (TRUE_BOOLEXP); }
	/* Room */
	virtual		void	set_last_entry_time		()				{ return; }
	virtual	const	time_t	get_last_entry_time		()			const	{ return 0; }
	/* Information */
    	virtual	const	unsigned int	get_number_of_elements		()			const	{ return 0; }
    	virtual	const	String&	get_element			(int)			const	{ return (NULLSTRING); }
    	virtual		void	destroy_element			(int)				{ return; }
    	virtual		void	insert_element			(int, const String&)		{ return; }
    	/* Array_storage */
    	virtual		void	sort_elements			(int);
	/* Array */
    	virtual		void	set_element			(int, const String&)	{ return; }
    	virtual	const	int	exist_element			(int)			const	{ return 0; }
    	/* Dictionary */
    	virtual		void	set_element			(int, const String&, const String&){ return; }
    	virtual	const	int	exist_element			(const String&)		const	{ return false; }
    	virtual		void	set_index			(const int, const String&);
    	virtual	const	String&	get_index			(int)			const	{ return (NULLSTRING); }
};


class	object_and_flags
{
    private:
	object_and_flags(const object_and_flags&); // DUMMY
	object_and_flags& operator=(const object_and_flags&); // DUMMY

	int			type;
	object			*obj;
	dbref			freelist;
    public:
    	typeof_type		get_type	()			const	{ return (type); }
	void			set_type	(const typeof_type i)		{ type = i; }
				object_and_flags () : type(TYPE_FREE), obj(NULL), freelist(NOTHING)				{}
	bool			is_free		()			const	{ return (type == TYPE_FREE); }
	dbref			get_free	()			const	{ return (freelist); }
	object			*get_obj	()			const	{ return (type == TYPE_FREE ? (object*)NULL : obj); }
	void			init_free	(const dbref next)		{ type = TYPE_FREE; freelist = next; }
	void			set_free	(const dbref next)		{ if (type != TYPE_FREE) delete (obj); type = TYPE_FREE; freelist = next; }
	void			set_obj		(object &new_obj)		{ type = TYPE_NO_TYPE; obj = &new_obj; }
};


#define	CACHE_INVALID	0
#define	CACHE_VALID	1

struct player_cache_struct
{
	String		name;
	dbref		player;
	int		state;

	int compare(const player_cache_struct* other) const;
};


class	Database
{
    private:
	Database(const Database&); // DUMMY
	Database& operator=(const Database&); // DUMMY
	object_and_flags	*array;
	dbref			top;
	dbref			free_start;
	dbref			free_end;
	Pending_alarm		*alarms;
	int			player_count;
	struct player_cache_struct *player_cache;
	struct changed_player_list_struct
	{
		char	*name;
		dbref	player;
		struct	changed_player_list_struct	*next;
		struct	changed_player_list_struct	*prev;
	} *changed_player_list;
	void			grow		(dbref newsize);
	void			build_player_cache	(int player_count);
    public:
				Database	();
				~Database	();
		object		&operator[]	(dbref i)		const	{ return (*(array [i].get_obj ())); }
		object		*operator+	(dbref i)		const	{ return (array [i].get_obj ()); }
	const	dbref		get_top		()			const	{ return top; }
	const	dbref		new_object	(object &obj);
	const	bool		delete_object	(const dbref oldobj);
	const	bool		write		(FILE *f)		const;
	const	dbref		read		(FILE *f);
		void		set_free_between (dbref, dbref);

	void			set_typeof	(dbref i, typeof_type j)	{ array[i].set_type(j); }
	typeof_type		get_typeof	(dbref i)		const	{ return (array[i].get_type()); }

		void		sanity_check	();
		void		pend		(dbref i);
		void		unpend		(dbref i);
		dbref		alarm_pending	(time_t now);
		Pending_alarm	*get_alarms	()			const	{ return (alarms); }
	const	dbref		lookup_player	(const String&)	const;
		void		add_player_to_cache(const dbref, const String&);
		void		remove_player_from_cache(const String&);
};

extern	Database		db;


extern	const	dbref			parse_dbref	(const char *);


#define	DOLIST(var, first)	for((var) = (first); (var) != NOTHING; (var) = db[(var)].get_next ())
#define	DOREALLIST(var, first)	for((var) = (first); (var) != NOTHING; (var) = db[(var)].get_real_next ())
#define	PUSH(thing,index,field)	{ db[(thing)].set_next (db[(index)].get_##field ()); db[(index)].set_##field ((thing)); }
#define	getloc(thing)		(db[thing].get_location ())

/*
  Usage guidelines:

  To refer to objects use db[object_ref].  Pointers to objects may 
  become invalid after a call to new_object().

  The programmer is responsible for managing storage for string
  components of entries; db_read will produce malloc'd strings.  The
  alloc_string routine is provided for generating malloc'd strings
  duplicates of other strings.  Note that db_free and db_read will
  attempt to free any non-NULL string that exists in db when they are
  invoked.  
*/
#endif /* _DB_H */

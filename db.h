/** \file
 * Structures to make up the database of objects, plus many constants that are
 *	only used when reading and writing these to files.  TODO: Separate the
 *	file-only stuff for faster compilation.
 */

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

#include "copyright.h"
#ifndef _DB_H
#define _DB_H
#pragma interface
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <map>
#include <list>

#include "mudstring.h"
#include "config.h"

#ifdef DBREF_BUG_HUNTING
class dbref
{
private:
	int	_ref;
	void	assign(int i);
public:
	dbref(int i = -1) { assign(i); }
	explicit operator int() const { return _ref; }
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
typedef	int	dbref;		///< Offset into db array, used as an object identity.
#endif // DBREF_BUG_HUNTING

#include "colour.h"		// colour.h relies on dbref

// special dbrefs
const	dbref	NOTHING		(-1);		///< null dbref
const	dbref	AMBIGUOUS	(-2);		///< multiple possibilities, for matchers
const	dbref	HOME		(-3);		///< virtual room representing mover's home
const	dbref	UNPARSE_ID	(-4);		///< Generic 'can always UNPARSE'

typedef unsigned char	flag_type;

class	Channel;

#define FLAGS_WIDTH 8

/*These defines will be used to store the flags in the database
  so they must NOT be changed without a db trawl or a special
  load function*/

/**
 * Type definitions, TYPE_FREE mustn't be 0 since it is used
 *	to indicate whether or not the member of the array has
 *	an object associated with it.
 */

enum typeof_type
{
	TYPE_NO_TYPE		=1,
	TYPE_FREE		=2,
	TYPE_PLAYER		=3,
	TYPE_PUPPET		=4,
	TYPE_ROOM		=5,
	TYPE_THING		=6,
	TYPE_EXIT		=7,
	TYPE_COMMAND		=8,
	TYPE_VARIABLE		=9,
	TYPE_PROPERTY		=10,
	TYPE_ARRAY		=11,
	TYPE_DICTIONARY	=12,
	TYPE_ALARM		=13,
	TYPE_FUSE		=14,
	TYPE_TASK		=15,
	TYPE_SEMAPHORE	=16
};


/* Flag definitions */
enum object_flag_type
{
	FLAG_WIZARD			= 1,
	FLAG_APPRENTICE			= 2,
	FLAG_BUILDER			= 3,
	FLAG_LISTEN			= 4,
	FLAG_CONNECTED			= 5,
	FLAG_ERIC			= 6,
	FLAG_MALE			= 7,
	FLAG_FEMALE			= 8,
	FLAG_NEUTER			= 9,
	FLAG_DARK			= 10,
	FLAG_SILENT			= 11,
	FLAG_STICKY			= 12,
	FLAG_VISIBLE			= 13,
	FLAG_OPENABLE			= 14,
	FLAG_OPEN			= 15,
	FLAG_OPAQUE			= 16,
	FLAG_INHERITABLE		= 17,
	FLAG_LOCKED			= 18,
	FLAG_CHOWN_OK			= 19,
	FLAG_FIGHTING			= 20,
	FLAG_HAVEN			= 21,
	FLAG_TRACING			= 22,
	FLAG_NUMBER			= 23,
	FLAG_ABORT			= 24,
	FLAG_TOM			= 25,
	FLAG_ASHCAN			= 26,
	FLAG_REFERENCED			= 27,
	FLAG_READONLY			= 28,
	FLAG_ARTICLE_NOUN		= 29,	/* This will be the default */
	FLAG_ARTICLE_PLURAL		= 30,
	FLAG_ARTICLE_SINGULAR_VOWEL	= 31,
	FLAG_ARTICLE_SINGULAR_CONS	= 32,
	FLAG_ABODE_OK			= 33,
	FLAG_LINK_OK			= 34,
	FLAG_JUMP_OK			= 35,
	FLAG_COLOUR			= 36,
	FLAG_HOST			= 37,
	FLAG_OFFICER			= 38,
	FLAG_WELCOMER			= 39,
	FLAG_XBUILDER			= 40,
	FLAG_PRETTYLOOK			= 41,
	FLAG_BACKWARDS			= 42,
	FLAG_LINENUMBERS		= 43,
	FLAG_CENSORALL			= 44,
	FLAG_CENSORED			= 45,
	FLAG_CENSORPUBLIC		= 46,
	FLAG_PUBLIC			= 47,
	FLAG_FCHAT			= 48,
	FLAG_NOHUHLOGS			= 49,
	FLAG_DEBUG			= 50,
	FLAG_DONTANNOUNCE		= 51,
/* Flags to allow specific privs */
	FLAG_NATTER			= 52,
	FLAG_RETIRED			= 53,
	FLAG_LITERALINPUT		= 54,
	FLAG_NO_EMAIL_FORWARD		= 55,
/* God-like powers */
	FLAG_GOD_SET_GOD		= 56,	// Catch-22, have to have this set in order to set it...
	FLAG_GOD_WRITE_ALL		= 57,	// Overide all controls_for_write, apart from @password
	FLAG_GOD_BOOT_ALL		= 58,	// Override all @boot checks
	FLAG_GOD_CHOWN_ALL		= 59,	// Override all @chown checks
	FLAG_GOD_MAKE_WIZARD		= 60,	// Can set Wizard on a Player
	FLAG_GOD_PASSWORD_RESET		= 61,	// Can set any player's password
	FLAG_GOD_DEEP_POCKETS		= 62,	// Has an infinite supply of Drogna
/* We're running out of bits here guys... */
	FLAG_PRIVATE			= 63	// Object is private and can't be looked at normally.
};

/* Used inside the code and not stored in the db */
/* Lee says so and I belive him! */
#define ARTICLE_LOWER_INDEFINITE  0x0
#define ARTICLE_LOWER_DEFINITE    0x1
#define ARTICLE_UPPER_INDEFINITE  0x2
#define ARTICLE_UPPER_DEFINITE	  0x3
#define ARTICLE_IRRELEVANT	  0x4 /* Not everything has an article */

#define VALUE_ARTICLE_NOUN		0x0	/* This will be the default */
#define VALUE_ARTICLE_PLURAL		0x1
#define VALUE_ARTICLE_SINGULAR_VOWEL	0x2
#define VALUE_ARTICLE_SINGULAR_CONS	0x3

#define TOM_FUSE	2	///< This is for count_down_fuses last parameter - tom_state

#define Articleof(x)	((db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_CONS))?FLAG_ARTICLE_SINGULAR_CONS:(db[(x)].get_flag(FLAG_ARTICLE_PLURAL))?FLAG_ARTICLE_PLURAL:(db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_VOWEL))?FLAG_ARTICLE_SINGULAR_VOWEL:FLAG_ARTICLE_NOUN)
#define ConvertArticle(x)	((db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_CONS))?VALUE_ARTICLE_SINGULAR_CONS:(db[(x)].get_flag(FLAG_ARTICLE_PLURAL))?VALUE_ARTICLE_PLURAL:(db[(x)].get_flag(FLAG_ARTICLE_SINGULAR_VOWEL))?VALUE_ARTICLE_SINGULAR_VOWEL:VALUE_ARTICLE_NOUN)
#ifdef RESTRICTED_BUILDING
#define Builder(x)	((db[(x)].get_flag(FLAG_WIZARD)||db[(x)].get_flag(FLAG_BUILDER)) != 0)
#endif /* RESTRICTED_BUILDING */
#define Container(x)	((db[(x)].get_flag(FLAG_OPENABLE) || db[(x)].get_flag(FLAG_OPEN)) && (Typeof (x) == TYPE_THING))
#define Unassigned(x)	(!((db[(x)].get_flag(FLAG_NEUTER)) || (db[(x)].get_flag(FLAG_MALE)) || (db[(x)].get_flag(FLAG_FEMALE))))
#define Settypeof(x,y)	(db.set_typeof((x),(y)))
#define Visible(x)	(db[(x)].get_flag(FLAG_VISIBLE))
#define Welcomer(x)	(db[(x)].get_flag(FLAG_WELCOMER))
#define Wizard(x)	(db[(x)].get_flag(FLAG_WIZARD))
#define XBuilder(x)	(db[(x)].get_flag(FLAG_XBUILDER))
#define NoForwardEmail(x)	(db[(x)].get_flag(FLAG_NO_EMAIL_FORWARD))
#define DontAnnounce(x)	(db[(x)].get_flag(FLAG_DONTANNOUNCE))
#define	Created(x)	{if((x) != NOTHING) (db[(x)].set_ctime());}
#define	Modified(x)	{if((x) != NOTHING) (db[(x)].set_mtime());}
#define	Accessed(x)	{if((x) != NOTHING) (db[(x)].set_atime());}
// God-like powers
#define	SetGodPower(x)	(((x) == GOD_ID) || db[(x)].get_flag(FLAG_GOD_SET_GOD))
#define	WriteAll(x)	(db[(x)].get_flag(FLAG_GOD_WRITE_ALL))
#define	BootAll(x)	(db[(x)].get_flag(FLAG_GOD_BOOT_ALL))
#define	ChownAll(x)	(db[(x)].get_flag(FLAG_GOD_CHOWN_ALL))
#define	MakeWizard(x)	(db[(x)].get_flag(FLAG_GOD_MAKE_WIZARD))
#define	PasswordReset(x)	(db[(x)].get_flag(FLAG_GOD_PASSWORD_RESET))
#define	MoneyMaker(x)	(db[(x)].get_flag(FLAG_GOD_DEEP_POCKETS))

#define Private(x)	(db[(x)].get_flag(FLAG_PRIVATE))


#define PLAYER_PID_STACK_SIZE 16


typedef	char			boolexp_type;

#define	BOOLEXP_AND		0
#define	BOOLEXP_OR		1
#define	BOOLEXP_NOT		2
#define	BOOLEXP_CONST		3
#define BOOLEXP_FLAG		4

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
	dbref			id;
	String			m_name;		///< The thing's name. May be a list of names separated by ';'
	std::list<String>	namelist;	///< A list of name-components (';' separated bits of name)
	dbref			location;	///< pointer to container
	dbref			next;		///< pointer to next in contents/exits/etc chain
	flag_type		flags[FLAGS_WIDTH];	///< Flag list (bits)
//	typeof_type		type;		///< Type of object
	dbref			m_owner;	///< who controls this object
	time_t			m_ctime;	///< creation time
	time_t			m_mtime;	///< modification time
	time_t			m_atime;	///< access time
#ifdef ALIASES
			void	set_alias			(const int which, const String& what);
			int	look_at_aliases			(const String&)	const	{ return 0; }
#endif /* ALIASES */
    public:
				object				();
	virtual			~object				();
	dbref			get_id				() const	{ return id; }
	void			set_id				(dbref i)	{ id = i; }

	virtual		void	add_recall_line			(const String& string);
	virtual		void	output_recall			(const int lines, const context * con);
	virtual		void	output_recall_conditional	(String match, const int lines, const context * con);
	virtual		void	ditch_recall			();

	virtual	const	bool	write				(FILE *)		const;
	virtual	const	bool	read				(FILE *);
	virtual	const	bool	destroy				(const dbref);
	/* set functions */

			void	set_flag			(const object_flag_type f)	{ flags[f/8] |= (1<<(f%8)); }
			void	clear_flag			(const object_flag_type f)	{ flags[f/8] &= ~(1<<(f%8)); }
			bool	get_flag			(const object_flag_type f)	{ return (flags[f/8] & (1<<(f%8))) != 0; }
			void	set_flag_byte			(const int c, const flag_type v){ flags[c] = v; }
			flag_type	get_flag_byte		(const int c)			const { return (flags[c]); }

			void	set_referenced			()				{ set_flag(FLAG_REFERENCED); }

			void	set_ctime			()				{ time(&m_ctime); }
			void	set_mtime			()				{ time(&m_mtime); }
			void	set_atime			()				{ time(&m_atime); }

			void	set_ctime			(const time_t t)		{ m_ctime=t; }
			void	set_mtime			(const time_t t)		{ m_mtime=t; }
			void	set_atime			(const time_t t)		{ m_atime=t; }

	virtual		void	set_name			(const String& str);
	virtual		void	set_location			(const dbref o)			{ location = o; }
	virtual		void	set_remote_location		(const dbref o);
			void	set_next			(const dbref o)			{ next = o; }
			void	set_owner_no_check		(const dbref o)			{ m_owner = o; }
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
	virtual		void	set_exits_string		(const String&);
	virtual		void	set_commands			(const dbref o);
	virtual		void	add_command			(const dbref o);
	virtual		void	remove_command			(const dbref o);
	virtual		dbref	find_command			(const String&) const;
	virtual		void	set_info_items			(const dbref o);
	virtual		void	set_fuses			(const dbref o);
	/* Command */
	virtual		void	set_codelanguage		(const String&);
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
	virtual		void	set_colour_string		(const String&);
	virtual		String	get_colour_string		()			const	{return NULLSTRING;}
	virtual		void	set_colour_attr			(ColourAttribute, const String&);
	virtual	const	String&	get_colour_attr			(ColourAttribute)	const	{return NULLSTRING;}
	virtual const	colour_at& get_colour_at		()			const	{return default_colour_at; }
	virtual		void	set_colour_player		(dbref, const String&);
	virtual	const	String&	get_colour_player		(dbref)			const	{return NULLSTRING;}
	virtual const	colour_pl& get_colour_players		()			const	{return default_colour_players; }
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

		const	String& get_name		()			const	{ return (m_name); }
		const	String& name			()			const	{ return (m_name); }
		const	std::list<String>&	get_namelist	()		const	{ return namelist; }
		const	String&	get_inherited_name	()			const;
	virtual	const	dbref	get_location			()			const	{ return location; }
		const	dbref	get_real_location		()			const	{ return location; }
		const	dbref	get_next			()			const	{ return next; }
		const	dbref	get_real_next			()			const	{ return next; }
		const	dbref	get_owner			()			const	{ return m_owner; }
		const	dbref	owner				()			const	{ return m_owner; }
		const	time_t	get_ctime			()			const	{ return m_ctime; }
		const	time_t	get_mtime			()			const	{ return m_mtime; }
		const	time_t	get_atime			()			const	{ return m_atime; }
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
	virtual const std::map<String, dbref>* get_commandmap	()			const	{ return NULL; }

	virtual	const	dbref	get_contents			()			const	{ return NOTHING; }
	virtual	const	dbref	get_destination			()			const	{ return NOTHING; }
	virtual	const	dbref	get_exits			()			const	{ return NOTHING; }
	virtual 	String&	get_exits_string		()			const	{ return NULLSTRING; }
	virtual	const	dbref	get_fuses			()			const	{ return NOTHING; }
	virtual	const	dbref	get_info_items			()			const	{ return NOTHING; }
	virtual	const	dbref	get_variables			()			const	{ return NOTHING; }
	virtual	const	dbref	get_properties			()			const	{ return NOTHING; }
	virtual	const	dbref	get_arrays			()			const	{ return NOTHING; }
	virtual	const	dbref	get_dictionaries		()			const	{ return NOTHING; }
	/* Command */
	virtual const	String&	get_codelanguage		()			const	{ return NULLSTRING; }
	virtual const	String&	get_inherited_codelanguage	()			const	{ return NULLSTRING; }
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
	virtual	const	String&	get_alias			(const int)			const	{ return NULLSTRING; }
	virtual	const	int	remove_alias			(const String&	)		{ return 0; }
	virtual	const	int	add_alias			(const String&	)		{ return 0; }
	virtual	const	int	has_alias			(const String&	)		const	{ return 0; }
	virtual	const	int	has_partial_alias		(const String&	)		const	{ return 0; }
#endif /* ALIASES */
	virtual	const	int	get_pennies			()			const	{ return (0); }
	virtual	const	dbref	get_controller			()			const	{ return (NOTHING); }
	virtual	const	dbref	controller			()			const	{ return (NOTHING); }
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
    	virtual	const	String&	get_element			(const int)			const	{ return (NULLSTRING); }
    	virtual		void	destroy_element			(const int)				{ return; }
    	virtual		void	insert_element			(const int, const String&)		{ return; }
    	/* Array_storage */
    	virtual		void	sort_elements			(int);
	/* Array */
    	virtual		void	set_element			(const int, const String&)	{ return; }
    	virtual	const	int	exist_element			(const int)			const	{ return 0; }
    	/* Dictionary */
    	virtual		void	set_element			(const int, const String&, const String&){ return; }
    	virtual	const	int	exist_element			(const String&)		const	{ return false; }
    	virtual		void	set_index			(const int, const String&);
    	virtual	const	String&	get_index			(const int)			const	{ return (NULLSTRING); }
};


class	object_and_flags
{
    private:
	object_and_flags(const object_and_flags&); // DUMMY
	object_and_flags& operator=(const object_and_flags&); // DUMMY

	typeof_type		type;
	object			*obj;
    public:
    	typeof_type		get_type	()			const	{ return (type); }
	void			set_type	(const typeof_type i)		{ type = i; }
				object_and_flags () : type(TYPE_FREE), obj(NULL){}
	bool			is_free		()			const	{ return (type == TYPE_FREE); }
	object			*get_obj	()			const	{ return (type == TYPE_FREE ? (object*)NULL : obj); }
	void			init_free	()				{ type = TYPE_FREE; }
	void			set_free	()				{ if (type != TYPE_FREE) delete (obj); type = TYPE_FREE; }
	void			set_obj		(object &new_obj, dbref i)		{ type = TYPE_NO_TYPE; obj = &new_obj; obj->set_id(i); }
};


class	Database
{
    private:
	Database(const Database&); // DUMMY
	Database& operator=(const Database&); // DUMMY
	object_and_flags	*array;
	dbref			m_top;
	dbref			free_start;
	Pending_alarm		*alarms;
	int			player_count;
	std::map<String, dbref> player_cache;
	void			grow		(dbref newsize);
	void			build_player_cache	(int player_count);
    public:
				Database	();
				~Database	();
		object		&operator[]	(dbref i)		const	{ return (*(array [i].get_obj ())); }
		object		*operator+	(dbref i)		const	{ return (array [i].get_obj ()); }
	const	dbref		get_top		()			const	{ return m_top; }
	const	dbref		top		()			const	{ return m_top; }
	const	dbref		new_object	(object &obj);
	const	bool		delete_object	(const dbref oldobj);
	const	bool		write		(FILE *f)		const;
	const	dbref		read		(FILE *f);

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


inline typeof_type	Typeof	(dbref x) { return db.get_typeof(x); }
extern	const	dbref			parse_dbref	(const char *);


#define	DOLIST(var, first)	for((var) = (first); (var) != NOTHING; (var) = db[(var)].get_next ())
#define	DOREALLIST(var, first)	for((var) = (first); (var) != NOTHING; (var) = db[(var)].get_real_next ())
#define	PUSH(thing,index,field)	{ db[(thing)].set_next (db[(index)].get_##field ()); db[(index)].set_##field ((thing)); }
#define	getloc(thing)		(db[thing].get_location ())

inline bool Abort		(dbref x) { return db[x].get_flag(FLAG_ABORT); }
inline bool Apprentice		(dbref x) { return db[x].get_flag(FLAG_APPRENTICE); }
inline bool Retired		(dbref x) { return db[x].get_flag(FLAG_RETIRED); }
inline bool Natter		(dbref x) { return db[x].get_flag(FLAG_NATTER); }
inline bool LiteralInput	(dbref x) { return db[x].get_flag(FLAG_LITERALINPUT); }
inline bool Backwards		(dbref x) { return db[x].get_flag(FLAG_BACKWARDS); }
inline bool Abode		(dbref x) { return db[x].get_flag(FLAG_ABODE_OK); }
inline bool Ashcan		(dbref x) { return db[x].get_flag(FLAG_ASHCAN); }
inline bool Censorall		(dbref x) { return db[x].get_flag(FLAG_CENSORALL); }
inline bool Censored		(dbref x) { return db[x].get_flag(FLAG_CENSORED); }
inline bool Censorpublic	(dbref x) { return db[x].get_flag(FLAG_CENSORPUBLIC); }
inline bool Chown_ok		(dbref x) { return db[x].get_flag(FLAG_CHOWN_OK); }
inline bool Colour		(dbref x) { return db[x].get_flag(FLAG_COLOUR); }
inline bool Connected		(dbref x) { return db[x].get_flag(FLAG_CONNECTED); }
inline bool Dark		(dbref x) { return db[x].get_flag(FLAG_DARK); }
inline bool Debug		(dbref x) { return db[x].get_flag(FLAG_DEBUG); }
inline bool Eric		(dbref x) { return db[x].get_flag(FLAG_ERIC); }
inline bool Fchat		(dbref x) { return db[x].get_flag(FLAG_FCHAT); }
inline bool Female		(dbref x) { return db[x].get_flag(FLAG_FEMALE); }
inline bool Fighting		(dbref x) { return db[x].get_flag(FLAG_FIGHTING); }
inline bool Number		(dbref x) { return db[x].get_flag(FLAG_NUMBER); }
inline bool Haven		(dbref x) { return db[x].get_flag(FLAG_HAVEN); }
inline bool Inheritable		(dbref x) { return db[x].get_flag(FLAG_INHERITABLE); }
inline bool Jump		(dbref x) { return db[x].get_flag(FLAG_JUMP_OK); }
inline bool Linenumbers		(dbref x) { return db[x].get_flag(FLAG_LINENUMBERS); }
inline bool Link		(dbref x) { return db[x].get_flag(FLAG_LINK_OK); }
inline bool Listen		(dbref x) { return db[x].get_flag(FLAG_LISTEN); }
inline bool Locked		(dbref x) { return db[x].get_flag(FLAG_LOCKED); }
inline bool Male		(dbref x) { return db[x].get_flag(FLAG_MALE); }
inline bool Neuter		(dbref x) { return db[x].get_flag(FLAG_NEUTER); }
inline bool NoHuhLogs		(dbref x) { return db[x].get_flag(FLAG_NOHUHLOGS); }
inline bool Officer		(dbref x) { return db[x].get_flag(FLAG_OFFICER); }
inline bool Opaque		(dbref x) { return db[x].get_flag(FLAG_OPAQUE); }
inline bool Open		(dbref x) { return db[x].get_flag(FLAG_OPEN); }
inline bool Openable		(dbref x) { return db[x].get_flag(FLAG_OPENABLE); }
inline bool Prettylook		(dbref x) { return db[x].get_flag(FLAG_PRETTYLOOK); }
inline bool Public		(dbref x) { return db[x].get_flag(FLAG_PUBLIC); }
inline bool Puppet		(dbref x) { return db[x].get_controller() != x; }
inline bool Readonly		(dbref x) { return db[x].get_flag(FLAG_READONLY); }
inline bool Referenced		(dbref x) { return db[x].get_flag(FLAG_REFERENCED); }
inline bool Silent		(dbref x) { return db[x].get_flag(FLAG_SILENT); }
inline bool Sticky		(dbref x) { return db[x].get_flag(FLAG_STICKY); }
inline bool Tom			(dbref x) { return db[x].get_flag(FLAG_TOM); }
inline bool Tracing		(dbref x) { return db[x].get_flag(FLAG_TRACING); }


#endif /* _DB_H */

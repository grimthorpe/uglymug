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
#include "colour.h"

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

// special dbrefs
const	dbref	NOTHING		(-1);		///< null dbref
const	dbref	AMBIGUOUS	(-2);		///< multiple possibilities, for matchers
const	dbref	HOME		(-3);		///< virtual room representing mover's home

typedef unsigned char	flag_type;
typedef int		object_flag_type;

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

typedef int		typeof_type;
const typeof_type TYPE_NO_TYPE		(1);
const typeof_type TYPE_FREE		(2);
const typeof_type TYPE_PLAYER		(3);
const typeof_type TYPE_PUPPET		(4);
const typeof_type TYPE_ROOM		(5);
const typeof_type TYPE_THING		(6);
const typeof_type TYPE_EXIT		(7);
const typeof_type TYPE_COMMAND		(8);
const typeof_type TYPE_VARIABLE		(9);
const typeof_type TYPE_PROPERTY		(10);
const typeof_type TYPE_ARRAY		(11);
const typeof_type TYPE_DICTIONARY	(12);
const typeof_type TYPE_ALARM		(13);
const typeof_type TYPE_FUSE		(14);
const typeof_type TYPE_TASK		(15);
const typeof_type TYPE_SEMAPHORE	(16);


/* Flag definitions */
#define FLAG_WIZARD			1
#define FLAG_APPRENTICE 		2
#define FLAG_BUILDER			3
#define FLAG_LISTEN			4
#define FLAG_CONNECTED			5
#define FLAG_ERIC			6
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
/* Flags to allow specific privs */
#define FLAG_NATTER			52
#define	FLAG_RETIRED			53
#define	FLAG_LITERALINPUT		54
#define FLAG_NO_EMAIL_FORWARD		55

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



#ifdef	ABORT_FUSES
#define Abort(x)	(db[(x)].get_flag(FLAG_ABORT) != 0)
#endif
#define Apprentice(x)	(db[(x)].get_flag(FLAG_APPRENTICE) != 0)
#define Retired(x)	(db[(x)].get_flag(FLAG_RETIRED) != 0)
#define Natter(x)	(db[(x)].get_flag(FLAG_NATTER) != 0)
#define LiteralInput(x)	(db[(x)].get_flag(FLAG_LITERALINPUT) != 0)

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
#define Chown_ok(x)	(db[(x)].get_flag(FLAG_CHOWN_OK) !=0)
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
#define NoForwardEmail(x)	(db[(x)].get_flag(FLAG_NO_FORWARD_EMAIL))


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
	unsigned char		flags[FLAGS_WIDTH];	///< Flag list (bits)
//	typeof_type		type;		///< Type of object
	dbref			m_owner;	///< who controls this object
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

			void	set_flag			(const int f)			{ flags[f/8] |= (1<<(f%8)); }
			void	clear_flag			(const int f)			{ flags[f/8] &= ~(1<<(f%8)); }
			int	get_flag			(const int f)		const	{ return(flags[f/8] & (1<<(f%8))); }
			void	set_flag_byte			(const int c, const flag_type v){ flags[c] = v; }
			unsigned char	get_flag_byte		(const int c)		const	{ return (flags[c]); }


			void	set_referenced			()				{ set_flag(FLAG_REFERENCED); }
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


#ifdef GRIMTHORPE_CANT_CODE
#define	CACHE_INVALID	0
#define	CACHE_VALID	1

struct player_cache_struct
{
	String		name;
	dbref		player;
	int		state;

	player_cache_struct() : name(), player(NOTHING), state(CACHE_INVALID) {}
	int compare(const player_cache_struct* other) const;
};
#endif

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
#ifdef GRIMTHORPE_CANT_CODE
	struct player_cache_struct *player_cache;
	struct changed_player_list_struct
	{
		char	*name;
		dbref	player;
		struct	changed_player_list_struct	*next;
		struct	changed_player_list_struct	*prev;
	} *changed_player_list;
#else
	std::map<String, dbref> player_cache;
#endif
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


extern	const	dbref			parse_dbref	(const char *);


#define	DOLIST(var, first)	for((var) = (first); (var) != NOTHING; (var) = db[(var)].get_next ())
#define	DOREALLIST(var, first)	for((var) = (first); (var) != NOTHING; (var) = db[(var)].get_real_next ())
#define	PUSH(thing,index,field)	{ db[(thing)].set_next (db[(index)].get_##field ()); db[(index)].set_##field ((thing)); }
#define	getloc(thing)		(db[thing].get_location ())

#endif /* _DB_H */

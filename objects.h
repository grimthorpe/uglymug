/*
 * objects.h: Bodies of things in db.h that are of no interest to
 *	the majority of modules - eg. how the inheritance is
 *	arranged.
 *
 *	Peter Crowther, 14/8/94.
 */

#ifndef	_OBJECTS_H
#define	_OBJECTS_H

#include "config.h"

#pragma interface

class	Information
: public object
{
private:
	Information(const Information&); // DUMMY
	Information& operator=(const Information&); // DUMMY
	protected:
		int		number;			/* Number of elements in array */
		String		*elements;		/* Elements for array */
	public:
				Information		();
	const	unsigned int	get_number_of_elements	()			const	{ return number; }
};

class	Array_storage
:public Information
{
private:
	Array_storage(const Array_storage&); // DUMMY
	Array_storage& operator=(const Array_storage&); // DUMMY
	private:
		int		size;
	public:
				Array_storage		();
	virtual			~Array_storage		();
		void		empty_object		();
		void		resize			(const int newsize);
	virtual	void		set_element		(const int element, const String& string);
	const	String&		get_element		(const int element)	const;

		void		destroy_element		(const int element);
	virtual const	int		exist_element		(const int)		const;
		void		insert_element		(const int, const String&);
	const	bool		destroy			(const dbref);
    		void		set_size		(const int value)	{ size = value; }
		int		get_size		()			{ return size; }
	const	bool		write			(FILE *f)		const;
//	const	bool		read			(FILE* f);
		void		sort_elements		(int);
};

class	Dictionary
: public Information
{
private:
	Dictionary(const Dictionary&); // DUMMY
	Dictionary& operator=(const Dictionary&); // DUMMY
    private:
		String		*indices;		/* Index strings */
    public:
				Dictionary		();
	virtual			~Dictionary		();
		void		empty_object		();
		void		resize			(const int newsize);
	const	String&		get_element		(const int element)	const;
	virtual	void		set_element		(const int element, const String& index, const String& string);
		void		destroy_element		(const int element);
	virtual const	int		exist_element		(const String& )		const;
	const	bool		destroy			(const dbref);
	const	String&		get_index		(const int)		const;
		void		set_index		(const int, const String& );
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
	const	dbref		get_next		()			const;
};


class	Array
: public Array_storage
{
private:
	Array(const Array&); // DUMMY
	Array& operator=(const Array&); // DUMMY
    public:
	Array() {}
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
	const	dbref		get_next		()			const;
};


class	Task
: public object
{
};


class	Describable_object
: public Array_storage
{
private:
	Describable_object(const Describable_object&); // DUMMY
	Describable_object& operator=(const Describable_object&); // DUMMY
    public:
	Describable_object() {}
	virtual			~Describable_object	();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
		void		set_description		(const String&);
	const	String&		get_description	()			const;
};


class	Lockable_object
: public Describable_object
{
private:
	Lockable_object(const Lockable_object&); // DUMMY
	Lockable_object& operator=(const Lockable_object&); // DUMMY
    private:
		boolexp		*key;		/* if not TRUE_BOOLEXP, must have this to do op */
		String		fail_message;	/* what you see if op fails */
		String		drop_message;	/* what you see if this thing is dropped */
		String		succ_message;	/* what you see if op succeeds */
		String		ofail;		/* what others see if op fails */
		String		osuccess;	/* what others see if op succeeds */
		String		odrop;		/* what others see if thing is dropped */
    public:
				Lockable_object		();
	virtual			~Lockable_object	();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
		void		set_key			(boolexp *);
		void		set_key_no_free		(boolexp *k)			{ key = k; }
		void		set_fail_message	(const String& str);
		void		set_drop_message	(const String& str);
		void		set_succ_message	(const String& str);
		void		set_ofail		(const String& str);
		void		set_osuccess		(const String& str);
		void		set_odrop		(const String& str);
	const	boolexp		*get_key		()			const	{ return key; }
		boolexp		*get_key_for_edit	()			const	{ return key; }
	const	String&		get_fail_message	()			const	{ return fail_message; }
	const	String&		get_drop_message	()			const	{ return drop_message; }
	const	String&		get_succ_message	()			const	{ return succ_message; }
	const	String&		get_ofail		()			const	{ return ofail; }
	const	String&		get_osuccess		()			const	{ return osuccess; }
	const	String&		get_odrop		()			const	{ return odrop; }
};


class	Inheritable_object
: public Lockable_object
{
private:
	Inheritable_object(const Inheritable_object&); // DUMMY
	Inheritable_object& operator=(const Inheritable_object&); // DUMMY
    private:
	dbref			parent;
    public:
				Inheritable_object	() : parent(NOTHING)		{ }
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
		void		set_parent		(const dbref p);
		void		set_parent_no_check	(const dbref p)			{ parent = p; }
	const	dbref		get_parent		()			const	{ return (parent); }
	const String&		get_inherited_element	(const int)	const;
	const int		exist_inherited_element	(const int)	const;
	const unsigned int	get_inherited_number_of_elements(void)	const;
};


class	Old_object
: public Inheritable_object
{
private:
	Old_object(const Old_object&); // DUMMY
	Old_object& operator=(const Old_object&); // DUMMY
    private:
	dbref			commands;	/* Commands attached to this object */
	std::map<String, dbref>	commandmap;
	dbref			contents;	/* pointer to first item */
	String			contents_string;
	dbref			exits;		/* pointer to first exit for rooms */
	String			exits_string;	/* A String to display instead of 'Obvious exits:' */
	dbref			fuses;		/* Fuses and alarms attached to this object */
	dbref			info_items;	/* pointer to first variable for this object */
	dbref			destination;	/* pointer to destination of exits */
    public:
				Old_object		();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
		void		set_destination		(const dbref o);
		void		set_destination_no_check(const dbref o)			{ destination = o; }
		void		set_contents		(const dbref o)			{ contents = o; }
		void		set_exits		(const dbref o)			{ exits = o; }
		void		set_commands		(const dbref o)			{ commands = o; }
		void		add_command		(const dbref o);
		void		remove_command		(const dbref o);
		dbref		find_command		(const String& n) const;
		void		set_info_items		(const dbref o)			{ info_items = o; }
		void		set_fuses		(const dbref o)			{ fuses = o; }
	const	dbref		get_commands		()			const	{ return commands; }
	const	std::map<String, dbref>*get_commandmap	()			const	{ return &commandmap; }
	const	dbref		get_contents		()			const	{ return contents; }
		void		set_contents_string	(const String& c);
	const	String&		get_contents_string	()			const	{ return (contents_string); }
	const	dbref		get_destination		()			const	{ return destination; }
	const	dbref		get_exits		()			const	{ return exits; }
	const	dbref		get_fuses		()			const	{ return fuses; }
	const	dbref		get_info_items		()			const	{ return info_items; }
	const	dbref		get_variables		()			const;
	const	dbref		get_properties		()			const;
	const	dbref		get_arrays		()			const;
	const	dbref		get_dictionaries	()			const;
	const	bool		empty_an_object		(dbref, dbref);
};


class	Massy_object
: public Old_object

{
private:
	Massy_object(const Massy_object&); // DUMMY
	Massy_object& operator=(const Massy_object&); // DUMMY
    private:
	double			gravity_factor;
	double			mass;
	double			volume;
	double			mass_limit;
	double			volume_limit;
    public:
				Massy_object		();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
		void		set_gravity_factor	(const double g)		{ gravity_factor = g; }
		void		set_mass		(const double m)		{ mass = m; }
		void		set_volume		(const double v)		{ volume = v; }
		void		set_mass_limit		(const double m)		{ mass_limit = m; }
		void		set_volume_limit	(const double v)		{ volume_limit = v; }
	const	double		get_gravity_factor	()			const	{ return (gravity_factor); }
	const	double		get_mass		()			const	{ return (mass); }
	const	double		get_volume		()			const	{ return (volume); }
	const	double		get_mass_limit		()			const	{ return (mass_limit); }
	const	double		get_volume_limit	()			const	{ return (volume_limit); }
};


class	Alarm
: public Old_object

{
private:
	Alarm(const Alarm&); // DUMMY
	Alarm& operator=(const Alarm&); // DUMMY
    public:
	Alarm() {}
	const	bool		destroy			(const dbref);
	const	dbref		get_csucc		()			const	{ return (get_destination ()); }
		void		set_csucc		(const dbref o)			{ set_destination (o); }
};


class	Command
: public Old_object

{
private:
	Command(const Command&); // DUMMY
	Command& operator=(const Command&); // DUMMY
    private:
		unsigned short	*parse_helper;
		unsigned	lines_in_command_block(const unsigned) const;
		unsigned	reconstruct_command_block(char *const, const unsigned, const unsigned)	const;
    public:
				Command			()	: parse_helper (0)	{}
	virtual			~Command		();
	const	bool		destroy			(const dbref);
		void		set_name		(const String&);
		void		set_location		(const dbref);
	const	dbref		get_csucc		()			const	{ return (get_contents ()); }
	const	dbref		get_cfail		()			const	{ return (get_exits ()); }
		void		set_csucc		(const dbref o);
		void		set_cfail		(const dbref o);
		void		set_description		(const String&);
	const	unsigned short	get_parse_helper	(const unsigned int index)	const;
		void		set_parse_helper	(const unsigned int index, const unsigned short value);
	const	bool		alloc_parse_helper	();
		void		flush_parse_helper	();
		unsigned	inherited_lines_in_cmd_blk(const unsigned) const;
		unsigned	reconstruct_inherited_command_block(char *const, const unsigned, const unsigned)	const;
};


class	Exit
: public Old_object

{
private:
	Exit(const Exit&); // DUMMY
	Exit& operator=(const Exit&); // DUMMY
    public:
	Exit() {}
	const	bool		destroy			(const dbref);
};


class	Fuse
: public Old_object

{
private:
	Fuse(const Fuse&); // DUMMY
	Fuse& operator=(const Fuse&); // DUMMY
    public:
	Fuse() {}
	const	bool		destroy			(const dbref);
	const	dbref		get_csucc		()			const	{ return (get_contents ()); }
	const	dbref		get_cfail		()			const	{ return (get_exits ()); }
		void		set_csucc		(const dbref o);
		void		set_cfail		(const dbref o);
};

class	puppet
: public Massy_object
{
private:
	puppet(const puppet&); // DUMMY
	puppet& operator=(const puppet&); // DUMMY
    private:
	int			pennies;
	long			score;
	long			last_name_change;	/*Last time they changed their name*/
	int			build_id;		/*The build id*/
	String			race;
	String			who_string;
   	String			email_addr;		/* email address of the player */
	int			money;
	dbref			remote_location;
	struct	fsm_state
	{
		dbref		player;				/* who's talking to us... */
		dbref		state;				/* ...and the state we're in with them */
		time_t		lastused;			/* so we can time them out */

		struct fsm_state *next;
		struct fsm_state *prev;
	}			*fsm_states;

	struct fsm_state	*find_state		(dbref);
	struct fsm_state	*make_state		(dbref);
	void			delete_state		(struct fsm_state *);
	void			fsm_error		(const char *fmt, ...);

    public:
				puppet			();
	virtual			~puppet			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
	virtual	const	dbref		get_location		()			const	{ return remote_location; }
	virtual		void		set_location		(const dbref loc)	{ Massy_object::set_location(loc); remote_location = loc; }
	virtual		void		set_remote_location	(const dbref loc)	{ remote_location = loc; }
		void		set_pennies		(const int i)			{ pennies=i; }
		void		add_pennies		(const int i)			{ pennies+=i; }
	const	dbref		get_build_id		()			const	{ return build_id; }
		void		set_build_id		(const dbref c);
		void		reset_build_id		(const dbref c);
		void		set_email_addr		(const String& addr);
		void		set_score		(const long v)			{ score = v; }
		void		set_last_name_change	(const long v)			{ last_name_change = v; }
		void		set_race		(const String& r);
		void		set_who_string		(const String& s);
		void		set_money		(const int i)			{ money=i; }

	const	int		get_pennies		()			const	{ return (pennies); }
	const	dbref		get_controller		()			const	{ return owner(); }
	const	dbref		controller		()			const	{ return owner(); }
	const	String&		get_email_addr		()			const	{ return (email_addr); }
	const	String&		get_race		()			const	{ return (race); }
	const	long		get_score		()			const	{ return (score); }
	const	long		get_last_name_change	()			const	{ return (last_name_change); }
	const	String&		 get_who_string		()			const	{ return (who_string); }
	const	int		get_money		()			const	{ return (money); }

#if 0	/* PJC 24/1/97 */
		void		event			(const dbref player, const dbref npc, const char *e);
#endif
	const	bool		destroy			(const dbref);
};

class	Player
: public puppet

{
private:
	Player(const Player&); // DUMMY
	Player& operator=(const Player&); // DUMMY
    private:
	struct RecallBuffer
	{
	//Items for the circular buffer to store lines for @recall - Reaps
		String	buffer[400];		// The buffer
		int	buffer_next;		//Next place to put a line
		bool	buffer_wrapped;		//Have we wrapped the buffer yet
		char	buffer_build[BUFFER_LEN];//Buffer to build lines
		RecallBuffer() : buffer_next(0), buffer_wrapped(false) { buffer_build[0] = 0; }
	};
	RecallBuffer*		recall;

#ifdef ALIASES
	String			alias[MAX_ALIASES];
#endif
	String			password;
	dbref			m_controller;
#ifdef ALIASES
		void		set_alias		(const int which, const String& what);
	const	int		look_at_aliases		(const String& string)	const;
#endif /* ALIASES */

	 /* Colour struct */
	colour_at		colour_attrs;
	mutable colour_pl		colour_players;

	Channel*		channel;

    public:
				Player			();
	virtual			~Player			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);


		void		add_recall_line		(const String& strung);
		void		output_recall		(const int lines, const context * con);
		void		output_recall_conditional (String match, const int lines, const context * con);
		void		ditch_recall		();

		void		set_colour_string	(const String&);
		String		get_colour_string	()			const;
		void		set_colour_attr		(ColourAttribute, const String&);
	const	String&		get_colour_attr		(ColourAttribute) 	const;
	const	colour_at&	get_colour_at		()			const	{ return colour_attrs; }
		void		set_colour_player	(dbref, const String&);
	const	String&		get_colour_player	(dbref)			const;
	const	colour_pl&	get_colour_players	()			const	{return colour_players; }
		void		set_controller		(const dbref c);
		void		set_password		(const String& p);
		void		set_channel		(Channel *c)		{channel = c;}
		Channel *	get_channel		()			const	{return channel;}

#ifdef ALIASES
	const	String&		get_alias		(const int which)	const	{ return (alias[which]); }
	const	int		remove_alias		(const String& string);
	const	int		add_alias		(const String& string);
	const	int		has_alias		(const String& string)	const;
	const	int		has_partial_alias	(const String& string)	const;
#endif /* ALIASES */

	const	dbref		get_controller		()			const	{ return m_controller; }
	const	dbref		controller		()			const	{ return m_controller; }
	const	String&		get_password		()			const	{ return (password); }
	const	double		get_inherited_mass	()			const;
	const	double		get_inherited_volume	()			const;
	const	double		get_inherited_mass_limit ()			const;
	const	double		get_inherited_volume_limit ()			const;
	const	double		get_inherited_gravity_factor ()			const;
};


class	Room
: public Massy_object

{
private:
	Room(const Room&); // DUMMY
	Room& operator=(const Room&); // DUMMY
    private:
        time_t			last_entry_time;
    public:
				Room			();
				~Room			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
	const	bool		destroy			(const dbref);
		void		set_last_entry_time	()				{ time (&last_entry_time); }
	const	time_t		get_last_entry_time	()			const	{ return last_entry_time; }
	const	double		get_inherited_mass	()			const;
	const	double		get_inherited_volume	()			const;
	const	double		get_inherited_mass_limit ()			const;
	const	double		get_inherited_volume_limit ()			const;
	const	double		get_inherited_gravity_factor ()			const;

};


class	Thing
: public Massy_object

{
private:
	Thing(const Thing&); // DUMMY
	Thing& operator=(const Thing&); // DUMMY
    private:
		boolexp		*lock_key;		/* the objects required to lock/unlock a container */
    public:
				Thing			();
	virtual			~Thing			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE* f);
		void		set_lock_key		(boolexp *k);
		void		set_lock_key_no_free	(boolexp *k)			{ lock_key=k; }
	const	boolexp		*get_lock_key		()			const	{ return (lock_key); }
		boolexp		*get_lock_key_for_edit	()			const	{ return (lock_key); }
	const	bool		destroy			(const dbref);
	const	double		get_inherited_mass	()			const;
	const	double		get_inherited_volume	()			const;
	const	double		get_inherited_mass_limit ()			const;
	const	double		get_inherited_volume_limit ()			const;
	const	double		get_inherited_gravity_factor ()			const;
};


class	Variable
: public Lockable_object

{
private:
	Variable(const Variable&); // DUMMY
	Variable& operator=(const Variable&); // DUMMY
    public:
	Variable() {}
	const	bool		destroy			(const dbref);
	const	dbref		get_next		()			const;
};

class	Property
: public Describable_object
{
private:
	Property(const Property&); // DUMMY
	Property& operator=(const Property&); // DUMMY
    public:
	Property() {}
	const	bool		destroy			(const dbref);
	const	dbref		get_next		()			const;
};
#endif	/* !_OBJECTS_H */

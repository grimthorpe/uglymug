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
		int		size;
	public:
				Array_storage		();
	virtual			~Array_storage		();
		void		empty_object		();
		void		resize			(const int newsize);
		void		set_element		(const int element, const CString& string);
	const	String&		get_element		(const int element)	const;

		void		destroy_element		(const int element);
	const	int		exist_element		(const int)		const;
		void		insert_element		(const int, const CString&);
	const	bool		destroy			(const dbref);
    		void		set_size		(const int value)	{ size = value; }
		int		get_size		()			{ return size; }
	const	bool		write			(FILE *f)		const;
//	const	bool		read			(FILE *f, const int db_version, const int iformat);
		void		sort_elements		(int);
};

class	Dictionary
: public Information
{
    private:
		String		*indices;		/* Index strings */
    public:
				Dictionary		();
	virtual			~Dictionary		();
		void		empty_object		();
		void		resize			(const int newsize);
	const	String&		get_element		(const int element)	const;
		void		set_element		(const int element, const CString& index, const CString& string);
		void		destroy_element		(const int element);
	const	int		exist_element		(const CString& )		const;
	const	bool		destroy			(const dbref);
	const	String&		get_index		(const int)		const;
		void		set_index		(const int, const CString& );
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
	const	dbref		get_next		()			const;
};


class	Array
: public Array_storage
{
    public:
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
	const	dbref		get_next		()			const;
};


class	Task
: public object
{
};


class	Describable_object
: public Array_storage
{
    public:
	virtual			~Describable_object	();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, int db_version, int iformat);
		void		set_description		(const CString&);
	const	String&		get_description	()			const;
};


class	Lockable_object
: public Describable_object
{
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
	const	bool		read			(FILE *f, const int db_version, const int iformat);
		void		set_key			(boolexp *);
		void		set_key_no_free		(boolexp *k)			{ key = k; }
		void		set_fail_message	(const CString& str);
		void		set_drop_message	(const CString& str);
		void		set_succ_message	(const CString& str);
		void		set_ofail		(const CString& str);
		void		set_osuccess		(const CString& str);
		void		set_odrop		(const CString& str);
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
	dbref			parent;
    public:
				Inheritable_object	()				{ parent = NOTHING; }
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
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
	dbref			commands;	/* Commands attached to this object */
	dbref			contents;	/* pointer to first item */
	dbref			exits;		/* pointer to first exit for rooms */
	dbref			fuses;		/* Fuses and alarms attached to this object */
	dbref			info_items;	/* pointer to first variable for this object */
	dbref			destination;	/* pointer to destination of exits */
    public:
				Old_object		();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
	const	bool		read_pre12		(FILE *, const int);
		void		set_destination		(const dbref o);
		void		set_destination_no_check(const dbref o)			{ destination = o; }
		void		set_contents		(const dbref o)			{ contents = o; }
		void		set_exits		(const dbref o)			{ exits = o; }
		void		set_commands		(const dbref o)			{ commands = o; }
		void		set_info_items		(const dbref o)			{ info_items = o; }
		void		set_fuses		(const dbref o)			{ fuses = o; }
	const	dbref		get_commands		()			const	{ return commands; }
	const	dbref		get_contents		()			const	{ return contents; }
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
	double			gravity_factor;
	double			mass;
	double			volume;
	double			mass_limit;
	double			volume_limit;
    public:
				Massy_object		();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
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
    public:
	const	bool		destroy			(const dbref);
	const	dbref		get_csucc		()			const	{ return (get_destination ()); }
		void		set_csucc		(const dbref o)			{ set_destination (o); }
};


class	Command
: public Old_object

{
    private:
		unsigned short	*parse_helper;
		unsigned	lines_in_command_block(const unsigned) const;
		unsigned	reconstruct_command_block(char *const, const unsigned, const unsigned)	const;
    public:
				Command			()	: parse_helper (0)	{}
	virtual			~Command		();
	const	bool		destroy			(const dbref);
	const	dbref		get_csucc		()			const	{ return (get_contents ()); }
	const	dbref		get_cfail		()			const	{ return (get_exits ()); }
		void		set_csucc		(const dbref o);
		void		set_cfail		(const dbref o);
		void		set_description		(const CString&);
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
    public:
	const	bool		destroy			(const dbref);
};


class	Fuse
: public Old_object

{
    public:
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
	int			pennies;
	long			score;
	long			last_name_change;	/*Last time they changed their name*/
	int			build_id;		/*The build id*/
	String			race;
	String			who_string;
   	String			email_addr;		/* email address of the player */
	int			money;
	struct	descriptor_data	**connections;		/* pointer to array of descriptors interested in output */
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
	const	bool		read			(FILE *f, const int db_version, const int iformat);
		void		set_pennies		(const int i)			{ pennies=i; }
		void		add_pennies		(const int i)			{ pennies+=i; }
	const	dbref		get_build_id		()			const	{ return build_id; }
		void		set_build_id		(dbref c);
		void		reset_build_id		(dbref c);
		void		set_email_addr		(const CString& addr);
		void		set_score		(const long v)			{ score = v; }
		void		set_last_name_change	(const long v)			{ last_name_change = v; }
		void		set_race		(const CString& r);
		void		set_who_string		(const CString& s);
		void		set_money		(const int i)			{ money=i; }

	const	colour_at&	get_colour_at		()			const	{ return default_colour_at; }
	const	int		get_pennies		()			const	{ return (pennies); }
	const	dbref		get_controller		()			const	{ return (get_owner()); }
	const	String&		get_email_addr		()			const	{ return (email_addr); }
	const	String&		get_race		()			const	{ return (race); }
	const	long		get_score		()			const	{ return (score); }
	const	long		get_last_name_change	()			const	{ return (last_name_change); }
	const	String&		 get_who_string		()			const	{ return (who_string); }
	const	int		get_money		()			const	{ return (money); }

#if 0	/* PJC 24/1/97 */
		void		event			(const dbref player, const dbref npc, const char *e);
#endif
	const	bool		destroy			(dbref);
};

class	Player
: public puppet

{
    private:
	//Items for the circular buffer to store lines for @recall - Reaps
	char* 			recall_buffer[MAX_RECALL_LINES]; //the buffer
	int			recall_buffer_next;		 //Next place to put a line
	int			recall_buffer_wrapped;			 //Have we wrapped the buffer yet
	char			recall_buffer_build[BUFFER_LEN]; //Buffer to build lines
	

#ifdef ALIASES
	String			alias[MAX_ALIASES];
#endif
	String			password;
	dbref			controller;
#ifdef ALIASES
		void		set_alias		(const int which, const CString& what);
	const	int		look_at_aliases		(const CString& string)	const;
#endif /* ALIASES */

	/* Combat stats */
	dbref			weapon;
	dbref			armour;
	int			strength;
	int			dexterity;
	int			constitution;
	int			hit_points;
	int			max_hit_points;
	int			experience;
	int			last_attack_time;

	 /* Colour struct */
    	char *			colour;
	colour_at*		col_at;
	cplay *			colour_play;
	int			colour_play_size;

	Channel*		channel;

    public:
				Player			();
	virtual			~Player			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, int db_version, int iformat);


		void		add_recall_line		(const CString& strung);
		void		output_recall		(const int lines, const context * con);

		void		set_colour		(const char *new_colour);
		CString		get_colour		()			const	{return colour;}
		void		set_colour_at		(colour_at* c)		{ if(c != col_at) delete col_at; col_at = c; }
	const	colour_at&	get_colour_at		()  	   		const	{ return col_at?(*col_at):default_colour_at; }
		void		set_colour_play		(cplay *c)  			{if(colour_play) delete[] colour_play; colour_play = c;}
	const	cplay *		get_colour_play		()			const	{return colour_play;}
		void		set_colour_play_size	(int c)				{colour_play_size=c;}
	const	int		get_colour_play_size	()			const	{return colour_play_size;}
		void		set_controller		(dbref c);
		void		set_password		(const CString& p);
		void		set_channel		(Channel *c)		{channel = c;}
		Channel *	get_channel		()			const	{return channel;}

#ifdef ALIASES
	const	String&		get_alias		(const int which)	const	{ return (alias[which]); }
	const	int		remove_alias		(const CString& string);
	const	int		add_alias		(const CString& string);
	const	int		has_alias		(const CString& string)	const;
	const	int		has_partial_alias	(const CString& string)	const;
#endif /* ALIASES */

	const	dbref		get_controller		()			const	{ return (controller); }
	const	String&		get_password		()			const	{ return (password); }
	const	double		get_inherited_mass	()			const;
	const	double		get_inherited_volume	()			const;
	const	double		get_inherited_mass_limit ()			const;
	const	double		get_inherited_volume_limit ()			const;
	const	double		get_inherited_gravity_factor ()			const;

	const	dbref		get_weapon		()			const	{return weapon;}
	const	dbref		get_armour		()			const	{return armour;}
	const	int		get_strength		()			const	{return strength;}
	const	int		get_dexterity		()			const	{return dexterity;}
	const	int		get_constitution	()			const	{return constitution;}
	const	int		get_max_hit_points	()			const	{return max_hit_points;}
	const	int		get_hit_points		()			const	{return hit_points;}
	const	int		get_experience		()			const	{return experience;}
	const	int		get_last_attack_time	()			const	{return last_attack_time;}
	const	int		get_level		()			const;
		void		set_weapon		(const dbref i);
		void		set_armour		(const dbref i);
		void		set_strength		(const int i)			{strength = i;}
		void		set_dexterity		(const int i)			{dexterity = i;}
		void		set_constitution	(const int i)			{constitution = i;}
		void		set_max_hit_points	(const int i)			{max_hit_points = i;}
		void		set_hit_points		(const int i)			{hit_points = i;}
		void		set_experience		(const int i)			{experience = i;}
		void		set_last_attack_time	(const int i)			{last_attack_time = i;}
};


class	Room
: public Massy_object

{
    private:
	String			contents_string;
        time_t			last_entry_time;
    public:
				Room			();
				~Room			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
	const	bool		destroy			(const dbref);
		void		set_last_entry_time	()				{ time (&last_entry_time); }
		void		set_contents_string	(const CString& c);
	const	String&		get_contents_string	()			const	{ return (contents_string); }
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
		String		contents_string;	/* the string shown when the container is looked at and contains something */
		boolexp		*lock_key;		/* the objects required to lock/unlock a container */
    public:
				Thing			();
	virtual			~Thing			();
	const	bool		write			(FILE *f)		const;
	const	bool		read			(FILE *f, const int db_version, const int iformat);
		void		set_contents_string	(const CString& c);
		void		set_lock_key		(boolexp *k);
		void		set_lock_key_no_free	(boolexp *k)			{ lock_key=k; }
	const	String&		get_contents_string	()			const	{ return (contents_string); }
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
    public:
	const	bool		destroy			(const dbref);
	const	dbref		get_next		()			const;
};

class	Property
: public Describable_object
{
    public:
	const	bool		destroy			(const dbref);
	const	dbref		get_next		()			const;
};

class Weapon
: public Inheritable_object
{
    private:
		int		degradation;
		int		damage;
		int		speed;
		int		ammunition;
		int		range;
		dbref		parent_ammunition;

    public:
				Weapon			();
	const	bool		write			(FILE *)		const;
	const	bool		read			(FILE *, const int, const int iformat);
	const	bool		destroy			(const dbref);
		void		set_degradation(int i)		{degradation = i;}
		int		get_degradation()		{return(degradation);}
		void		set_damage(int i)		{damage = i;}
		int		get_damage()			{return(damage);}
		void		set_speed(int i)		{speed = i;}
		int		get_speed()			{return(speed);}
		void		set_ammunition(int i)		{ammunition = i;}
		int		get_ammunition()		{return(ammunition);}
		void		set_range(int i)		{range = i;}
		int		get_range()			{return(range);}
		void		set_ammo_parent(dbref i);
		dbref		get_ammo_parent()		{return(parent_ammunition);}
};


class Ammunition
: public Inheritable_object
{
    private:
		int	count;

    public:
				Ammunition();
	const	bool		write			(FILE *)		const;
	const	bool		read			(FILE *, const int, const int iformat);
	const	bool		destroy			(const dbref);
		void		set_count(int i)	{count = i;}
		int		get_count()		{return(count);}
};

class Armour
: public Inheritable_object
{
    private:
		int	degradation;
		int	protection;

    public:
				Armour();
	const	bool		write			(FILE *)		const;
	const	bool		read			(FILE *, const int, const int iformat);
	const	bool		destroy			(const dbref);
		void		set_degradation(int i)	{degradation = i;}
		int		get_degradation()	{return(degradation);}
		void		set_protection(int i)	{protection = i;}
		int		get_protection()	{return(protection);}
};

#endif	/* !_OBJECTS_H */

/* static char SCCSid[] = "@(#)objects.c	1.34\t7/27/95"; */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <time.h>
#pragma implementation "objects.h"

#include "externs.h"
#include "db.h"
#include "objects.h"
#include "interface.h"
#include "log.h"
#include "regexp_interface.h"


/* DB_GROWSIZE must be a power of 2 */
#define DB_GROWSIZE	1024
#define	ASSIGN_STRING(d,s)	{ if ((d)) free ((d));  if ((s && *s!=0)) (d)=strdup((s)); else (d)=NULL; }
#define	IMPLEMENTATION_ERROR(str)	log_bug("Illegal access to %s", str);

/* This realloc macro is made for the dictionaries and arrays types. It is quite specific */
#define REALLOC(p,s)		{ ((p)) ? ((p) = (char **) realloc ((p),(s))) : ((p) = (char **) malloc((s))); }


static	void			destroy_all_commands		(dbref);
static	void			destroy_all_exits		(dbref);
static	void			destroy_all_fuses		(dbref);
static	void			destroy_all_variables		(dbref);


Database	db;


/************************************************************************/
/*									*/
/*			Utility functions for objects			*/
/*									*/
/************************************************************************/


/*
 * alloc_string: Equivalent to strdup(), with the exception that NULL
 *	or empty strings are mapped to NULL.
 *
 * Return value:
 *	The address of the new string, or NULL.
 */

char *
alloc_string (
const	char	*string)

{
	char	*s;

	/* NULL, "" -> NULL */
	if((string == NULL) || (*string == '\0'))
		return NULL;

	if((s = strdup (string)) == NULL)
	{
		log_bug("Fatal: alloc_string: strdup failed");
		abort();
	}
	return s;
}


/************************************************************************/
/*									*/
/*			class Database					*/
/*									*/
/************************************************************************/


Database::Database ()
: array(NULL)
, m_top(NOTHING)
, free_start(0)
, alarms(NULL)
, player_count(0)
#ifdef GRIMTHORPE_CANT_CODE
, player_cache(NULL)
, changed_player_list(NULL)
#else
, player_cache()
#endif
{
}
/*
 * ~Database: Set free all our objects. This relies on
 *	object_and_flags::set_free() to call the destructors.
 */

Database::~Database ()

{
	dbref	i;

	if (array != NULL)
	{
		for (i = 0; i < top (); i++)
			array [i].set_free ();
		free (array);
	}
}


/*
 * grow: Make at least newtop entries in the array. This should work
 *	regardless of the current state of the database (new or old).
 *	It keeps old entries.
 */

void
Database::grow (
const	dbref	newtop)

{
	int	index;
	dbref	real_newtop = (array == NULL) ? (int)newtop : (newtop + (DB_GROWSIZE - 1)) & (~(DB_GROWSIZE - 1));

	if(newtop > m_top)
	{
		/* If the database exists, extend it; else make one */
		if(array != NULL)
		{
			/* Extend it */
			if((array = (object_and_flags *) realloc(array, real_newtop * sizeof(object_and_flags))) == NULL)
			{
				log_bug("Fatal: grow: realloc failed extending database\n");
				abort();
			}
		}
		else
		{
			/* make the initial one */
			m_top = 0;
			if((array = (object_and_flags *) malloc(real_newtop * sizeof(object_and_flags))) == NULL)
			{
				log_bug("Fatal: grow: malloc failed");
				abort();
			}
		}
		/* Set up the entries. */
		for (index = real_newtop - 1; index >= m_top; index--)
			array [index].init_free();
		free_start = m_top;
		m_top = real_newtop;
	}
}


/*
 * new_object: Get a ref for a new object. This re-uses deleted objects
 *	first, or grows the database if there are no free objects.
 *
 * Return value:
 *	The dbref of the newly-added object.
 */

const dbref
Database::new_object (
object		&obj)

{
	dbref	loc = NOTHING;

	/* free_start is a hint to where a free object is. */
	for(dbref i = free_start; i < m_top; i++)
	{
		if(array[i].is_free())
		{
			loc = i;
			break;
		}
	}
	if(NOTHING == loc)
	{
		grow (m_top + 1);

		/* Should guarantee a free slot this time. */
		loc = free_start;
	}
	free_start=loc+1;
	array [loc].set_obj (obj, loc);

	return loc;
}


/*
 * delete_object: Add an object to the free chain. Relies on set_free()
 *	calling the object's destructor.
 *
 * NOTE: This does NOT do any reference checking to see what other objects
 *	should be deleted.
 *
 * Return value:
 *	true	Success,
 *	false	Failure.
 */

const bool
Database::delete_object (
const dbref	oldobj)

{
	if (oldobj >= 0 && oldobj < m_top)
	{
		if(oldobj < free_start)
			free_start = oldobj;

		array [oldobj].set_free ();

		return true;
	}

	return false;
}

/************************************************************************/
/*									*/
/*			class object					*/
/*									*/
/************************************************************************/


/**
 * Initialise all the fields in a new object.
 */

object::object ()
: id(NOTHING)
, m_name(NULLSTRING)
, namelist()
, location(NOTHING)
, next(NOTHING)
, m_owner(NOTHING)
, m_ctime(0)
, m_mtime(0)
, m_atime(0)
{
	int	i;
	for (i = 0; i < FLAGS_WIDTH; i++)
		flags[i] = '\0';
}


/**
 * Zap the fields in an object. This is a virtual destructor;
 *	subclasses *will* override this destructor.
 */

object::~object ()

{
	// Don't need to do anything now, as all the strings are reference
	// counted, and everything else is automatically destroyed
	set_location(NOTHING);	// This is useful for command maps
}


const String&
object::get_inherited_name ()
const

{
	if (m_name)
		return m_name;
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return db [get_parent ()].get_inherited_name ();
}


const boolexp *
object::get_inherited_key ()
const

{
	if (get_key () != TRUE_BOOLEXP)
		return get_key ();
	else if (get_parent () == NOTHING)
		return TRUE_BOOLEXP;
	else
		return db [get_parent ()].get_inherited_key ();
}


const String&
object::get_inherited_description ()
const

{
	if (get_description ())
		return get_description ();
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return db [get_parent ()].get_inherited_description ();
}


const String&
object::get_inherited_fail_message ()
const

{
	if (get_fail_message ())
		return get_fail_message ();
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return db [get_parent ()].get_inherited_fail_message ();
}


const String&
object::get_inherited_succ_message ()
const

{
	if (get_succ_message ())
		return get_succ_message ();
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return (db [get_parent ()].get_inherited_succ_message ());
}


const dbref
object::get_inherited_cfail ()
const

{
	if (get_cfail () != NOTHING)
		return (get_cfail ());
	else if (get_parent () == NOTHING)
		return (NOTHING);
	else
		return (db [get_parent ()].get_inherited_cfail ());
}


const dbref
object::get_inherited_csucc ()
const

{
	if (get_csucc () != NOTHING)
		return (get_csucc ());
	else if (get_parent () == NOTHING)
		return (NOTHING);
	else
		return (db [get_parent ()].get_inherited_csucc ());
}


const String&
object::get_inherited_drop_message ()
const

{
	if (get_drop_message ())
		return (get_drop_message ());
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return (db [get_parent ()].get_inherited_drop_message ());
}


const String&
object::get_inherited_ofail ()
const

{
	if (get_ofail ())
		return (get_ofail ());
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return (db [get_parent ()].get_inherited_ofail ());
}


const String&
object::get_inherited_osuccess ()
const

{
	if (get_osuccess ())
		return (get_osuccess ());
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return (db [get_parent ()].get_inherited_osuccess ());
}


const String&
object::get_inherited_odrop ()
const

{
	if (get_odrop ())
		return (get_odrop ());
	else if (get_parent () == NOTHING)
		return NULLSTRING;
	else
		return (db [get_parent ()].get_inherited_odrop ());
}

const double
object::get_inherited_mass ()
const

{
	if (get_mass () != NUM_INHERIT)
		return (get_mass ());
	else if (get_parent () == NOTHING)
		return (NUM_INHERIT);
	else
		return (db [get_parent ()].get_inherited_mass ());
}


const double
object::get_inherited_volume ()
const

{
	if (get_volume () != NUM_INHERIT)
		return (get_volume ());
	else if (get_parent () == NOTHING)
		return (NUM_INHERIT);
	else
		return (db [get_parent ()].get_inherited_volume ());
}


const double
object::get_inherited_gravity_factor ()
const

{
	if (get_gravity_factor () != NUM_INHERIT)
		return (get_gravity_factor ());
	else if (get_parent () == NOTHING)
		return (NUM_INHERIT);
	else
		return (db [get_parent ()].get_inherited_gravity_factor ());
}


const double
object::get_inherited_mass_limit ()
const

{
	if (get_mass_limit () != NUM_INHERIT)
		return (get_mass_limit ());
	else if (get_parent () == NOTHING)
		return (NUM_INHERIT);
	else
		return (db [get_parent ()].get_inherited_mass_limit ());
}


const double
object::get_inherited_volume_limit ()
const

{
	if (get_volume_limit () != NUM_INHERIT)
		return (get_volume_limit ());
	else if (get_parent () == NOTHING)
		return (NUM_INHERIT);
	else
		return (db [get_parent ()].get_inherited_volume_limit ());
}


void
object::set_name (
const String& str)
{
	// This is the easy bit...
	m_name = str;

	// Now we'll separate the name into its constituent parts.
	// Mainly useful for commands and exits, but other things might want
	// to use this too.
	namelist.erase(namelist.begin(), namelist.end());
	const char* p1 = m_name.c_str();
	const char* p2 = p1;
	String nameportion;
	for(; *p2; p2++)
	{
		if(*p2 == ';')
		{
			if(p1 != p2)
			{
				String nameportion(p1, (p2 - p1));
				namelist.push_back(nameportion);
			}
			p1 = p2+1;
		}
	}
	if(p1 == m_name.c_str())
	{
		namelist.push_back(m_name);
	}
	else if(*p1)
	{
		namelist.push_back(p1);
	}
}


void
object::set_owner (
const	dbref	o)

{
	m_owner = o;
	if (o != NOTHING)
		db [o].set_referenced ();
}

void object::set_remote_location (const dbref)
{ IMPLEMENTATION_ERROR ("remote_location"); }

void object::set_index(const int, const String&)
{ IMPLEMENTATION_ERROR("index"); }

void object::set_size (const int)
{ IMPLEMENTATION_ERROR ("size"); }

const bool object::destroy (const dbref)
{ IMPLEMENTATION_ERROR ("destroy"); return (true); }

void object::set_description (const String&)
{ IMPLEMENTATION_ERROR ("description") }

void object::set_destination (const dbref)
{ IMPLEMENTATION_ERROR ("destination") }

void object::set_destination_no_check (const dbref)
{ IMPLEMENTATION_ERROR ("destination_no_check") }

void object::set_contents (const dbref)
{ IMPLEMENTATION_ERROR ("contents") }

void object::set_exits (const dbref)
{ IMPLEMENTATION_ERROR ("exits") }

void object::set_fail_message (const String&)
{ IMPLEMENTATION_ERROR ("fail_message") }

void object::set_drop_message (const String&)
{ IMPLEMENTATION_ERROR ("drop_message") }

void object::set_succ_message (const String&)
{ IMPLEMENTATION_ERROR ("succ_message") }

void object::set_ofail (const String&)
{ IMPLEMENTATION_ERROR ("ofail") }

void object::set_osuccess (const String&)
{ IMPLEMENTATION_ERROR ("osuccess") }

void object::set_odrop (const String&)
{ IMPLEMENTATION_ERROR ("odrop") }

void object::set_pennies (const int)
{ IMPLEMENTATION_ERROR ("pennies") }

void object::add_pennies (const int)
{ IMPLEMENTATION_ERROR ("add_pennies") }

void object::set_commands (const dbref)
{ IMPLEMENTATION_ERROR ("commands") }

void object::add_command (const dbref)
{ IMPLEMENTATION_ERROR ("add_command") }

void object::remove_command (const dbref)
{ IMPLEMENTATION_ERROR ("remove_command") }

dbref object::find_command (const String&) const
{ return NOTHING;}

void object::set_info_items (const dbref)
{ IMPLEMENTATION_ERROR ("info_item") }

void object::set_fuses (const dbref)
{ IMPLEMENTATION_ERROR ("fuses") }

void object::set_key (struct boolexp *)
{ IMPLEMENTATION_ERROR ("key") }

void object::set_key_no_free (struct boolexp *)
{ IMPLEMENTATION_ERROR ("key_no_free") }

/* Inheritable_object */
void object::set_parent (const dbref)
{ IMPLEMENTATION_ERROR ("parent") }

/* Inheritable_object */
void object::set_parent_no_check (const dbref)
{ IMPLEMENTATION_ERROR ("parent_no_check") }

const String& object::get_inherited_element(const int) const
{ IMPLEMENTATION_ERROR ("get_inherited_element"); return NULLSTRING; }

const int object::exist_inherited_element(const int) const
{ IMPLEMENTATION_ERROR ("exist_inherited_element"); return 0; }

const unsigned int object::get_inherited_number_of_elements(void) const
{ IMPLEMENTATION_ERROR ("get_inherited_number_of_elements"); return 0; }

/* Old_object */
void object::set_exits_string (const String&)
{ IMPLEMENTATION_ERROR ("exits_string") }

/* Massy_thing */
void object::set_gravity_factor (const double)
{ IMPLEMENTATION_ERROR ("gravity factor") }

void object::set_mass (const double)
{ IMPLEMENTATION_ERROR ("mass") }

void object::set_volume (const double)
{ IMPLEMENTATION_ERROR ("volume") }

void object::set_mass_limit (const double)
{ IMPLEMENTATION_ERROR ("mass limit") }

void object::set_volume_limit (const double)
{ IMPLEMENTATION_ERROR ("volume limit") }

/* Command */

void object::set_parse_helper (const unsigned int, const unsigned short)
{ IMPLEMENTATION_ERROR ("parse helper") }

const bool object::alloc_parse_helper ()
{ IMPLEMENTATION_ERROR ("alloc parse helper") return false; }

void object::flush_parse_helper ()
{ IMPLEMENTATION_ERROR ("flush parse helper") }

unsigned object::inherited_lines_in_cmd_blk(const unsigned) const
{ IMPLEMENTATION_ERROR ("inherited_lines_in_cmd_blk"); return (unsigned)0; }

unsigned object::reconstruct_inherited_command_block(char *const, const unsigned, const unsigned) const
{ IMPLEMENTATION_ERROR ("reconstruct_inherited_command_block"); 
  return (unsigned)0; }

/* Player */

void object::set_channel (Channel *)
{ IMPLEMENTATION_ERROR ("channel") }

void object::set_money (int)
{ IMPLEMENTATION_ERROR ("money") }

void object::set_colour (const char *)
{ IMPLEMENTATION_ERROR ("colour") }

void object::set_colour_at (colour_at*)
{ IMPLEMENTATION_ERROR ("colour_at") }

void object::set_colour_play (cplay *)
{ IMPLEMENTATION_ERROR ("colour_play") }

void object::set_colour_play_size (int)
{ IMPLEMENTATION_ERROR ("colour_play_size") }

void object::set_controller (const dbref)
{ IMPLEMENTATION_ERROR ("controller") }

void object::set_build_id (const dbref)
{ IMPLEMENTATION_ERROR ("build_id") }

void object::reset_build_id (const dbref)
{ IMPLEMENTATION_ERROR ("build_id") }

void object::set_email_addr (const String&)
{ IMPLEMENTATION_ERROR ("email_addr") }

#ifdef ALIASES
void object::set_alias (const int, const String&)
{ IMPLEMENTATION_ERROR ("alias") }
#endif /* ALIASES */

void object::add_recall_line (const String&)
{ IMPLEMENTATION_ERROR ("add_recall_line") }

void object::output_recall_conditional (const String match, const int lines, const context * con)
{ IMPLEMENTATION_ERROR ("output_recall_conditional") }

void object::output_recall (const int lines, const context * con)
{ IMPLEMENTATION_ERROR ("output_recall") }

void object::ditch_recall ()
{ IMPLEMENTATION_ERROR ("ditch_recall") }

void object::set_password (const String&)
{ IMPLEMENTATION_ERROR ("password") }

void object::set_race (const String&)
{ IMPLEMENTATION_ERROR ("race") }

void object::set_score (const long)
{ IMPLEMENTATION_ERROR ("score") }

void object::set_last_name_change (const long)
{ IMPLEMENTATION_ERROR ("last_name_change") }

void object::set_who_string (const String&)
{ IMPLEMENTATION_ERROR ("who_string") }

#if 0	/* PJC 24/1/97 */
void object::event (const dbref, const dbref, const char *)
{ IMPLEMENTATION_ERROR ("event") }
#endif

/* Thing */
void object::set_contents_string (const String&)
{ IMPLEMENTATION_ERROR ("contents_string") }

void object::set_lock_key (struct boolexp *)
{ IMPLEMENTATION_ERROR ("lock_key") }

void object::set_lock_key_no_free (struct boolexp *)
{ IMPLEMENTATION_ERROR ("lock_key") }

void object::set_csucc (const dbref)
{ IMPLEMENTATION_ERROR ("csucc") }

void object::set_cfail (const dbref)
{ IMPLEMENTATION_ERROR ("cfail") }

void object::sort_elements (const int)
{ IMPLEMENTATION_ERROR ("sort_elements") }

/************************************************************************/
/*									*/
/*			class Information				*/
/*									*/
/************************************************************************/

Information::Information()
: number (0)
, elements (0)

{
}



/************************************************************************/
/*									*/
/*			class Dictionary				*/
/*									*/
/************************************************************************/

Dictionary::Dictionary()
	: indices(NULL)
{
}


Dictionary::~Dictionary()

{
	empty_object();
}


void
Dictionary::empty_object()

{
	delete[] indices;
	delete[] elements;

	indices = NULL;
	elements = NULL;
	number = 0;
}

void
Dictionary::resize(const int newsize)
{
	String* newelements = new String[newsize];
	String* newindices = new String[newsize];

	int upto = (newsize<number)?newsize:number; // Doesn't know min(x,y)
	for(int i = 0; i < upto; i++)
	{
		newelements[i] = elements[i];
		newindices[i] = indices[i];
	}

	delete[] elements;
	delete[] indices;

	elements = newelements;
	indices = newindices;
	number = newsize;
}


/* It may be thought odd that we pass the index and the element number.
   The reason for doing this is that the element number has already
   been found out and it would be inefficient(Ian Sanity Prod. tm) to
   look for it again. */

void
Dictionary::set_element (
const	int	element,
const	String&	index,
const	String&	string)

{
	if(element)
	{
		elements[element-1] = string;
	}
	else
	{
		resize(number+1);// Resize the storage
		// NOTE: resize() changes number...
		elements[number-1] = string; // Add the new element at the end
		indices[number-1] = index;
	}
}


const String&
Dictionary::get_element(const int element)
const

{
	return(elements[element - 1]);
}


void
Dictionary::set_index (
const	int	index,
const	String& value)

{
	indices[index-1] = value;
}


const String&
Dictionary::get_index(const int element)
const

{
	return(indices[element - 1]);
}


const int
Dictionary::exist_element (
const	String& element)
const

{
	int temp;

	for(temp = 1; temp <= number; temp++)
	{
		if(semicolon_string_match (indices[temp - 1], element))
			return(temp);
	}

	return(0);
}


void
Dictionary::destroy_element (
const	int	element)

{
	for(int shifted_element = element; shifted_element < number; shifted_element++)
	{
		indices[shifted_element - 1] = indices[shifted_element];
		elements[shifted_element - 1] = elements[shifted_element];
	}

	resize(number - 1);
}


const dbref
Dictionary::get_next()
const

{
	dbref temp = get_real_next();

	while((temp != NOTHING) && (Typeof(temp) != TYPE_DICTIONARY))
		temp = db[temp].get_real_next();

	return(temp);
}

/************************************************************************/
/*									*/
/*			class Array_storage				*/
/*									*/
/************************************************************************/


void
Array_storage::sort_elements(int direction)
{
	/** Fix me **/
}


void
Array_storage::empty_object()

{
	delete[] elements;

	elements = 0;
	number = 0;
	size = 0;
}


void Array_storage::resize(
const int newsize)
{
	int upto = (newsize < number)?newsize:number;
	String* newelements = new String[newsize];

	for(int i = 0; i < upto; i++)
	{
		newelements[i] = elements[i];
	}

	delete[] elements;
	elements = newelements;
	number = newsize;
}

void
Array_storage::set_element (
const	int	element,
const	String& string)

{
	int this_size;

// Truncate to MAX_WIZARD_ARRAY_ELEMENTS entries. Silently.
// We should really bug this one, but given that our recent problem
// would have generated ~96000 BUG reports, we can give this a miss for
// now.
	if(element > (int)MAX_WIZARD_ARRAY_ELEMENTS) return;

	//NULL pointer is not a zero length string
	this_size = string.length();
	if(element > number)
	{
		resize(element);
	}
// size is a total counter of how big the array is so it doesn't go silly
	if (size)
		size = size - elements[element -1].length() + this_size;
	else
		set_size(this_size);


	elements[element - 1] = string;
}


const String&
Array_storage::get_element (
const	int	element)
const

{
	if (element <= number)
		return elements [element - 1];
	else
		return NULLSTRING;
}


void
Array_storage::destroy_element (
const	int	element)

{
	for (int copy_element = element; copy_element < number; copy_element++)
		elements [copy_element - 1] = elements[copy_element];

	resize(number -1);
}


const int
Array_storage::exist_element (
const	int	element)
const

{
	if ((element <= number) && (element > 0))
		return element;
	else
		return 0;
}


void
Array_storage::insert_element (
const	int	index,
const	String& element)
{
	resize(number+1);

	for(int count = number-1;count >= index;count--)
		elements[count] = elements[count - 1];

	elements[index-1]=element;

	size += element.length();
}



Array_storage::Array_storage ()
: size (0)

{
}


Array_storage::~Array_storage ()

{
	delete[] elements;
}

/************************************************************************/
/*									*/
/*			class Array					*/
/*									*/
/************************************************************************/

const dbref
Array::get_next ()
const

{
	dbref temp = get_real_next();

	while((temp != NOTHING) && (Typeof(temp) != TYPE_ARRAY))
		temp = db[temp].get_real_next();

	return(temp);
}


/************************************************************************/
/*									*/
/*			class Describable_object			*/
/*									*/
/************************************************************************/


Describable_object::~Describable_object ()

{
	set_description (NULLSTRING);
}


void
Describable_object::set_description (
const	String& new_text)
{
	char	*copy=NULL;
	char	*begin=NULL;
	char	*end=NULL;

	empty_object ();

	if (!new_text)
		return;

	set_size( new_text.length());
	copy = strdup (new_text.c_str());
	begin = copy;

	while (begin && *begin)
	{
		end = strchr (begin, '\n');
		if (end)
			*end = '\0';
		set_element (get_number_of_elements () + 1, begin);
		begin = end;
		if (begin)
			begin++;
	}
	free (copy);
}

static	char	buildbuf[BUFFER_LEN];
static  String buildstring;

const String&
Describable_object::get_description()
const
{
	int	x;
	char 	*p = buildbuf;
	*p = '\0';

	if (number==0)
		return NULLSTRING;
	for(x = 0;x < number;x++)
	{
		if(x!=0)
			strcat(p, "\n");
		if(elements[x])		/* NULL pointer not null string */
			strcat(p, elements[x].c_str());
		else
			if(x==(number-1))	/* for trailing blank lines */
				strcat(p, "\n");
	}
	buildstring = buildbuf;
	return buildstring;
}

/************************************************************************/
/*									*/
/*			class Property					*/
/*									*/
/************************************************************************/

const dbref
Property::get_next()
const

{
	dbref temp = get_real_next();

	while((temp != NOTHING) && (Typeof(temp) != TYPE_PROPERTY))
		temp = db[temp].get_real_next();

	return(temp);
}


/************************************************************************/
/*									*/
/*			class Lockable_object				*/
/*									*/
/************************************************************************/

Lockable_object::Lockable_object ()
	: key(TRUE_BOOLEXP), fail_message(), drop_message(),
		succ_message(), ofail(), osuccess(),
		odrop()
{
}


Lockable_object::~Lockable_object ()

{
	set_key (TRUE_BOOLEXP);
}


void
Lockable_object::set_key (
struct	boolexp	*k)

{
	if (key != TRUE_BOOLEXP)
		delete (key);
	key=k;
	if (key != TRUE_BOOLEXP)
		key->set_references ();
}


void
Lockable_object::set_fail_message (
const	String& str)

{
	fail_message = str;
}


void
Lockable_object::set_drop_message (
const	String& str)

{
	drop_message = str;
}


void
Lockable_object::set_succ_message (
const	String& str)

{
	succ_message = str;
}


void
Lockable_object::set_ofail (
const	String& str)

{
	ofail = str;
}


void
Lockable_object::set_osuccess (
const	String& str)

{
	osuccess = str;
}


void
Lockable_object::set_odrop (
const	String& str)

{
	odrop = str;
}


/************************************************************************/
/*									*/
/*			class Inheritable_object			*/
/*									*/
/************************************************************************/

void
Inheritable_object::set_parent (
const	dbref	p)

{
	parent = p;
	if (p != NOTHING)
		db [p].set_referenced ();
}

const String&
Inheritable_object::get_inherited_element(int element_to_get) const
{
	if(get_number_of_elements() || (parent==NOTHING))
		return get_element(element_to_get);
	else
		return db[parent].get_inherited_element(element_to_get);
}

const int Inheritable_object::exist_inherited_element(const int element_to_find) const
{
	if(get_number_of_elements() || (parent==NOTHING))
		return exist_element(element_to_find);
	else
		return db[parent].exist_inherited_element(element_to_find);
}

const unsigned int Inheritable_object::get_inherited_number_of_elements(void) const
{
	if(get_number_of_elements() || (parent==NOTHING))
		return get_number_of_elements();
	else
		return db[parent].get_inherited_number_of_elements();
}

/************************************************************************/
/*									*/
/*			class Old_object				*/
/*									*/
/************************************************************************/

Old_object::Old_object ()
	: commands(NOTHING), contents(NOTHING), contents_string(),
		exits(NOTHING), exits_string(), fuses(NOTHING),
		info_items(NOTHING), destination(NOTHING)
{
}


void
Old_object::add_command(
dbref	com)
{
std::list<String>::const_iterator nameiter;
std::map<String, dbref>::iterator insert;

	for(nameiter = db[com].get_namelist().begin(); nameiter != db[com].get_namelist().end(); nameiter++)
	{
		insert = commandmap.find(*nameiter);
		if(insert == commandmap.end())
		{
			commandmap[*nameiter] = com;
		}
		else if(insert->second != com)
		{
			insert->second = AMBIGUOUS;
		}
	}
}

void
Old_object::remove_command(
const	dbref	com)
{
std::map<String, dbref>::iterator remove;

// I'm not going to deal with AMBIGUOUS cases yet, as there shouldn't be too
// many of them.
	remove = commandmap.begin();
	while(remove != commandmap.end())
	{
		if(com == remove->second)
		{
			commandmap.erase(remove);
			// When you erase an entry from a map, you can't get an iterator back,
			// so we'll start all over again.
			remove = commandmap.begin();
		}
		else
		{
			remove++;
		}
	}
}

dbref
Old_object::find_command (
const	String& name) const
{
	std::map<String, dbref>::const_iterator iter;
	iter = get_commandmap()->find(name);

	if(iter != get_commandmap()->end())
	{
		return iter->second;
	}
	return NOTHING;
}

void
Old_object::set_destination (
const	dbref	o)

{
	destination = o;
	if (o != NOTHING && o != HOME)
		db [o].set_referenced ();
}


const dbref
Old_object::get_variables ()
const

{
	dbref temp = info_items;
	while((temp != NOTHING) && (Typeof(temp) != TYPE_VARIABLE))
		temp = db[temp].get_real_next();

	return(temp);
}


const dbref
Old_object::get_properties ()
const

{
	dbref temp = info_items;
	while((temp != NOTHING) && (Typeof(temp) != TYPE_PROPERTY))
		temp = db[temp].get_real_next();

	return(temp);
}


const dbref
Old_object::get_arrays ()
const

{
	dbref temp = info_items;
	while((temp != NOTHING) && (Typeof(temp) != TYPE_ARRAY))
		temp = db[temp].get_real_next();

	return(temp);
}


const dbref
Old_object::get_dictionaries ()
const

{
	dbref temp = info_items;
	while((temp != NOTHING) && (Typeof(temp) != TYPE_DICTIONARY))
		temp = db[temp].get_real_next();

	return(temp);
}

void
Old_object::set_contents_string (
const	String& c)
{
	contents_string = c;
}


/************************************************************************/
/*									*/
/*			class Room					*/
/*									*/
/************************************************************************/

Room::Room()
	: last_entry_time(0)
{
}

Room::~Room()
{
}


/************************************************************************/
/*									*/
/*			class Massy_object				*/
/*									*/
/************************************************************************/

Massy_object::Massy_object ()
	: gravity_factor(1.0), mass(0.0), volume(0.0), mass_limit(0.0),
		volume_limit(0.0)
{
}


/* Massy_object doesn't need a destructor. */


/************************************************************************/
/*									*/
/*			class puppet					*/
/*									*/
/************************************************************************/

puppet::puppet ()
	: pennies(0), score(0), last_name_change(time(NULL)),
		build_id(NOTHING), race(), who_string(), email_addr(),
		money(0), remote_location(get_real_location()), fsm_states(NULL)
{
}


puppet::~puppet ()
{
	set_race		(NULLSTRING);
	set_who_string		(NULLSTRING);
	set_email_addr		(NULLSTRING);
}


void
puppet::set_email_addr (
const	String& addr)
{
	email_addr = addr;
}


void
puppet::set_race (
const	String& r)

{
	race = r;
}


void
puppet::set_who_string (
const	String& s)

{
	who_string = s;
}


/************************************************************************/
/*									*/
/*			class Player					*/
/*									*/
/************************************************************************/

Player::Player ()
: recall(NULL)
, password()
, m_controller(NOTHING)
, colour(NULL)
, col_at(NULL)
, colour_play(NULL)
, colour_play_size(0)
, channel(NULL)
{
#ifdef ALIASES
	int	i;
	for (i = 0; i < MAX_ALIASES; i++)
		alias[i]	= NULLSTRING;

#endif
}


Player::~Player ()

{
	ditch_recall();
}


#ifdef ALIASES
void
Player::set_alias(
const	int	which,
const	String& what)

{
	alias[which] = what;
}
#endif

void
Player::add_recall_line (
const String& strung)
{
	if(!recall)
	{
		recall = new RecallBuffer;
	}
        //Quick thing of what this does and why.
        //As things are notified we place things into the output_lines_build
        //string and whenever we get a newline we put the whole thing into
        //the recall array. If we didn't group them you end up by each
        //line of the recall array having a few characters as the whole
        //output line is built up. When we go to output it if the output_lines_build
        //is not empty we output it at the end. This can mean that you
        //get 51 lines if you did @recall but it is 0430 you picky bastard.
        //Love Reaps.

	const char* string = strung.c_str();

	char* nlpos = 0;
	while((nlpos = strchr(string, '\n')) != 0)
	{
		if((nlpos == string) && (!*(recall->buffer_build)))
		{
			string++;
			continue; // Leave blank lines alone.
		}
		strncat(recall->buffer_build, string, (nlpos+1)-string);
		recall->buffer[recall->buffer_next++] = recall->buffer_build;
		recall->buffer_build[0] = '\0';
		if(recall->buffer_next == MAX_RECALL_LINES)
		{
			recall->buffer_next = 0;
			recall->buffer_wrapped = true;
		}
		string = nlpos+1;
	}
	if(*string)
	{
		strcat(recall->buffer_build, string);
	}
}

void
Player::output_recall (
const int	lines,
const context *	con)
{

	int thelines = lines;

        //Return if there is nothing in the buffer yet
        if(!recall || (recall->buffer_next == 0 && recall->buffer_wrapped == 0))
        {
                return;
        }

        //Check there is enough in the buffer already
        if(!recall->buffer_wrapped)
        {
                if(thelines > recall->buffer_next)
                {
                        thelines = recall->buffer_next;
                }
        }

        int line_to_output=recall->buffer_next - thelines;

        if(line_to_output < 0)
        {
                if(recall->buffer_wrapped)
                {
                        line_to_output = MAX_RECALL_LINES + line_to_output;
                }
                else
                {
                        line_to_output = 0;
                }
        }

        for(int i=0;i<thelines;i++)
        {
                if(line_to_output >= MAX_RECALL_LINES)
                {
                        line_to_output = 0;
                }

                notify_norecall(con->get_player(), "%s", recall->buffer[line_to_output].c_str());
                line_to_output++;
        }

        if(recall->buffer_build)
        {
                notify_norecall(con->get_player(), "%s", recall->buffer_build);
        }

}

void
Player::output_recall_conditional (
const String	match,
const int	lines,
const context *	con)
{

	int thelines = lines;

        //Return if there is nothing in the buffer yet
        if(!recall || (recall->buffer_next == 0 && recall->buffer_wrapped == 0))
        {
                return;
        }

        //Check there is enough in the buffer already
        if(!recall->buffer_wrapped)
        {
                if(thelines > recall->buffer_next)
                {
                        thelines = recall->buffer_next;
                }
        }

        int line_to_output=recall->buffer_next - thelines;

	RegularExpression re(match);

        if(line_to_output < 0)
        {
                if(recall->buffer_wrapped)
                {
                        line_to_output = MAX_RECALL_LINES + line_to_output;
                }
                else
                {
                        line_to_output = 0;
                }
        }

        for(int i=0;i<thelines;i++)
        {
                if(line_to_output >= MAX_RECALL_LINES)
                {
                        line_to_output = 0;
                }

		const String line = recall->buffer[line_to_output];
		if(re.Match(line))
		{
			notify_norecall(con->get_player(), "%s", line.c_str());
		}
                line_to_output++;
        }

        if(recall->buffer_build)
        {
		if(re.Match(recall->buffer_build))
			notify_norecall(con->get_player(), "%s", recall->buffer_build);
        }

}

void
Player::ditch_recall()
{
/* Get rid of the recall buffer, because it wastes memory. */
	delete recall;
	recall = 0;
}

void
Player::set_colour (
const	char	*new_colour)

{
	ASSIGN_STRING(colour, new_colour);
}


void
Player::set_controller (
const	dbref	o)

{
	m_controller = o;
	if (o != NOTHING)
		db [o].set_referenced ();
}


void
puppet::set_build_id(
const	dbref	o)

{
	build_id = o;
	if (o != NOTHING)
		db [o].set_referenced ();
}


void
puppet::reset_build_id(
const	dbref	o)

{
	build_id = o;
}


void
Player::set_password (
const	String& p)
{
	password = p;
}


#ifdef ALIASES
const int
Player::look_at_aliases (
const	String& string)
const

{
	int	i;
	int	foundempty = 0;

	/*
	 * The return values from this function are NON OBVIOUS!
	 * Beware. It is private to class player, so this shouldn't
	 * be a problem. In the description below, 'i' is the array
	 * reference of the position in the array concerned.
	 * Values - (i + 1) if 'string' is already an alias for this
	 *                  player.
	 *          -(i+ 1) if 'string' is not an alias for this
	 *                  player, we return this to show where to
	 *                  put the string.
	 *             0    'string' is not an alias for this player,
	 *                   and there is no room to put it in.
	 */

	for (i = 0;; i++)
	{
		if (i == MAX_ALIASES)
			break;
		if ((!alias[i]) && (foundempty == 0))
			foundempty = (i + 1);
		if ((string_compare(alias[i], string)) == 0)
			return (i + 1);
	}
	return (-foundempty);
}


const int
Player::remove_alias (
const	String& string)

{
	int	test;

	if ((test = look_at_aliases(string)) > 0)
	{
		set_alias((test - 1), NULLSTRING);
		return 1;
	}
	return 0;
}


const int
Player::add_alias (
const	String& string)

{
	int	test;

	test = look_at_aliases(string);
	if (test < 0)
	{
		set_alias(-(test + 1), string);
		return 1;
	}
	if (test > 0)
		return -1;
	return 0;
}


const int
Player::has_alias(
const	String& string)
const

{
	if (look_at_aliases(string) > 0)
		return 1;
	return 0;
}


const int
Player::has_partial_alias(
const	String& string)
const

{
	int	i;

	for (i = 0; i < MAX_ALIASES; i++)
	{
		if (string_prefix(alias[i].c_str(), string.c_str()))
			return 1;
	}
	return 0;
}
#endif /* ALIASES */


const double
Player::get_inherited_mass ()
const

{
	if (get_mass () != NUM_INHERIT)
		return (get_mass ());
	else if (get_parent () == NOTHING)
		return (STANDARD_PLAYER_MASS);
	else
		return (db [get_parent ()].get_inherited_mass ());
}

const double
Player::get_inherited_volume ()
const

{
	if (get_volume () != NUM_INHERIT)
		return (get_volume ());
	else if (get_parent () == NOTHING)
		return (STANDARD_PLAYER_VOLUME);
	else
		return (db [get_parent ()].get_inherited_volume ());
}

const double
Player::get_inherited_gravity_factor ()
const

{
	if (get_gravity_factor () != NUM_INHERIT)
		return (get_gravity_factor ());
	else if (get_parent () == NOTHING)
		return (STANDARD_PLAYER_GRAVITY);
	else
		return (db [get_parent ()].get_inherited_gravity_factor ());
}

const double
Player::get_inherited_mass_limit ()
const

{
	if (get_mass_limit () != NUM_INHERIT)
		return (get_mass_limit ());
	else if (get_parent () == NOTHING)
		return (STANDARD_PLAYER_MASS_LIMIT);
	else
		return (db [get_parent ()].get_inherited_mass_limit ());
}

const double
Player::get_inherited_volume_limit ()
const

{
	if (get_volume_limit () != NUM_INHERIT)
		return (get_volume_limit ());
	else if (get_parent () == NOTHING)
		return (STANDARD_PLAYER_VOLUME_LIMIT);
	else
		return (db [get_parent ()].get_inherited_volume_limit ());
}


/************************************************************************/
/*									*/
/*			class Variable					*/
/*									*/
/************************************************************************/

const dbref
Variable::get_next()
const

{
	dbref temp = get_real_next();

	while((temp != NOTHING) && (Typeof(temp) != TYPE_VARIABLE))
		temp = db[temp].get_real_next();

	return(temp);
}


/************************************************************************/
/*									*/
/*			class Thing					*/
/*									*/
/************************************************************************/

Thing::Thing ()
	: lock_key(TRUE_BOOLEXP)
{
}


Thing::~Thing ()

{
	set_lock_key		(TRUE_BOOLEXP);
}


void
Thing::set_lock_key (
struct	boolexp	*k)
{
	if (lock_key != TRUE_BOOLEXP)
		delete (lock_key);
	lock_key=k;
	if (lock_key != TRUE_BOOLEXP)
		lock_key->set_references ();
}


const bool
Thing::destroy (
const	dbref	item)

{
	dbref	temp;

	destroy_all_commands (item);
	destroy_all_exits (item);
	destroy_all_fuses (item);
	destroy_all_variables (item);

	/* Unlink the thing */
	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db[temp].set_contents(remove_first (db[temp].get_contents(), item));
	db [db [item].get_owner ()].add_pennies (THING_COST);
	return (db.delete_object (item));
}


const double
Thing::get_inherited_mass ()
const

{
	if (get_mass () != NUM_INHERIT)
		return (get_mass ());
	else if (get_parent () == NOTHING)
		return (STANDARD_THING_MASS);
	else
		return (db [get_parent ()].get_inherited_mass ());
}


const double
Thing::get_inherited_volume ()
const

{
	if (get_volume () != NUM_INHERIT)
		return (get_volume ());
	else if (get_parent () == NOTHING)
		return (STANDARD_THING_VOLUME);
	else
		return (db [get_parent ()].get_inherited_volume ());
}


const double
Thing::get_inherited_gravity_factor ()
const

{
	if (get_gravity_factor () != NUM_INHERIT)
		return (get_gravity_factor ());
	else if (get_parent () == NOTHING)
		return (STANDARD_THING_GRAVITY);
	else
		return (db [get_parent ()].get_inherited_gravity_factor ());
}


const double
Thing::get_inherited_mass_limit ()
const

{
	if (get_mass_limit () != NUM_INHERIT)
		return (get_mass_limit ());
	else if (get_parent () == NOTHING)
		return (STANDARD_THING_MASS_LIMIT);
	else
		return (db [get_parent ()].get_inherited_mass_limit ());
}


const double
Thing::get_inherited_volume_limit ()
const

{
	if (get_volume_limit () != NUM_INHERIT)
		return (get_volume_limit ());
	else if (get_parent () == NOTHING)
		return (STANDARD_THING_VOLUME_LIMIT);
	else
		return (db [get_parent ()].get_inherited_volume_limit ());
}


void
destroy_all_commands (
const	dbref	zap_thing)

{
	dbref	temp;

	while ((temp = db[zap_thing].get_commands ()) != NOTHING)
		db [temp].destroy (temp);
}


void
destroy_all_exits (
const	dbref	zap_thing)

{
	dbref	temp;

	while ((temp = db[zap_thing].get_exits ()) != NOTHING)
		db [temp].destroy (temp);
}


void
destroy_all_fuses (
const	dbref	zap_thing)

{
	dbref	temp;

	while ((temp = db[zap_thing].get_fuses ()) != NOTHING)
		db [temp].destroy (temp);
}


void
destroy_all_variables (
const	dbref	zap_thing)

{
	dbref	temp;

	while ((temp = db[zap_thing].get_info_items ()) != NOTHING)
		db [temp].destroy (temp);
}


/************************************************************************/
/*									*/
/*			class Fuse					*/
/*									*/
/************************************************************************/

void
Fuse::set_cfail(
const	dbref	command)

{
	if((command != HOME) && (command != NOTHING))
		db[command].set_referenced();
	set_exits(command);
}


void
Fuse::set_csucc(
const	dbref	command)

{
	if((command != HOME) && (command != NOTHING))
		db[command].set_referenced();
	set_contents(command);
}

const bool
Fuse::destroy (
const	dbref	item)

{
	dbref	temp;

	destroy_all_variables(item);
	/* If I'm Sticky and in use, rely on the sticky fuse executor to notice that I've gone away */

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_fuses(remove_first (db [temp].get_fuses(), item));
	db [db [item].get_owner ()].add_pennies (FUSE_COST);

	return (db.delete_object (item));
}


/************************************************************************/
/*									*/
/*			class Alarm					*/
/*									*/
/************************************************************************/

const bool
Alarm::destroy (
const	dbref	item)

{
	dbref	temp;

	db.unpend (item);
	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_fuses(remove_first (db [temp].get_fuses(), item));
	db [db [item].get_owner ()].add_pennies (ALARM_COST);

	return (db.delete_object (item));
}


const bool
Room::destroy (
const	dbref	item)

{
	dbref	temp;

	destroy_all_commands (item);
	destroy_all_exits (item);
	destroy_all_fuses (item);
	destroy_all_variables (item);

	if (((temp = db[item].get_location()) != NOTHING) && (Typeof (temp) != TYPE_FREE))
		db[temp].set_contents(remove_first (db[temp].get_contents(), item));

	db [db [item].get_owner ()].add_pennies (ROOM_COST);

	return (db.delete_object (item));
}


const double
Room::get_inherited_mass ()
const

{
	if (get_mass () != NUM_INHERIT)
		return (get_mass ());
	else if (get_parent () == NOTHING)
		return (STANDARD_ROOM_MASS);
	else
		return (db [get_parent ()].get_inherited_mass ());
}


const double
Room::get_inherited_volume ()
const

{
	if (get_volume () != NUM_INHERIT)
		return (get_volume ());
	else if (get_parent () == NOTHING)
		return (STANDARD_ROOM_VOLUME);
	else
		return (db [get_parent ()].get_inherited_volume ());
}


const double
Room::get_inherited_gravity_factor ()
const

{
	if (get_gravity_factor () != NUM_INHERIT)
		return (get_gravity_factor ());
	else if (get_parent () == NOTHING)
		return (STANDARD_ROOM_GRAVITY);
	else
		return (db [get_parent ()].get_inherited_gravity_factor ());
}


const double
Room::get_inherited_mass_limit ()
const

{
	if (get_mass_limit () != NUM_INHERIT)
		return (get_mass_limit ());
	else if (get_parent () == NOTHING)
		return (STANDARD_ROOM_MASS_LIMIT);
	else
		return (db [get_parent ()].get_inherited_mass_limit ());
}


const double
Room::get_inherited_volume_limit ()
const

{
	if (get_volume_limit () != NUM_INHERIT)
		return (get_volume_limit ());
	else if (get_parent () == NOTHING)
		return (STANDARD_ROOM_VOLUME_LIMIT);
	else
		return (db [get_parent ()].get_inherited_volume_limit ());
}

const bool
Variable::destroy (
const	dbref	item)

{
	dbref	temp;

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_info_items(remove_first (db [temp].get_info_items(), item));
	db [db [item].get_owner ()].add_pennies (VARIABLE_COST);

	return (db.delete_object (item));
}


const bool
Property::destroy (
const	dbref	item)

{
	dbref	temp;

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_info_items(remove_first (db [temp].get_info_items(), item));
	db [db [item].get_owner ()].add_pennies (PROPERTY_COST);

	return (db.delete_object (item));
}


const bool
Array_storage::destroy (
const	dbref	item)

{
	dbref	temp;

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_info_items(remove_first (db [temp].get_info_items(), item));
	db [db [item].get_owner ()].add_pennies (ARRAY_COST);

	return (db.delete_object (item));
}


const bool
Dictionary::destroy (
const	dbref	item)

{
	dbref	temp;

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_info_items(remove_first (db [temp].get_info_items(), item));
	db [db [item].get_owner ()].add_pennies (DICTIONARY_COST);

	return (db.delete_object (item));
}


const bool
Exit::destroy (
const	dbref	item)

{
	dbref	temp;

	destroy_all_commands (item);
	destroy_all_fuses (item);
	destroy_all_variables (item);

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db [temp].set_exits(remove_first (db [temp].get_exits(), item));
	if(db[item].get_owner() != NOTHING)
		db [db [item].get_owner ()].add_pennies (EXIT_COST);

	return (db.delete_object (item));
}


const bool
puppet::destroy (
const	dbref	item)

{
	dbref	temp;

	destroy_all_commands (item);
	destroy_all_exits (item);
	destroy_all_fuses (item);
	destroy_all_variables (item);

	if(db[item].get_owner() != NOTHING)
	{
		db [db [item].get_owner ()].add_pennies (PUPPET_COST);
	}
	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db[temp].set_contents(remove_first (db[temp].get_contents(), item));

	return db.delete_object (item);
}


Command::~Command ()

{
	if((get_location() != NOTHING) && (Typeof(get_location()) != TYPE_FREE))
	{
		db[get_location()].remove_command(get_id());
	}
	/* Ditch the parse-helper array if it's ever been allocated */
	if (parse_helper)
		delete [] parse_helper;
}

/* Return number of lines in a block (elements added together due to \'s on
 * the end). */
unsigned int Command::lines_in_command_block(const unsigned start_line) const
{
	bool backslash_primed=true; 	// Get first line
	unsigned current_line=start_line;

	while((current_line<=unsigned(get_number_of_elements())) && (backslash_primed))
	{
		backslash_primed=false;
		const char *const line_start=get_element(current_line).c_str();
		if(line_start)
		{
			const char *sliding_ptr=line_start+strlen(line_start);
			while((sliding_ptr>line_start) && (*(--sliding_ptr)=='\\'))
				backslash_primed=(backslash_primed==true)?false:true;
		}
		current_line++;
	}
	return current_line-start_line;
}

unsigned int Command::inherited_lines_in_cmd_blk(const unsigned start_line) const
{
	if(get_number_of_elements() || (get_parent()==NOTHING))	
		return lines_in_command_block(start_line);
	else
		return db[get_parent()].inherited_lines_in_cmd_blk(start_line);
}
	

/* Make a command line by appending elements with the right number of \
 * characters at the end */
unsigned Command::reconstruct_command_block(char *const command_block, const unsigned max_size, const unsigned start_line) const
{
	static const char too_big[]="TooBig";
	unsigned space_left=max_size;
	bool it_dont_fit=false;
	unsigned	current_line=start_line;
	bool first=true;

	*command_block='\0';
	space_left--;
	for(;current_line<(start_line+lines_in_command_block(start_line));current_line++)
	{
		if(first==false)
		{
			if(space_left)
			{
				strcat(command_block,"\n");
				space_left--;
			}
			else
				it_dont_fit=true;
		}
		else
			first=false;
		if(get_element(current_line))
		{
			if(get_element(current_line).length()<=space_left)
			{
				strcat(command_block,get_element(current_line).c_str());
				space_left-=get_element(current_line).length();
			}
			else
			{
				it_dont_fit=true;
				log_bug("command_block overflow, Start_line:  %u, max_size:  %u", start_line, max_size);
			}
		}
		if(it_dont_fit)		//Catch overflow.
		{
			if(space_left>=(sizeof(too_big)-1))
				strcat(command_block,too_big);
			else 
				if(max_size>sizeof(too_big))
					*(command_block+max_size-sizeof(too_big)-1)='\0';
			//return anyway
			return lines_in_command_block(start_line);
		}
	}
	return lines_in_command_block(start_line);
}

unsigned int Command::reconstruct_inherited_command_block(char *const command_block, const unsigned max_length, const unsigned start_line) const
{
	if(get_number_of_elements() || (get_parent()==NOTHING))	
		return reconstruct_command_block(command_block, max_length, start_line);
	else
		return db[get_parent()].reconstruct_inherited_command_block(command_block, max_length, start_line);
}

void
Command::set_cfail(
const	dbref	command)
{
	if((command != HOME) && (command != NOTHING))
		db[command].set_referenced();
	set_exits(command);
}

void
Command::set_csucc(
const	dbref	command)
{
	if((command != HOME) && (command != NOTHING))
		db[command].set_referenced();
	set_contents(command);
}


const bool
Command::destroy (
const	dbref	item)

{
	dbref	temp;

	destroy_all_variables(item);

	if (Typeof (temp = db[item].get_location()) != TYPE_FREE)
		db[temp].set_commands(remove_first(db[temp].get_commands(), item));
	db [db [item].get_owner ()].add_pennies (COMMAND_COST);

	return db.delete_object (item);
}

void
Command::set_name(const String& str)
{
	dbref loc = get_location();

	if(loc != NOTHING)
	{
		if(Typeof(loc) != TYPE_FREE)
		{
			db[loc].remove_command(get_id());
		}
		else
		{
			loc = NOTHING;
		}
	}
	object::set_name(str);
	if(loc != NOTHING)
	{
		db[loc].add_command(get_id());
	}
}

void
Command::set_location(const dbref target)
{
	dbref loc = get_location();
// Need to check for TYPE_FREE objects when we're loading the database
// incase they haven't been loaded yet.
	if((loc != NOTHING) && (Typeof(loc) != TYPE_FREE))
	{
			db[get_location()].remove_command(get_id());
	}
	object::set_location(target);
	if((target != NOTHING) && (Typeof(target) != TYPE_FREE))
	{
		db[target].add_command(get_id());
	}
}

/*
 * Allocate the parse_helper array.
 *
 * Return values:
 *	true if the array was newly allocated,
 *	false if the array was already allocated.
 */

const bool
Command::alloc_parse_helper ()

{
	if (parse_helper)
		return false;

	if (!(parse_helper = new unsigned short [get_inherited_number_of_elements () + 1]))
		panic ("Couldn't allocate parse helper array");

	return true;
}


void
Command::flush_parse_helper ()

{
	if (parse_helper)
	{
		delete [] parse_helper;
		parse_helper = 0;
	}
}


const unsigned short
Command::get_parse_helper (
const	unsigned int	index)
const

{
	if (!parse_helper || index <= 0 || index > get_inherited_number_of_elements ())
		return 0;
	else
		return parse_helper [index];
}


void
Command::set_parse_helper (
const	unsigned int	index,
const	unsigned short	value)

{
	if (!parse_helper)
		alloc_parse_helper ();
#ifdef	DEBUG
	log_debug("set_parse_helper: line %d = 0x%4x (%d)", index, value, value);
#endif	/* DEBUG */
	if (index > 0 && index <= get_inherited_number_of_elements ())
		parse_helper [index] = value;
}


/*
 * Setting a description messes up any existing parse, so nuke it.
 */

void
Command::set_description (
const	String& desc)

{
	if (parse_helper)
	{
		delete [] parse_helper;
		parse_helper = 0;
	}

	Describable_object::set_description (desc);
}

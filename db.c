/* static char SCCSid[] = "@(#)db.c	1.46\t7/27/95"; */
#include "copyright.h"

/* Modified database format finished 1/3/96. See db.h for details */

#pragma implementation "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "db.h"
#include "objects.h"
#include "interface.h"
#include "config.h"
#include "externs.h"

#define DB_MSGLEN	1024

#define FIELD_SEPARATOR '\027'
#define	OBJECT_SEPARATOR '\002'
#define LOAD_BUFFER_SIZE (8*BUFFER_LEN)


#ifdef DEBUG
#define FREE(x) { if((x)==NULL) \
			Trace( "WARNING:  attempt to free NULL pointer (%s, line %d)\n", __FILE__, __LINE__); \
		  else \
		  { \
		  	free((x)); \
		  	(x)=NULL; \
		  } \
	  	}
#else
#define FREE(x) free((x))
#endif /* DEBUG */


static	char			get_next_char	(FILE *f);
static	char			*get_next_field	(FILE *f);
static	void			initialise_load_buffer (FILE *f);
static	void			finalise_load_buffer (void);
static	void			reload_buffer	(FILE *f);
static	void			putboolexp	(FILE *, const boolexp *);
static	const	char		*getstring	(FILE *);
static	void			putfieldtype	(FILE *, const int);
static	const	int		getfieldtype	(FILE *);
static	const	int		getint		(FILE *);
static	const	double		getdouble	(FILE *);
static	boolexp			*getboolexp	(FILE *, const int version, const int iformat);
static	const	dbref		getref		(FILE *);


static void
get_array_elements(
FILE	*f,
object	*it)
{
	int i;
	int num_elements;
	int use_elements;

	num_elements = getint(f);

// In case of the array sizes shrinking, or a badly saved array, truncate
// truncate arrays to their maximum lengths
	use_elements = ((it->get_flag(FLAG_WIZARD)) ? MIN(num_elements, MAX_WIZARD_ARRAY_ELEMENTS) : MIN(num_elements, MAX_MORTAL_ARRAY_ELEMENTS));

	for(i = 1; i <= use_elements; i++)
		it->set_element(i, getstring(f));
// In order to make the db load sane, junk additional elements.
	if(use_elements < num_elements)
	{
		Trace( "BUG: Array too large on database load, truncated from %d to %d elements\n", num_elements, use_elements);
		for(; i <= num_elements; i++)
			getstring(f);
	}
}

/* Used for backward compatibility to convert from the flag
 *	int used in db version 16 to the unsigned char array used with
 *	db version 17+
 */

static void
convert_flags (
const	dbref			i,
const	object_flag_type	oldflags)

{
	if(oldflags & OLD_APPRENTICE)
		db[i].set_flag(FLAG_APPRENTICE);
	if(oldflags & OLD_ABORT)
		db[i].set_flag(FLAG_ABORT);
	if(oldflags & OLD_WIZARD)
		db[i].set_flag(FLAG_WIZARD);
	if(oldflags & OLD_DARK)
		db[i].set_flag(FLAG_DARK);
	if(oldflags & OLD_LISTEN)
		db[i].set_flag(FLAG_LISTEN);
	if(oldflags & OLD_STICKY)
		db[i].set_flag(FLAG_STICKY);
	if(oldflags & OLD_BUILDER)
		db[i].set_flag(FLAG_BUILDER);
	if(oldflags & OLD_VISIBLE)
		db[i].set_flag(FLAG_VISIBLE);
	if(oldflags & OLD_OPENABLE)
		db[i].set_flag(FLAG_OPENABLE);
	if(oldflags & OLD_OPEN)
		db[i].set_flag(FLAG_OPEN);
	if(oldflags & OLD_OPAQUE)
		db[i].set_flag(FLAG_OPAQUE);
	if(oldflags & OLD_LOCKED)
		db[i].set_flag(FLAG_LOCKED);
	if(oldflags & OLD_HAVEN)
		db[i].set_flag(FLAG_HAVEN);
	if(oldflags & OLD_CHOWN_OK)
		db[i].set_flag(FLAG_CHOWN_OK);
	if(oldflags & OLD_APPRENTICE)
		db[i].set_flag(FLAG_APPRENTICE);
	if(oldflags & OLD_TOM)
		db[i].set_flag(FLAG_TOM);
	if(oldflags & OLD_NUMBER)
		db[i].set_flag(FLAG_NUMBER);
	if(oldflags & OLD_CONNECTED)
		db[i].set_flag(FLAG_CONNECTED);
	if(oldflags & OLD_TRACING)
		db[i].set_flag(FLAG_TRACING);
	if(oldflags & OLD_FIGHTING)
		db[i].set_flag(FLAG_FIGHTING);
	if(oldflags & OLD_ASHCAN)
		db[i].set_flag(FLAG_ASHCAN);
	if(oldflags & OLD_INHERITABLE)
		db[i].set_flag(FLAG_INHERITABLE);
	if(oldflags & OLD_REFERENCED)
		db[i].set_flag(FLAG_REFERENCED);

	switch(Typeof(i))
	{
		case TYPE_PLAYER:
		case TYPE_PUPPET:
			switch ((oldflags & OLD_GENDER_MASK) >> OLD_GENDER_SHIFT)
			{
				case OLD_GENDER_MALE:
					db[i].set_flag(FLAG_MALE);
					break;
				case OLD_GENDER_FEMALE:
					db[i].set_flag(FLAG_FEMALE);
					break;
				case OLD_GENDER_NEUTER:
					db[i].set_flag(FLAG_NEUTER);
					break;
				default:
					/* Unassigned, leave it clear */
					break;
			}
			/*FALLTHROUGH*/
		case TYPE_THING:
		case TYPE_EXIT:
		case TYPE_ROOM:

			switch (((oldflags & OLD_LOW_ARTICLE) >> OLD_LOW_ARTICLE_SHIFT) | ((oldflags & OLD_HIGH_ARTICLE) >> OLD_HIGH_ARTICLE_SHIFT))
			{
				case OLD_ARTICLE_NOUN:
					db[i].set_flag(FLAG_ARTICLE_NOUN);
					break;
				case OLD_ARTICLE_PLURAL:
					db[i].set_flag(FLAG_ARTICLE_PLURAL);
					break;
				case OLD_ARTICLE_SINGULAR_VOWEL:
					db[i].set_flag(FLAG_ARTICLE_SINGULAR_VOWEL);
					break;
				case OLD_ARTICLE_SINGULAR_CONS_FLAG:
					db[i].set_flag(FLAG_ARTICLE_SINGULAR_CONS);
					break;
				default:
					break;
			}
			break;
		default:
			break;

	}
}


static int
convert_flag (
const	int	oldflag)
{
	int	tmp;

	if(oldflag == OLD_APPRENTICE)
		return(FLAG_APPRENTICE);
	if(oldflag == OLD_ABORT)
		return(FLAG_ABORT);
	if(oldflag == OLD_WIZARD)
		return(FLAG_WIZARD);
	if(oldflag == OLD_DARK)
		return(FLAG_DARK);
	if(oldflag == OLD_LISTEN)
		return(FLAG_LISTEN);
	if(oldflag == OLD_STICKY)
		return(FLAG_STICKY);
	if(oldflag == OLD_BUILDER)
		return(FLAG_BUILDER);
	if(oldflag == OLD_VISIBLE)
		return(FLAG_VISIBLE);
	if(oldflag == OLD_OPENABLE)
		return(FLAG_OPENABLE);
	if(oldflag == OLD_OPEN)
		return(FLAG_OPEN);
	if(oldflag == OLD_OPAQUE)
		return(FLAG_OPAQUE);
	if(oldflag == OLD_LOCKED)
		return(FLAG_LOCKED);
	if(oldflag == OLD_HAVEN)
		return(FLAG_HAVEN);
	if(oldflag == OLD_CHOWN_OK)
		return(FLAG_CHOWN_OK);
	if(oldflag == OLD_APPRENTICE)
		return(FLAG_APPRENTICE);
	if(oldflag == OLD_TOM)
		return(FLAG_TOM);
	if(oldflag == OLD_NUMBER)
		return(FLAG_NUMBER);
	if(oldflag == OLD_CONNECTED)
		return(FLAG_CONNECTED);
	if(oldflag == OLD_TRACING)
		return(FLAG_TRACING);
	if(oldflag == OLD_FIGHTING)
		return(FLAG_FIGHTING);
	if(oldflag == OLD_ASHCAN)
		return(FLAG_ASHCAN);
	if(oldflag == OLD_INHERITABLE)
		return(FLAG_INHERITABLE);
	if(oldflag == OLD_REFERENCED)
		return(FLAG_REFERENCED);

	tmp = (oldflag & OLD_GENDER_MASK) >> OLD_GENDER_SHIFT;
	switch(tmp)
	{
		case OLD_GENDER_MALE:
			return(FLAG_MALE);
			break;
		case OLD_GENDER_FEMALE:
			return(FLAG_FEMALE);
			break;
		case OLD_GENDER_NEUTER:
			return(FLAG_NEUTER);
			break;
		default:
			/* Unassigned, leave it clear */
			break;
	}

	tmp = (((oldflag & OLD_LOW_ARTICLE) >> OLD_LOW_ARTICLE_SHIFT) | ((oldflag & OLD_HIGH_ARTICLE) >> OLD_HIGH_ARTICLE_SHIFT));
	switch(tmp)
	{
		case OLD_ARTICLE_NOUN:
			return(FLAG_ARTICLE_NOUN);
			break;
		case OLD_ARTICLE_PLURAL:
			return(FLAG_ARTICLE_PLURAL);
			break;
		case OLD_ARTICLE_SINGULAR_VOWEL:
			return(FLAG_ARTICLE_SINGULAR_VOWEL);
			break;
		case OLD_ARTICLE_SINGULAR_CONS_FLAG:
			return(FLAG_ARTICLE_SINGULAR_CONS);
			break;
		default:
			break;
	}
	Trace( "Unknown flag in lock, converted to 0");
	return(0);
}


static void
putref (
	FILE	*f,
const	dbref	ref)

{
	fprintf(f, "%d%c", ref, FIELD_SEPARATOR);
}


static void
putint (
	FILE	*f,
const	int	entry)

{
	fprintf(f, "%d%c", entry, FIELD_SEPARATOR);
}


static void
puthex (
	FILE		*f,
const	unsigned char	v)

{
	fprintf(f, "%02x", v);
}


static void
putdouble (
	FILE	*f,
const	double	entry)

{
	if(entry == HUGE_VAL)
		fprintf(f, "%s%c", "INF", FIELD_SEPARATOR);
	else
		fprintf(f, "%.40g%c", entry, FIELD_SEPARATOR);
}


static void
putstring (
	FILE	*f,
const	char	*s)

{
	if (s != NULL)
		fputs(s, f);
	putc(FIELD_SEPARATOR, f);

}


void
putbool_subexp(
	FILE	*f,
const	boolexp	*b,
	char	**buf_ptr)

{
	static	char	temp_buf[10];

	switch(b->type)
	{
		case BOOLEXP_AND:
			*((*buf_ptr)++) = '(';
			putbool_subexp(f, b->sub1, buf_ptr);
			*((*buf_ptr)++) = AND_TOKEN;
			putbool_subexp(f, b->sub2, buf_ptr);
			*((*buf_ptr)++) = ')';
			break;
		case BOOLEXP_OR:
			*((*buf_ptr)++) = '(';
			putbool_subexp(f, b->sub1, buf_ptr);
			*((*buf_ptr)++) = OR_TOKEN;
			putbool_subexp(f, b->sub2, buf_ptr);
			*((*buf_ptr)++) = ')';
			break;
		case BOOLEXP_NOT:
			*((*buf_ptr)++) = '(';
			*((*buf_ptr)++) = NOT_TOKEN;
			putbool_subexp(f, b->sub1, buf_ptr);
			*((*buf_ptr)++) = ')';
			break;
		case BOOLEXP_CONST:
			sprintf(temp_buf, "%d", b->thing);
			strcpy(*buf_ptr, temp_buf);
			*buf_ptr += strlen(temp_buf)*sizeof(char);
			break;
		case BOOLEXP_FLAG:
			*((*buf_ptr)++) = '(';
			*((*buf_ptr)++) = COMMAND_TOKEN;
			sprintf(temp_buf, "%d", b->thing);
			strcpy(*buf_ptr, temp_buf);
			*buf_ptr += strlen(temp_buf)*sizeof(char);
			*((*buf_ptr)++) = ')';
			break;
		default:
			break;
	}
}

void
putboolexp(
	FILE	*f,
const	boolexp	*b)

{
	char	buf [DB_MSGLEN*2];
	char	*buf_ptr = buf;

	if(b != TRUE_BOOLEXP)
	{
		putbool_subexp(f, b, &buf_ptr);
		*buf_ptr = '\0';
		putstring(f, buf);
	}
	else
		putc(FIELD_SEPARATOR, f);
}


const Boolean
object::write (
FILE	*f)
const

{
	putfieldtype(f, OBJECT_FLAGS);

	/* These three lines output the flags */
	for(int count=0;count < FLAGS_WIDTH;count++)
		puthex(f, get_flag_byte(count));
	putc (FIELD_SEPARATOR, f);

	putfieldtype	(f, OBJECT_NAME);
	putstring	(f, name);
	if(location != NOTHING)
	{
		putfieldtype	(f, OBJECT_LOCATION);
		putref		(f, location);
	}
	if(location != NOTHING)
	{
		putfieldtype	(f, OBJECT_NEXT);
		putref		(f, next);
	}
	putfieldtype	(f, OBJECT_OWNER);
	putref		(f, owner);
	return (True);
}


const Boolean
object::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

	if(iformat == 0)
	{
		if(version >16)
		{
			field = get_next_field(f);

			for (int count=0; count < FLAGS_WIDTH; count++)
			{
				sscanf (&field[count*2], "%02x", &byte);
				set_flag_byte (count, byte);
			}
			/* This should really be the field separator */
		}
		set_name (getstring (f));
		set_location (getref (f));
		set_next (getref (f));

		/* Owner set using direct assignment to avoid set_referenced() on an object that isn't read yet */
		owner = getref (f);
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
		{
			switch(fieldtype)
			{
				case OBJECT_FLAGS:
	                        	field = get_next_field(f);

					for(int count=0;count < FLAGS_WIDTH;count++)
                        		{
                       	         		sscanf(&field[count*2], "%02x", &byte);
                                		set_flag_byte(count, byte);
                        		}
					break;
				case OBJECT_NAME:
					set_name(getstring (f));
					break;
				case OBJECT_LOCATION:
					set_location (getref (f));
					break;
				case OBJECT_NEXT:
					set_next (getref (f));
					break;
				case OBJECT_OWNER:
					owner = getref(f);
					break;
				default:
					Trace( "Something has gone seriously wrong in object::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
Dictionary::write (
FILE	*f)
const

{
	int i;

	object::write (f);
	putfieldtype(f, DICTIONARY_ELEMENTS);
	putint(f, number);
	for(i=1; i <= number; i++)
	{
		putstring(f, indices[i - 1]);
		putstring(f, elements[i - 1]);
	}
	return (True);
}

const Boolean
Dictionary::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int		i;
	int		fieldtype;
	char		*temp;
	char		*field;
	unsigned int	byte;

	if(iformat == 0)
	{
		object::read (f, version, iformat);
		for(i=getint(f); i; i--)
		{
			temp = strdup(getstring(f));
			set_element(0, temp, getstring(f));
			FREE(temp);
		}
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
                {
                        switch(fieldtype)
                        {
                                case DICTIONARY_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case DICTIONARY_NAME:
                                        set_name(getstring (f));
                                        break;
                                case DICTIONARY_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case DICTIONARY_NEXT:
                                        set_next (getref (f));
                                        break;
				case DICTIONARY_OWNER:
                                        set_owner_no_check (getref(f));
					break;
				case DICTIONARY_ELEMENTS:
					for(i=getint(f); i; i--)
				        {
				                temp = strdup(getstring(f));
				                set_element(0, temp, getstring(f));
                				FREE(temp);
        				}
					break;
				default:
					Trace( "Something has gone seriously wrong in Dictionary::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);

}


const Boolean
Array_storage::write(
FILE	*f)
const
{
	int i;

	object::write(f);
	putfieldtype(f, ARRAY_STORAGE_ELEMENTS);
	putint(f, number);
	for(i=1; i <= number; i++)
		putstring(f, elements[i - 1]);
	return (True);
}


const Boolean
Array::write (
FILE	*f)
const

{
	Array_storage::write (f);
	return (True);
}


const Boolean
Array::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int		fieldtype;
	int		i;
	int		num_elements;
	char		*field;
	unsigned int	byte;

	if(iformat == 0)
	{
		object::read (f, version, iformat);
		num_elements = getint(f);
		for(i = 1; i <= num_elements; i++)
			set_element(i, getstring(f));
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                	{
                                case ARRAY_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case ARRAY_NAME:
                                        set_name(getstring (f));
                                        break;
                                case ARRAY_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case ARRAY_NEXT:
                                        set_next (getref (f));
                                        break;
                                case ARRAY_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
				case ARRAY_ELEMENTS:
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Array::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return(True);
}


const Boolean
Describable_object::write (
FILE	*f)
const

{
	Array_storage::write (f);
	return (True);
}


const Boolean
Describable_object::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

	if(iformat==0)
	{
		object::read (f, version, iformat);
		set_description (getstring (f));
	}
	else
        {
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case DOBJECT_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case DOBJECT_NAME:
                                        set_name(getstring (f));
                                        break;
                                case DOBJECT_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case DOBJECT_NEXT:
                                        set_next (getref (f));
                                        break;
                                case DOBJECT_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
				case DOBJECT_DESC:
					set_description(getstring(f));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Describable_object::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
Lockable_object::write (
FILE	*f)
const

{
	Describable_object::write (f);
	if(key)
	{
		putfieldtype(f, LOBJECT_KEY);
		putboolexp	(f, key);
	}
	if(fail_message)
	{
		putfieldtype(f, LOBJECT_FAIL);
		putstring	(f, fail_message);
	}
	if(succ_message)
	{
		putfieldtype(f, LOBJECT_SUCC);
		putstring	(f, succ_message);
	}
	if(drop_message)
	{
		putfieldtype(f, LOBJECT_DROP);
		putstring	(f, drop_message);
	}
	if(ofail)
	{
		putfieldtype(f, LOBJECT_OFAIL);
		putstring	(f, ofail);
	}
	if(osuccess)
	{
		putfieldtype(f, LOBJECT_OSUCC);
		putstring	(f, osuccess);
	}
	if(odrop)
	{
		putfieldtype(f, LOBJECT_ODROP);
		putstring	(f, odrop);
	}
	return (True);
}


const Boolean
Lockable_object::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

	if(iformat==0)
	{
		Describable_object::read (f, version, iformat);
		set_key_no_free (getboolexp (f, version, iformat));
		set_fail_message (getstring (f));
		set_succ_message (getstring (f));
		set_drop_message (getstring (f));
		set_ofail (getstring (f));
		set_osuccess (getstring (f));
		set_odrop (getstring (f));
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case LOBJECT_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case LOBJECT_NAME:
                                        set_name(getstring (f));
                                        break;
                                case LOBJECT_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case LOBJECT_NEXT:
                                        set_next (getref (f));
                                        break;
                                case LOBJECT_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case LOBJECT_DESC:
                                        set_description(getstring(f));
                                        break;
				case LOBJECT_KEY:
					set_key_no_free (getboolexp (f, version, iformat));
					break;
				case LOBJECT_FAIL:
					set_fail_message (getstring (f));
					break;
				case LOBJECT_SUCC:
					set_succ_message (getstring (f));
					break;
				case LOBJECT_DROP:
					set_drop_message (getstring (f));
					break;
				case LOBJECT_OFAIL:
					set_ofail (getstring (f));
					break;
				case LOBJECT_OSUCC:
					set_osuccess (getstring (f));
					break;
				case LOBJECT_ODROP:
					set_odrop (getstring (f));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Lockable_object::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
Inheritable_object::write (
FILE    *f)
const

{
	Lockable_object::write (f);
	if(parent != NOTHING)
	{
		putfieldtype(f, IOBJECT_PARENT);
		putref          (f, parent);
	}
	return (True);
}


const Boolean
Inheritable_object::read (
	FILE    *f,
const	int     version,
const	int	iformat)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

	if(iformat==0)
	{
		Lockable_object::read (f, version, iformat);
		/* Parent set using direct assignment to avoid set_referenced() on an object that isn't read yet */
		parent = getref (f);
	}
	else
        {
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case IOBJECT_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case IOBJECT_NAME:
                                        set_name(getstring (f));
                                        break;
                                case IOBJECT_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case IOBJECT_NEXT:
                                        set_next (getref (f));
                                        break;
                                case IOBJECT_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case IOBJECT_DESC:
                                        set_description(getstring(f));
                                        break;
                                case IOBJECT_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case IOBJECT_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case IOBJECT_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case IOBJECT_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case IOBJECT_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case IOBJECT_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case IOBJECT_ODROP:
                                        set_odrop (getstring (f));
                                        break;
				case IOBJECT_PARENT:
					parent = getref(f);
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Inheritable_object::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
Old_object::write (
FILE	*f)
const

{
	Inheritable_object::write (f);
	if(commands != NOTHING)
	{
		putfieldtype(f, OOBJECT_COMMANDS);
		putref		(f, commands);
	}
	if(contents != NOTHING)
	{
		putfieldtype(f, OOBJECT_CONTENTS);
		putref		(f, contents);
	}
	if(destination != NOTHING)
	{
		putfieldtype(f, OOBJECT_DESTINATION);
		putref		(f, destination);
	}
	if(exits != NOTHING)
	{
		putfieldtype(f, OOBJECT_EXITS);
		putref		(f, exits);
	}
	if(fuses != NOTHING)
	{
		putfieldtype(f, OOBJECT_FUSES);
		putref		(f, fuses);
	}
	if(info_items != NOTHING)
	{
		putfieldtype(f, OOBJECT_INFO);
		putref		(f, info_items);
	}
	return (True);
}


const Boolean
Old_object::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

	if(iformat == 0)
	{
		Inheritable_object::read (f, version, iformat);
		set_commands (getref (f));
		set_contents (getref (f));
	/* Destination set using direct assignment to avoid set_referenced() on an object that isn't read yet */
		destination = getref (f);
		set_exits (getref (f));
		set_fuses (getref (f));
		set_info_items (getref (f));
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case OOBJECT_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case OOBJECT_NAME:
                                        set_name(getstring (f));
                                        break;
                                case OOBJECT_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case OOBJECT_NEXT:
                                        set_next (getref (f));
                                        break;
                                case OOBJECT_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case OOBJECT_DESC:
                                        set_description(getstring(f));
                                        break;
                               	case OOBJECT_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case OOBJECT_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case OOBJECT_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case OOBJECT_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case OOBJECT_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case OOBJECT_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case OOBJECT_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case OOBJECT_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
				case OOBJECT_COMMANDS:
					set_commands (getref (f));
					break;
				case OOBJECT_CONTENTS:
					set_contents (getref (f));
					break;
				case OOBJECT_DESTINATION:
					set_destination_no_check (getref (f));
					break;
				case OOBJECT_EXITS:
					set_exits (getref (f));
					break;
				case OOBJECT_FUSES:
					set_fuses (getref (f));
					break;
				case OOBJECT_INFO:
					set_info_items (getref (f));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements (f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Old_object::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}

const Boolean
Room::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Massy_object::read (f, version, iformat);
		if (version > 12)
			last_entry_time = getint (f);
		if (version < 15)
		{
			set_mass_limit (get_mass ());	/* on pre-version 15 db's mass was (supposed) to be used as mass limit */
			set_volume_limit (get_volume ());
			set_mass (STANDARD_ROOM_MASS);
			set_volume (STANDARD_ROOM_VOLUME);
			set_gravity_factor (STANDARD_ROOM_GRAVITY);
			if ((get_mass_limit () > 2100000000)
			|| (get_mass_limit () < 2200000000.0)
			|| (get_mass_limit () == 1000) || (get_mass_limit () == 0))
				set_mass_limit (STANDARD_ROOM_MASS_LIMIT);
			if ((get_volume_limit () > 2100000000)
			|| (get_volume_limit () < 2200000000.0)
			|| (get_volume_limit () == 200) || (get_volume_limit () == 0))
				set_volume_limit (STANDARD_ROOM_VOLUME_LIMIT);
		}
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case ROOM_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case ROOM_NAME:
                                        set_name(getstring (f));
                                        break;
                                case ROOM_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case ROOM_NEXT:
                                        set_next (getref (f));
                                        break;
                                case ROOM_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case ROOM_DESC:
                                        set_description(getstring(f));
                                        break;
                                case ROOM_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case ROOM_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case ROOM_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case ROOM_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case ROOM_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case ROOM_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case ROOM_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case ROOM_PARENT:
                                        set_parent_no_check (getref(f));
					break;
                                case ROOM_COMMANDS:
                                        set_commands (getref (f));
                                        break;
                                case ROOM_CONTENTS:
                                        set_contents (getref (f));
                                        break;
                                case ROOM_DESTINATION:
                                        set_destination_no_check (getref (f));
                                        break;
                                case ROOM_EXITS:
                                        set_exits (getref (f));
                                        break;
                                case ROOM_FUSES:
                                        set_fuses (getref (f));
                                        break;
                                case ROOM_INFO:
                                        set_info_items (getref (f));
                                        break;
                                case ROOM_GF:
                                        set_gravity_factor (getdouble (f));
                                        break;
                                case ROOM_MASS:
                                        set_mass (getdouble (f));
                                        break;
                                case ROOM_VOLUME:
                                        set_volume (getdouble (f));
                                        break;
                                case ROOM_MASSLIM:
                                        set_mass_limit (getdouble (f));
                                        break;
                                case ROOM_VOLUMELIM:
                                        set_volume_limit (getdouble (f));
                                        break;
				case ROOM_LASTENTRY:
					last_entry_time = getint (f);
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				case ROOM_CONTSTR:
					set_contents_string(getstring(f));
					break;
                                default:
                                        Trace( "Something has gone seriously wrong in Room::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}

const Boolean
Room::write (
FILE	*f)
const

{
	Massy_object::write (f);
	putfieldtype(f, ROOM_LASTENTRY);
	putint (f, last_entry_time);

	if(contents_string)
	{
		putfieldtype(f, ROOM_CONTSTR);
		putstring(f, contents_string);
	}
	return (True);
}

const Boolean
Massy_object::write (
FILE	*f)
const

{
	Old_object::write (f);
	if(gravity_factor != 1)
	{
		putfieldtype(f, MOBJECT_GF);
		putdouble (f, gravity_factor);
	}
	if(mass)
	{
		putfieldtype(f, MOBJECT_MASS);
		putdouble (f, mass);
	}
	if(volume)
	{
		putfieldtype(f, MOBJECT_VOLUME);
		putdouble (f, volume);
	}
	if(mass_limit)
	{
		putfieldtype(f, MOBJECT_MASSLIM);
		putdouble (f, mass_limit);
	}
	if(volume_limit)
	{
		putfieldtype(f, MOBJECT_VOLUMELIM);
		putdouble (f, volume_limit);
	}
	return (True);
}


const Boolean
Massy_object::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Old_object::read (f, version, iformat);
		if (version < 15)
		{
			set_mass (getdouble (f));
			set_volume (getdouble (f));
		}
		else
		{
			set_gravity_factor (getdouble (f));
			set_mass (getdouble (f));
			set_volume (getdouble (f));
			set_mass_limit (getdouble (f));
			set_volume_limit (getdouble (f));
		}
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case MOBJECT_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case MOBJECT_NAME:
                                        set_name(getstring (f));
                                        break;
                                case MOBJECT_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case MOBJECT_NEXT:
                                        set_next (getref (f));
                                        break;
                                case MOBJECT_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case MOBJECT_DESC:
                                        set_description(getstring(f));
                                        break;
                                case MOBJECT_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case MOBJECT_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case MOBJECT_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case MOBJECT_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case MOBJECT_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case MOBJECT_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case MOBJECT_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case MOBJECT_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
                                case MOBJECT_COMMANDS:
                                        set_commands (getref (f));
                                        break;
                                case MOBJECT_CONTENTS:
                                        set_contents (getref (f));
                                        break;
                                case MOBJECT_DESTINATION:
                                        set_destination_no_check (getref (f));
                                        break;
                                case MOBJECT_EXITS:
                                        set_exits (getref (f));
                                        break;
                                case MOBJECT_FUSES:
                                        set_fuses (getref (f));
                                        break;
                                case MOBJECT_INFO:
                                        set_info_items (getref (f));
                                        break;
				case MOBJECT_GF:
					set_gravity_factor (getdouble (f));
					break;
				case MOBJECT_MASS:
					set_mass (getdouble (f));
					break;
				case MOBJECT_VOLUME:
					set_volume (getdouble (f));
					break;
				case MOBJECT_MASSLIM:
					set_mass_limit (getdouble (f));
					break;
				case MOBJECT_VOLUMELIM:
					set_volume_limit (getdouble (f));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements (f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Massy_object::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
puppet::read(
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Massy_object::read(f, version, iformat);
		set_pennies(getint(f));
		set_score(getint(f));
		if(version < 16)
		{
			getint(f);	/* Read strength */
			getint (f);	/* Read paralysistime */
		}
		set_race (getstring (f));
		set_who_string (getstring (f));
		set_email_addr (getstring (f));
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case PUPPET_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case PUPPET_NAME:
                                        set_name(getstring (f));
                                        break;
                                case PUPPET_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case PUPPET_NEXT:
                                        set_next (getref (f));
                                        break;
                                case PUPPET_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case PUPPET_DESC:
                                        set_description(getstring(f));
                                        break;
                                case PUPPET_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case PUPPET_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case PUPPET_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case PUPPET_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case PUPPET_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case PUPPET_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case PUPPET_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case PUPPET_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
                                case PUPPET_COMMANDS:
                                        set_commands (getref (f));
                                        break;
                                case PUPPET_CONTENTS:
                                        set_contents (getref (f));
                                        break;
                                case PUPPET_DESTINATION:
                                        set_destination_no_check (getref (f));
                                        break;
                                case PUPPET_EXITS:
                                        set_exits (getref (f));
                                        break;
                                case PUPPET_FUSES:
                                        set_fuses (getref (f));
                                        break;
                                case PUPPET_INFO:
                                        set_info_items (getref (f));
                                        break;
                                case PUPPET_GF:
                                        set_gravity_factor (getdouble (f));
                                        break;
                                case PUPPET_MASS:
                                        set_mass (getdouble (f));
                                        break;
                                case PUPPET_VOLUME:
                                        set_volume (getdouble (f));
                                        break;
                                case PUPPET_MASSLIM:
                                        set_mass_limit (getdouble (f));
                                        break;
                                case PUPPET_VOLUMELIM:
                                        set_volume_limit (getdouble (f));
                                        break;
				case PUPPET_PENNIES:
					set_pennies(getint(f));
					break;
				case PUPPET_SCORE:
					set_score(getint(f));
					break;
				case PUPPET_RACE:
					set_race (getstring (f));
					break;
				case PUPPET_WHOSTRING:
					set_who_string (getstring (f));
					break;
				case PUPPET_EMAIL:
					set_email_addr (getstring (f));
					break;
				case PUPPET_MONEY:
					set_money (getint (f));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				case PUPPET_BUILDID:
					build_id = getref (f);
					break;
				default:
					Trace( "Something has gone seriously wrong in Puppet::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
puppet::write (
FILE	*f)
const

{
	Massy_object::write(f);
	if(pennies)
	{
		putfieldtype(f, PUPPET_PENNIES);
		putint		(f, pennies);
	}
	if(score)
	{
		putfieldtype(f, PUPPET_SCORE);
		putint		(f, score);
	}
	if(race)
	{
		putfieldtype(f, PUPPET_RACE);
		putstring	(f, race);
	}
	if(who_string)
	{
		putfieldtype(f, PUPPET_WHOSTRING);
		putstring	(f, who_string);
	}
	if(email_addr)
	{
		putfieldtype(f, PUPPET_EMAIL);
		putstring	(f, email_addr);
	}
	if(money)
	{
		putfieldtype(f, PUPPET_MONEY);
		putint		(f, money);
	}
	putfieldtype(f, PLAYER_BUILDID);
	putref		(f, get_build_id());
	return (True);
}


const Boolean
Player::write (
FILE	*f)
const

{
	int	i;

	puppet::write (f);
	putfieldtype	(f, PLAYER_PASSWORD);
	putstring	(f, password);
#ifdef ALIASES
	putfieldtype(f, PLAYER_ALIASES);
	for (i = 0; i < MAX_ALIASES; i++)
		putstring	(f, alias[i]);
#endif /* ALIASES */
	putfieldtype(f, PLAYER_CONTROLLER);
	putref		(f, controller);
	putfieldtype(f, PLAYER_BUILDID);
	putref		(f, get_build_id());
	/* Combat Stats */
#if 0
	putref		(f, weapon);
	putref		(f, armour);
	putint		(f, strength);
	putint		(f, dexterity);
	putint		(f, constitution);
	putint		(f, max_hit_points);
	putint		(f, hit_points);
	putint		(f, experience);
#endif
	if(colour)
	{
		putfieldtype(f, PLAYER_COLOUR);
		putstring	(f, colour);
	}
	return (True);
}


const Boolean
Player::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	int	i;
	char	*field;
	unsigned int byte;

	if(iformat==0)
	{
		if(version > 13)
		{
			puppet::read (f, version, iformat);
			set_password (getstring (f));
#ifdef ALIASES
				for (i = 0; i < MAX_ALIASES; i++)
					set_alias(i, getstring (f));
#endif /* ALIASES */
		/* Controller set using direct assignment to avoid set_referenced() on an object that isn't read yet */
			controller = getref (f);
			if(version > 15)
				set_build_id(getref (f));
			if(version > 17)
			{
				set_colour(getstring(f));
#if 0
				/* Combat Stats */
				weapon = getref(f);
				armour = getref(f);
				strength = getref(f);
				dexterity = getref(f);
				constitution = getref(f);
				max_hit_points = getref(f);
				hit_points = getref(f);
				experience = getref(f);
#endif
			}
		}
		else
		{
			Massy_object::read(f, version, iformat);
			set_pennies (getint (f));
			set_password (getstring (f));
#ifdef ALIASES
			if (version > 12)
				for (i = 0; i < MAX_ALIASES; i++)
					set_alias(i, getstring (f));
#endif /* ALIASES */
			set_score (getint (f));
			getint (f);		/* Strength */
			getint (f);		/* Constitution */
			getint (f);		/* Dexterity */
			getint (f);		/* Perception */
			getint (f);		/* Intelligence */
			getint (f);		/* Hitpoints */
			getint (f);		/* Armourclass */
			getint (f);		/* Paralysis time*/
			getint (f);		/* Level */
			getint (f);		/* Experience */
			set_race (getstring (f));
		/* Controller set using direct assignment to avoid set_referenced() on an object that isn't read yet */
			controller = getref (f);
			set_who_string (getstring (f));
			set_email_addr (getstring (f));
		}
		if (version < 15)
		{
			set_mass_limit (STANDARD_PLAYER_MASS_LIMIT);
			set_volume_limit (STANDARD_PLAYER_VOLUME_LIMIT);
			set_gravity_factor (STANDARD_PLAYER_GRAVITY);
		}
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case PLAYER_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case PLAYER_NAME:
                                        set_name(getstring (f));
                                        break;
                                case PLAYER_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case PLAYER_NEXT:
                                        set_next (getref (f));
                                        break;
                                case PLAYER_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case PLAYER_DESC:
                                        set_description(getstring(f));
                                        break;
                                case PLAYER_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case PLAYER_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case PLAYER_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case PLAYER_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case PLAYER_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case PLAYER_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case PLAYER_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case PLAYER_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
                                case PLAYER_COMMANDS:
                                        set_commands (getref (f));
                                        break;
                                case PLAYER_CONTENTS:
                                        set_contents (getref (f));
                                        break;
                                case PLAYER_DESTINATION:
                                        set_destination_no_check (getref (f));
                                        break;
                                case PLAYER_EXITS:
                                        set_exits (getref (f));
                                        break;
                                case PLAYER_FUSES:
                                        set_fuses (getref (f));
                                        break;
                                case PLAYER_INFO:
                                        set_info_items (getref (f));
                                        break;
                                case PLAYER_GF:
                                        set_gravity_factor (getdouble (f));
                                        break;
                                case PLAYER_MASS:
                                        set_mass (getdouble (f));
                                        break;
                                case PLAYER_VOLUME:
                                        set_volume (getdouble (f));
                                        break;
                                case PLAYER_MASSLIM:
                                        set_mass_limit (getdouble (f));
                                        break;
                                case PLAYER_VOLUMELIM:
                                        set_volume_limit (getdouble (f));
                                        break;
                                case PLAYER_PENNIES:
                                        set_pennies(getint(f));
                                        break;
                                case PLAYER_SCORE:
                                        set_score(getint(f));
                                        break;
                                case PLAYER_RACE:
                                        set_race (getstring (f));
                                        break;
                                case PLAYER_WHOSTRING:
                                        set_who_string (getstring (f));
                                        break;
                                case PLAYER_EMAIL:
                                        set_email_addr (getstring (f));
                                        break;
				case PLAYER_PASSWORD:
					set_password (getstring (f));
					break;
				case PLAYER_CONTROLLER:
					controller = getref (f);
					break;
				case PLAYER_BUILDID:
					set_build_id (getref (f));
					break;
				case PLAYER_COLOUR:
					set_colour(getstring(f));
					break;
				case PLAYER_ALIASES:
					for (i = 0; i < MAX_ALIASES; i++)
						set_alias(i, getstring (f));
					break;
				case PLAYER_MONEY:
					set_money (getint (f));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
				default:
					Trace( "Something has gone seriously wrong in Player::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
			}
		}
	}
	return (True);
}


const Boolean
Thing::write (
FILE	*f)
const

{
	Massy_object::write (f);
	if(contents_string)
	{
		putfieldtype(f, THING_CONTSTR);
		putstring	(f, contents_string);
	}
	if(lock_key)
	{
		putfieldtype(f, THING_KEYNOFREE);
		putboolexp	(f, lock_key);
	}
	return (True);
}


const Boolean
Thing::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Massy_object::read (f, version, iformat);
		set_contents_string (getstring (f));
		set_lock_key_no_free (getboolexp (f, version, iformat));
		if (version < 15)
		{
			set_mass_limit (STANDARD_THING_MASS_LIMIT);
			set_volume_limit (STANDARD_THING_VOLUME_LIMIT);
			set_gravity_factor (STANDARD_THING_GRAVITY);
		}
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case THING_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case THING_NAME:
                                        set_name(getstring (f));
                                        break;
                                case THING_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case THING_NEXT:
                                        set_next (getref (f));
                                        break;
                                case THING_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case THING_DESC:
                                        set_description(getstring(f));
                                        break;
                                case THING_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case THING_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case THING_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case THING_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case THING_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case THING_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case THING_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case THING_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
                                case THING_COMMANDS:
                                        set_commands (getref (f));
                                        break;
                                case THING_CONTENTS:
                                        set_contents (getref (f));
                                        break;
                                case THING_DESTINATION:
                                        set_destination_no_check (getref (f));
                                        break;
                                case THING_EXITS:
                                        set_exits (getref (f));
                                        break;
                                case THING_FUSES:
                                        set_fuses (getref (f));
                                        break;
                                case THING_INFO:
                                        set_info_items (getref (f));
                                        break;
                                case THING_GF:
                                        set_gravity_factor (getdouble (f));
                                        break;
                                case THING_MASS:
                                        set_mass (getdouble (f));
                                        break;
                                case THING_VOLUME:
                                        set_volume (getdouble (f));
                                        break;
                                case THING_MASSLIM:
                                        set_mass_limit (getdouble (f));
                                        break;
                                case THING_VOLUMELIM:
                                        set_volume_limit (getdouble (f));
                                        break;
				case THING_CONTSTR:
					set_contents_string (getstring (f));
					break;
				case THING_KEYNOFREE:
					set_lock_key_no_free (getboolexp (f, version, iformat));
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
                                default:
                                        Trace( "Something has gone seriously wrong in Thing::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
                        }
                }
	}
	return (True);
}


const Boolean
Weapon::write (
FILE	*f)
const

{
	Inheritable_object::write (f);
	if(degradation)
	{
		 putfieldtype(f, WEAPON_DEGRAD);
		 putint	(f, degradation);
	}
	if(damage)
	{
		putfieldtype(f, WEAPON_DAMAGE);
		putint	(f, damage);
	}
	if(speed)
	{
		putfieldtype(f, WEAPON_SPEED);
		putint	(f, speed);
	}
	if(ammunition)
	{
		putfieldtype(f, WEAPON_AMMO);
		putint	(f, ammunition);
	}
	if(range)
	{
		putfieldtype(f, WEAPON_RANGE);
		putint	(f, range);
	}
	if(parent_ammunition)
	{
		putfieldtype(f, WEAPON_PAMMO);
		putref	(f, parent_ammunition);
	}
	return (True);
}


const Boolean
Weapon::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Inheritable_object::read (f, version, iformat);
		degradation	= getint(f);
		damage		= getint(f);
		speed		= getint(f);
		ammunition	= getint(f);
		range		= getint(f);
		parent_ammunition = getref(f);
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case WEAPON_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case WEAPON_NAME:
                                        set_name(getstring (f));
                                        break;
                                case WEAPON_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case WEAPON_NEXT:
                                        set_next (getref (f));
                                        break;
                                case WEAPON_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case WEAPON_DESC:
                                        set_description(getstring(f));
                                        break;
                                case WEAPON_KEY:
                                        set_key_no_free (getboolexp (f, version,
 iformat));
                                        break;
                                case WEAPON_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case WEAPON_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case WEAPON_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case WEAPON_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case WEAPON_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case WEAPON_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case WEAPON_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
				case WEAPON_DEGRAD:
					degradation     = getint(f);
					break;
				case WEAPON_DAMAGE:
					damage          = getint(f);
					break;
				case WEAPON_SPEED:
					speed           = getint(f);
					break;
				case WEAPON_AMMO:
					ammunition      = getint(f);
					break;
				case WEAPON_RANGE:
					range           = getint(f);
					break;
				case WEAPON_PAMMO:
					parent_ammunition = getref(f);
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
                                default:
                                        Trace( "Something has gone seriously wrong in Weapon::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
                        }
                }
	}
	return (True);
}


const Boolean
Armour::write (
FILE	*f)
const

{
	Inheritable_object::write (f);
	if(degradation)
	{
		putfieldtype(f, ARMOUR_DEGRAD);
		putint(f, degradation);
	}
	if(protection)
	{
		putfieldtype(f, ARMOUR_PROTEC);
		putint(f, protection);
	}
	return (True);
}


const Boolean
Armour::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Inheritable_object::read (f, version, iformat);
		degradation = getint(f);
		protection = getint(f);
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case ARMOUR_FLAGS:
                                        field = get_next_field(f);

                                        for(int count=0;count < FLAGS_WIDTH;count++)
                                        {
                                                sscanf(&field[count*2], "%02x", &byte);
                                                set_flag_byte(count, byte);
                                        }
                                        break;
                                case ARMOUR_NAME:
                                        set_name(getstring (f));
                                        break;
                                case ARMOUR_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case ARMOUR_NEXT:
                                        set_next (getref (f));
                                        break;
                                case ARMOUR_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case ARMOUR_DESC:
                                        set_description(getstring(f));
                                        break;
                                case ARMOUR_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case ARMOUR_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case ARMOUR_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case ARMOUR_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case ARMOUR_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case ARMOUR_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case ARMOUR_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case ARMOUR_PARENT:
                                        set_parent_no_check (getref(f));
                                        break;
				case ARMOUR_DEGRAD:
					degradation = getint(f);
					break;
				case ARMOUR_PROTEC:
					protection = getint(f);
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
                                default:
                                        Trace( "Something has gone seriously wrong in Armour::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
                        }
                }
	}
	return (True);
}


const Boolean
Ammunition::write (
FILE	*f)
const

{
	Inheritable_object::write (f);
	if(count)
	{
		putfieldtype(f, AMMO_COUNT);
		putint(f, count);
	}
	return (True);
}


const Boolean
Ammunition::read (
	FILE	*f,
const	int	version,
const	int	iformat)

{
	int fieldtype;
	char *field;
	unsigned int byte;

	if(iformat==0)
	{
		Inheritable_object::read (f, version, iformat);
		count = getint(f);
	}
	else
	{
		for(fieldtype=getfieldtype(f); fieldtype != 257; fieldtype=getfieldtype(f))
        	{
                	switch(fieldtype)
                        {
                                case AMMO_FLAGS:
                                        field = get_next_field(f);

                                        for(int cnt=0;cnt < FLAGS_WIDTH;cnt++)
                                        {
                                                sscanf(&field[cnt*2], "%02x", &byte);
                                                set_flag_byte(cnt, byte);
                                        }
                                        break;
                                case AMMO_NAME:
                                        set_name(getstring (f));
                                        break;
                                case AMMO_LOCATION:
                                        set_location (getref (f));
                                        break;
                                case AMMO_NEXT:
                                        set_next (getref (f));
                                        break;
                                case AMMO_OWNER:
                                        set_owner_no_check (getref(f));
                                        break;
                                case AMMO_DESC:
                                        set_description(getstring(f));
                                        break;
                                case AMMO_KEY:
                                        set_key_no_free (getboolexp (f, version, iformat));
                                        break;
                                case AMMO_FAIL:
                                        set_fail_message (getstring (f));
                                        break;
                                case AMMO_SUCC:
                                        set_succ_message (getstring (f));
                                        break;
                                case AMMO_DROP:
                                        set_drop_message (getstring (f));
                                        break;
                                case AMMO_OFAIL:
                                        set_ofail (getstring (f));
                                        break;
                                case AMMO_OSUCC:
                                        set_osuccess (getstring (f));
                                        break;
                                case AMMO_ODROP:
                                        set_odrop (getstring (f));
                                        break;
                                case AMMO_PARENT:
                                        set_parent_no_check (getref(f));
					break;
				case AMMO_COUNT:
					count = getint(f);
					break;
				case ARRAY_STORAGE_ELEMENTS:
					get_array_elements(f, this);
					break;
                                default:
                                        Trace( "Something has gone seriously wrong in Ammunition::read\n");
					Trace( "Field Type: %d\n", fieldtype);
					exit(1);
                        }
                }
	}
	return (True);
}

const Boolean
Database::write (
FILE	*f)
const

{
	dbref	i;
	char	buf [20];

	/* Write the db descriptor */
	/* Version 16 is moving to having build_id's and no attribs */
	/* Version 17 is a cool new flag system that will last forever */
	/* Version 18 is the colour things */
	fprintf (f, "***UglyMug Beta(tagged) DUMP Format 0***\n");

	/* Write the number of objects in the database */
	fprintf (f, "Objects: %d\n", top);

	/* Write each object */
	for (i = 0; i < top; i++)
		if (db + i != NULL)
		{
			sprintf (buf, "%c#%d", OBJECT_SEPARATOR, i);
			putstring (f, buf);
			/* Then put the type field since it is not
			   available inside the object, the flags
			   are done inside the object */

			putint (f, Typeof(i));
			db [i].write (f);
		}

	/* That's all folks */
	putstring(f, "***END OF DUMP***");
	fflush(f);

	return(True);
}


const dbref
parse_dbref (
const	char *const	s)

{
	const	char	*p = s;
	long		x;
	int		chars;

	/* If it's NULL, return NOTHING. */
	if (s == NULL)
		return (NOTHING);

	/* If there's a leading hash, skip it */
	if (*p == NUMBER_TOKEN)
		p++;

	if (sscanf (p, "%ld%n", &x, &chars) == 1)
		/* Valid number */
		if ((x >= 0) && (p [chars] == '\0'))
			return (x);
		else
			return (NOTHING);
	else
		/* Invalid number */
		return NOTHING;
}

static const dbref
getref (
FILE	*f)

{
	return(atol(get_next_field(f)));
}


static const int
getint (
FILE	*f)

{
	return(atol(get_next_field(f)));
}

static const double
getdouble (
FILE	*f)

{
	char * temp;
	if(strcmp((temp = get_next_field(f)), "INF"))
		return(strtod(temp,(char **)NULL));
	else
		return HUGE_VAL;
}

static const char *
getstring (
FILE	*f)

{
	return (get_next_field(f));
}


boolexp *
getboolexp1(
char    **buffer_ptr,
int	version,
int	iformat)

{
	boolexp *b;
	boolexp	*lhs;
	int	c;

	c = *(*buffer_ptr)++;
	switch(c)
	{
		case '\0':
			return TRUE_BOOLEXP;
			/* break; */
		case '(':
			if((c = *(*buffer_ptr)++) == '!')
			{
				b = new boolexp (BOOLEXP_NOT);
				b->sub1 = getboolexp1(buffer_ptr, version, iformat);
				if((*(*buffer_ptr)++) != ')') goto error;
				return b;
			}
			else if (c == '@')
			{
				b = new boolexp (BOOLEXP_FLAG);
				b->thing = 0;

	/* NOTE possibly non-portable code */
	/* Will need to be changed if putref/getref change */
				while(isdigit(c = *(*buffer_ptr)++))
					b->thing = b->thing * 10 + c - '0';
				if((iformat == 0) && (version < 17))
					b->thing = convert_flag(b->thing);

				if (c != ')') goto error;
				return b;
			}
			else
			{
				(*buffer_ptr)--;
				lhs = getboolexp1(buffer_ptr, version, iformat);
				switch(c = *(*buffer_ptr)++)
				{
					case AND_TOKEN:
						b = new boolexp (BOOLEXP_AND);
						b->sub1 = lhs;
						break;
					case OR_TOKEN:
						b = new boolexp (BOOLEXP_OR);
						b->sub1 = lhs;
						break;
					default:
						goto error;
						/* break */
				}
				b->sub2 = getboolexp1(buffer_ptr, version, iformat);
				if(*(*buffer_ptr)++ != ')') goto error;
				return b;
			}
			/* break; */
		default:
			/* better be a dbref */
			(*buffer_ptr)--;
			b = new boolexp (BOOLEXP_CONST);
			b->thing = 0;
			/* NOTE possibly non-portable code */
			/* Will need to be changed if putref/getref change */
			while(isdigit(c = *(*buffer_ptr)++))
				b->thing = b->thing * 10 + c - '0';
			(*buffer_ptr)--;
			return b;
		}

	error:
		Trace("getboolexp1:  INVALID EXPRESSION\n");
		abort();			/* bomb out */
	return (TRUE_BOOLEXP);
}


boolexp *
getboolexp(
FILE	*f,
int	version,
int	iformat)

{
	char	*buf;

	buf = get_next_field(f);
	return (getboolexp1(&buf, version, iformat));
}


const Boolean
object::read_pre12 (
	FILE	*f,
const	int	/* version */)

{
	owner = getref		(f);
	return(True);
}


const Boolean
Old_object::read_pre12 (
	FILE	*f,
const	int	version)

{
	int			pennies;
	object_flag_type	type = TYPE_NO_TYPE;
//	= get_flags() & OLD_TYPE_MASK;
	dbref			scratch;

	set_name		(getstring	(f));
	set_description		(getstring	(f));
	set_location		(getref		(f));
	scratch = getref (f);
	if (type != TYPE_VARIABLE)
		destination = scratch;
	scratch = getref (f);
	if (type != TYPE_VARIABLE)
		set_contents (scratch);
	scratch = getref (f);
	if (type != TYPE_VARIABLE)
		set_exits (scratch);
	set_next		(getref		(f));
	set_key			(getboolexp	(f, version, 0));
	set_fail_message	(getstring	(f));
	set_succ_message	(getstring	(f));
	set_drop_message	(getstring	(f));
	set_ofail		(getstring	(f));
	set_osuccess		(getstring	(f));
	set_odrop		(getstring	(f));
	object::read_pre12(f, version);
	pennies	= getint (f);
	if (type == TYPE_PLAYER)
		set_pennies (pennies);
	scratch = getref (f);
	if (type != TYPE_VARIABLE)
		set_commands (scratch);
	scratch = getref (f);
	if (type != TYPE_VARIABLE)
		set_info_items (scratch);
	scratch = getref (f);
	if (type != TYPE_VARIABLE)
		set_fuses (scratch);
	switch (type)
	{
		case TYPE_ROOM:
			set_mass_limit		(getdouble (f));
			set_volume_limit	(getdouble (f));
			set_gravity_factor	(STANDARD_ROOM_GRAVITY);
			if (get_mass_limit () > 20000000)
				set_mass_limit (STANDARD_ROOM_MASS_LIMIT);
			if (get_volume_limit () > 20000000)
				set_volume_limit (STANDARD_ROOM_VOLUME_LIMIT);
			break;
		case TYPE_THING:
			set_contents_string	(getstring (f));
			set_mass		(getdouble (f));
			set_volume		(getdouble (f));
			set_lock_key		(getboolexp (f, version, 0));
			break;
		case TYPE_PLAYER:
			set_password		(getstring (f));
			set_score		(getint (f));
						getint (f);
						getint (f);
						getint (f);
						getint (f);
						getint (f);
						getint (f);
						getint (f);
						getint (f);
						getint (f);
						getint (f);
			set_race		(getstring (f));
			set_mass		(getdouble (f));
			set_volume		(getdouble (f));
			set_controller		(getref(f));
			set_who_string		(getstring (f));
			if (version >= 11)
				set_email_addr	(getstring(f));
			else
	      			set_email_addr	(NULL);

			break;
		case TYPE_EXIT:
		case TYPE_COMMAND:
		case TYPE_FUSE:
		case TYPE_ALARM:
		case TYPE_VARIABLE:
			break;
		default:
//			Trace( "BUG: Tried to read object of (unknown) type %d.\n", get_flags() & OLD_TYPE_MASK);
			return (False);
	}

	/* All went OK */
	return (True);
}


const dbref
Database::read (
FILE	*f)

{
	dbref			i;
	dbref			last_entry = NOTHING;
	object			*obj=NULL;
	object_flag_type	oldflags = 0;
	typeof_type		type=TYPE_NO_TYPE;
	int			version	= 0;
	char			format[20];
	int			iformat;
	char			ch;
	Boolean			error = False;

	/* Make sure we're empty */
	if (array != NULL)
		abort ();

	if(fscanf (f, "***UglyMug %s DUMP Format %d***", format, &version) != 2)
	{
		Trace( "File is not a recognised UglyMug dump format\n");
		return(NOTHING);
	}

	if (strcasecmp(format, "Beta(tagged)") == 0)
	{
		Trace( "Beta(tagged) Database DUMP format recognised.\n");
		iformat = 1;
	}
	else if (strcasecmp(format, "Alpha") == 0)
	{

		if (version < 9)
		{
			Trace( "Alpha Database DUMP format version %d is no longer supported.\n", version);
			return (NOTHING);
		}
		else
		{
			Trace( "Alpha Database DUMP format version %d recognised.\n", version);
			iformat = 0;
		}
	}
	else
	{
		Trace( "Unrecognised UglyMug DUMP format. Bailing out.\n");
		return(NOTHING);
	}

	getc(f);				/* Remove newline */
	if (fscanf (f, "Objects: %d", &i) != 1) /* Read the number of objects */
		return (NOTHING);		/* in the database. */
	getc(f);				/* grab newline */
	grow (i);			/* Allocate the main data structure */

	initialise_load_buffer(f);

	/* Read the objects up until end of dump */
	ch = get_next_char(f);
	while (True)
	{
		switch(ch)
		{
			case OBJECT_SEPARATOR:
				/* This is guaranteed to be a separator IF version > 13 */
				if(((iformat == 1) || (version > 13)) && ((ch = get_next_char(f)) != '#'))
				{
					Trace( "ERROR: Object not correctly saved - Looking for <\\002>#, got <\\002> 0x%x (%c).\n", ch, ch);
					return (-1);
				}
				/* another entry, yawn */
				i = getref(f);

				if (error)
				{
					Trace( "... skipped to #%d.\n", i);
					error = False;
				}

				if (i <= last_entry)
				{
					Trace( "Major Problem Dudes - Item #%d is out of order guv.\n", i);
					if((iformat == 1) || (version > 13))
						while((ch = get_next_char (f)) != OBJECT_SEPARATOR);
					else
						while ((ch = get_next_char (f)) != '#');
					error = True;
					continue;
				}

				if (i > last_entry+1)
					set_free_between (last_entry + 1, i - 1);

				/* read it in */
				if((iformat == 0) && (version < 17))
				{
					oldflags = getint (f);
					switch (oldflags & OLD_TYPE_MASK)
					{
						case OLD_TYPE_ALARM:
							type = TYPE_ALARM;
							break;
						case OLD_TYPE_COMMAND:
							type = TYPE_COMMAND;
							break;
						case OLD_TYPE_EXIT:
							type = TYPE_EXIT;
							break;
						case OLD_TYPE_FUSE:
							type = TYPE_FUSE;
							break;
						case OLD_TYPE_PLAYER:
							type = TYPE_PLAYER;
							break;
						case OLD_TYPE_PUPPET:
							type = TYPE_PUPPET;
							break;
						case OLD_TYPE_ROOM:
							type = TYPE_ROOM;
							break;
						case OLD_TYPE_THING:
							type = TYPE_THING;
							break;
						case OLD_TYPE_DICTIONARY:
							type = TYPE_DICTIONARY;
							break;
						case OLD_TYPE_PROPERTY:
							type = TYPE_PROPERTY;
							break;
						case OLD_TYPE_ARRAY:
							type = TYPE_ARRAY;
							break;
						case OLD_TYPE_VARIABLE:
							type = TYPE_VARIABLE;
							break;
						default:
							break;
					}
				}
				else
				{
				/* Get the type for 17+ version db's */
					type = getint(f);
				}

				switch (type)
				{
					case TYPE_ALARM:
						obj = new (Alarm);
						break;
					case TYPE_COMMAND:
						obj = new (Command);
						break;
					case TYPE_EXIT:
						obj = new (Exit);
						break;
					case TYPE_FUSE:
						obj = new (Fuse);
						break;
					case TYPE_PLAYER:
						obj = new (Player);
						player_count++;
						break;
					case TYPE_PUPPET:
						obj = new (puppet);
						obj->set_build_id(i);
						break;
					case TYPE_ROOM:
						obj = new (Room);
						break;
					case TYPE_THING:
						obj = new (Thing);
						break;
					case TYPE_DICTIONARY:
						obj = new (Dictionary);
						break;
					case TYPE_PROPERTY:
						obj = new (Property);
						break;
					case TYPE_ARRAY:
						obj = new (Array);
						break;
					case TYPE_VARIABLE:
						obj = new (Variable);
						break;
					case TYPE_WEAPON:
						obj = new (Weapon);
						break;
					case TYPE_ARMOUR:
						obj = new (Armour);
						break;
					case TYPE_AMMUNITION:
						obj = new (Ammunition);
						break;
					default:
						fputs ("Illegal object type in database load.\n", stderr);
				}
				array [i].set_obj (*obj);
				Settypeof(i, type);
				if((iformat == 0) && (version < 17))
					convert_flags(i, oldflags);

				if ((iformat == 0) && (version < 16))
				{
					if (version < 12)
						array [i].get_obj ()->read_pre12 (f, version);
					else
						array [i].get_obj ()->read (f, version, iformat);
					if((Typeof(i) == TYPE_PLAYER)
						|| (Typeof(i) == TYPE_PUPPET))
						db[i].reset_build_id(i);
				}
				else
					array [i].get_obj ()->read (f, version, iformat);
				last_entry = i;

#ifdef ALIASES
				if(Typeof(i) == TYPE_PLAYER)
					for(int alias=0; alias<MAX_ALIASES; alias++)
						if(db[i].get_alias(alias))
							player_count++;
#endif
				break;
			case '*':
				if(strcmp(getstring (f), "**END OF DUMP***"))
				{
					Trace( "Got a * without **END OF DUMP***.\n");
					return (-1);
				}
				else
				{
					set_free_between (last_entry + 1, top - 1);
					build_player_cache (player_count);
					return (top);
				}
				/* NOTREACHED */
				break;
			case '#':
				if((iformat == 0) && (version < 14))
				{
					ch = OBJECT_SEPARATOR;
					continue;
				}
			default:
				Trace( "Expected <\\002> or #, got 0x%x (%c). Last object read was #%d. Skipping to next #...\n",
					ch,
					ch,
					last_entry);
				if((iformat == 1) || (version > 13))
					while((ch = get_next_char (f)) != OBJECT_SEPARATOR);
				else
					while ((ch = get_next_char (f)) != '#');
				error = True;
				continue;
		}
		ch = get_next_char (f);

	}

	finalise_load_buffer ();

}


static	char	*buffer;
static	char	*load_pointer = NULL;

static void
initialise_load_buffer(
FILE *f)
{
	if ((buffer = (char *) malloc(LOAD_BUFFER_SIZE + 1)) == NULL)
	{
		Trace("initialise_load_buffer:  MALLOC FAILED\n");
		abort();
	}
	fread (buffer, sizeof(char), LOAD_BUFFER_SIZE, f);
	buffer[LOAD_BUFFER_SIZE] = '\0';
	load_pointer = buffer;
}

static const int
getfieldtype (
FILE	*f)

{
	unsigned char fieldtype = *load_pointer;

	if(fieldtype == 0)
	{
		reload_buffer(f);
		fieldtype = *load_pointer;
	}
	if((fieldtype == OBJECT_SEPARATOR) || (fieldtype == '*'))
		return(257);
	load_pointer++;
	return((int)fieldtype);
}

static void
putfieldtype (
FILE	*f,
int fieldtype)

{
	fprintf(f, "%c", (unsigned char) fieldtype);
}

static void
finalise_load_buffer ()

{
	free(buffer);
}

static char *
get_next_field(
FILE *f)

{
	char	*field_end;
	char	*field;


	if ((field_end = strchr(load_pointer, FIELD_SEPARATOR)) == NULL)
	{
		reload_buffer(f);
		field_end = strchr(load_pointer, FIELD_SEPARATOR);
	}
	/* By this point, field_end points to the end of the current field */
	/* and load_pointer points to the beginning. */

#ifdef DEBUG
	if (field_end - load_pointer >= BUFFER_LEN)
		Trace("BUG: Can't handle field size. Aborting db load.");
#endif

	*field_end = '\0';
	field = load_pointer;
	load_pointer = field_end + 1;
	return (field);
}

static char
get_next_char(
FILE *f)

{
	if (*load_pointer == '\0')
		reload_buffer(f);
	return(*load_pointer++);
}


static void
reload_buffer (
FILE *f)

{
	int	current_length;
	int	size;

	current_length = buffer + LOAD_BUFFER_SIZE - load_pointer;
	memcpy(buffer, load_pointer, current_length);
	size = fread (buffer + current_length, sizeof(char), load_pointer - buffer, f);
	buffer[current_length + size] = '\0';
	load_pointer = buffer;
}


static int compare_player_cache_entries(const void *p1, const void *p2)
{
	const struct player_cache_struct *s1=(const struct player_cache_struct *) p1;
	const struct player_cache_struct *s2=(const struct player_cache_struct *) p2;

	return strcasecmp(s1->name, s2->name);
}


void Database::build_player_cache(int player_count)
{
	int cache_entry=0;

	player_cache = (struct player_cache_struct *) calloc(sizeof(struct player_cache_struct), player_count);

	for(int i=0; i<top; i++)
		if(Typeof(i)==TYPE_PLAYER)
		{
			player_cache[cache_entry].state = CACHE_VALID;
			player_cache[cache_entry].name = (char *) strdup(db[i].get_name());
			player_cache[cache_entry++].player = i;
#ifdef ALIASES
			for(int alias=0; alias<MAX_ALIASES; alias++)
			{
				const	char	*alias_name;

				if((alias_name=db[i].get_alias(alias)))
				{
					player_cache[cache_entry].state = CACHE_VALID;
					player_cache[cache_entry].name = (char *) strdup(alias_name);
					player_cache[cache_entry++].player = i;
				}
			}
#endif
		}

	qsort(player_cache, player_count, sizeof(struct player_cache_struct), compare_player_cache_entries);
}


const dbref
Database::lookup_player (
const	char	*const	name)
const

{
	struct player_cache_struct *pc, key;

	if(blank(name))
		return NOTHING;

	/* Check for #number */

	if(*name=='#')
	{
		dbref result=atoi(name+1);

		if(Typeof(result)==TYPE_PLAYER)
			return result;
		else
			return NOTHING;
	}

	for(struct changed_player_list_struct *current=changed_player_list; current; current=current->next)
		if(strcasecmp(current->name, name)==0)
			return current->player;

	key.state=CACHE_VALID;
	key.name=name;

	if((pc=(struct player_cache_struct *) bsearch(&key, player_cache, player_count, sizeof(struct player_cache_struct), compare_player_cache_entries)))
	{
		if(pc->state==CACHE_INVALID)
			return NOTHING;

		return pc->player;
	}

	return NOTHING;
}


void
Database::add_player_to_cache (
	dbref	player,
const	char	*name)
{
	if(Typeof(player)!=TYPE_PLAYER)
		return;

	struct changed_player_list_struct *entry=(struct changed_player_list_struct *) malloc(sizeof(struct changed_player_list_struct));

	entry->name=strdup(name);
	entry->player=player;
	entry->prev=NULL;
	entry->next=changed_player_list;
	if(changed_player_list)
		changed_player_list->prev=entry;
	changed_player_list=entry;
}


void Database::remove_player_from_cache(const char *name)
{
	for(struct changed_player_list_struct *current=changed_player_list; current; )
		if(strcasecmp(current->name, name)==0)
		{
			struct changed_player_list_struct *old=current;

			if(current==changed_player_list)
			{
				changed_player_list=changed_player_list->next;
				if(changed_player_list)
					changed_player_list->prev=NULL;
			}
			else
			{
				current->prev->next=current->next;
				if(current->next)
					current->next->prev=current->prev;
			}

			current=current->next;

			free(old->name);
			free(old);
		}
		else
			current=current->next;

	for(int i=0; i<player_count; i++)
		if(strcasecmp(player_cache[i].name, name)==0)
			player_cache[i].state=CACHE_INVALID;
}

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

#include "config.h"
#include "log.h"

#define DB_MSGLEN	1024

#define FIELD_SEPARATOR '\027'
#define	OBJECT_SEPARATOR '\002'
//#define LOAD_BUFFER_SIZE (8*BUFFER_LEN)
#define LOAD_BUFFER_SIZE (1024*1024)

#ifdef DEBUG
#define FREE(x) { if((x)==NULL) \
			Trace( "WARNING:  attempt to free NULL pointer (%s, line %d)\n", __FILE__, __LINE__); \
		  else \
		  { \
		  	free((x)); \
		  	(x)=NULL; \
		  } \
	  	}
#else /* not DEBUG */
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
static	boolexp			*getboolexp	(FILE *);
static	const	dbref		getref		(FILE *);


static void
get_array_elements(
FILE	*f,
object	*it)
{
	unsigned int i;
	unsigned int num_elements;
	unsigned int use_elements;

	num_elements = getint(f);

// In case of the array sizes shrinking, or a badly saved array, truncate
// truncate arrays to their maximum lengths
	use_elements = ((it->get_flag(FLAG_WIZARD)) ? MIN(num_elements, MAX_WIZARD_ARRAY_ELEMENTS) : MIN(num_elements, MAX_MORTAL_ARRAY_ELEMENTS));

	for(i = 1; i <= use_elements; i++)
		it->set_element(i, getstring(f));
// In order to make the db load sane, junk additional elements.
	if(use_elements < num_elements)
	{
		log_bug("Array too large on database load, truncated from %d to %d elements", num_elements, use_elements);
		for(; i <= num_elements; i++)
			getstring(f);
	}
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
const 	String& s)
{
	if(s)
	{
		fputs(s.c_str(), f);
	}
	putc(FIELD_SEPARATOR, f);
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


const bool
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
	return (true);
}


const bool
object::read (
	FILE	*f)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

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
			log_bug("Something has gone seriously wrong in object::read\nField Type:%d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
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
	return (true);
}

const bool
Dictionary::read (
	FILE	*f)

{
	int		i;
	int		fieldtype;
	char		*temp;
	char		*field;
	unsigned int	byte;

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
			log_bug("Something has gone seriously wrong in Dictionary::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);

}


const bool
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
	return (true);
}


const bool
Array::write (
FILE	*f)
const

{
	Array_storage::write (f);
	return (true);
}


const bool
Array::read (
	FILE	*f)
{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

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
			log_bug("Something has gone seriously wrong in Array::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return(true);
}


const bool
Describable_object::write (
FILE	*f)
const

{
	Array_storage::write (f);
	return (true);
}


const bool
Describable_object::read (
	FILE	*f)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

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
			log_bug("Something has gone seriously wrong in Describable_object::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
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
	return (true);
}


const bool
Lockable_object::read (
	FILE	*f)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

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
			set_key_no_free (getboolexp (f));
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
			log_bug("Something has gone seriously wrong in Lockable_object::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
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
	return (true);
}


const bool
Inheritable_object::read (
	FILE    *f)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

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
			set_key_no_free (getboolexp (f));
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
			log_bug("Something has gone seriously wrong in Inheritable_object::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
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
	return (true);
}


const bool
Old_object::read (
	FILE	*f)

{
	int		fieldtype;
	char		*field;
	unsigned int	byte;

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
			set_key_no_free (getboolexp (f));
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
			log_bug("Something has gone seriously wrong in Old_object::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}

const bool
Room::read (
	FILE	*f)

{
	int fieldtype;
	char *field;
	unsigned int byte;

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
			set_key_no_free (getboolexp (f));
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
			log_bug("Something has gone seriously wrong in Room::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}

const bool
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
	return (true);
}

const bool
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
	return (true);
}


const bool
Massy_object::read (
	FILE	*f)

{
	int fieldtype;
	char *field;
	unsigned int byte;

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
			set_key_no_free (getboolexp (f));
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
			log_bug("Something has gone seriously wrong in Massy_object::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
puppet::read(
	FILE	*f)

{
	int fieldtype;
	char *field;
	unsigned int byte;

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
			set_key_no_free (getboolexp (f));
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
			log_bug("Something has gone seriously wrong in Puppet::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
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
	return (true);
}


const bool
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
	return (true);
}


const bool
Player::read (
	FILE	*f)

{
	int fieldtype;
	int	i;
	char	*field;
	unsigned int byte;

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
			set_key_no_free (getboolexp (f));
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
			reset_build_id (getref (f));
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
			log_bug("Something has gone seriously wrong in Player::read\nField Type: %d", fieldtype);
			exit(1);
		}
	}
	return (true);
}


const bool
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
	return (true);
}


const bool
Thing::read (
	FILE	*f)

{
	int fieldtype;
	char *field;
	unsigned int byte;

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
			set_key_no_free (getboolexp (f));
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
			set_lock_key_no_free (getboolexp (f));
			break;
		case ARRAY_STORAGE_ELEMENTS:
			get_array_elements(f, this);
			break;
		default:
			log_bug("Something has gone seriously wrong in Thing::read\nField Type: %d", fieldtype);
			exit(1);
                }
	}
	return (true);
}


const bool
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

	return(true);
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
char    **buffer_ptr)

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
				b->sub1 = getboolexp1(buffer_ptr);
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

				if (c != ')') goto error;
				return b;
			}
			else
			{
				(*buffer_ptr)--;
				lhs = getboolexp1(buffer_ptr);
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
				b->sub2 = getboolexp1(buffer_ptr);
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
		log_bug("getboolexp1: INVALID EXPRESSION");
		abort();			/* bomb out */
	return (TRUE_BOOLEXP);
}


boolexp *
getboolexp(
FILE	*f)

{
	char	*buf;

	buf = get_next_field(f);
	return (getboolexp1(&buf));
}


const dbref
Database::read (
FILE	*f)

{
	dbref			i;
	dbref			last_entry = NOTHING;
	object			*obj=NULL;
	typeof_type		type=TYPE_NO_TYPE;
	int			version	= 0;
	char			format[20];
	char			ch;
	bool			error = false;

	/* Make sure we're empty */
	if (array != NULL)
		abort ();

	if(fscanf (f, "***UglyMug %s DUMP Format %d***", format, &version) != 2)
	{
		log_bug("File is not a recognised UglyMug dump format");
		return(NOTHING);
	}

	if (string_compare(format, "Beta(tagged)") == 0)
	{
		log_message("Beta(tagged) Database DUMP format recognised.");
	}
	else if (string_compare(format, "Alpha") == 0)
	{

		if (version < 9)
		{
			log_message("Alpha Database DUMP format version %d is no longer supported.", version);
			return (NOTHING);
		}
		else
		{
			log_message("Alpha Database DUMP format version %d is no longer supported. Please use UglyCODE release um1_027 or earlier to convert to Beta(tagged) format.", version);
			return (NOTHING);
		}
	}
	else
	{
		log_message("Unrecognised UglyMug DUMP format. Bailing out.");
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
	while (true)
	{
		switch(ch)
		{
			case OBJECT_SEPARATOR:
				/* This is guaranteed to be a separator IF version > 13 */
				if((ch = get_next_char(f)) != '#')
				{
					log_bug("ERROR: Object not correctly saved - Looking for <\\002>#, got <\\002> 0x%x (%c).", ch, ch);
					return (-1);
				}
				/* another entry, yawn */
				i = getref(f);

				if (error)
				{
					log_message( "... skipped to #%d", i);
					error = false;
				}

				if (i <= last_entry)
				{
					log_bug("Major Problem Dudes - Item #%d is out of order guv", i);
					while((ch = get_next_char (f)) != OBJECT_SEPARATOR);
					error = true;
					continue;
				}

				if (i > last_entry+1)
					set_free_between (last_entry + 1, i - 1);

				/* read it in */
				type = getint(f);

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
						obj->reset_build_id(i);
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
					case TYPE_ARMOUR:
					case TYPE_AMMUNITION:
						log_bug("Weapon, Armour and Ammunition types not supported since um1_031.\n");
						break;
					default:
						log_bug ("Illegal object type in database load.\n");
				}
				array [i].set_obj (*obj);
				Settypeof(i, type);
				array [i].get_obj ()->read (f);
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
					log_bug("Got a * without **END OF DUMP***");
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
			default:
				log_bug("Expected <\\002>, got 0x%x (%c). Last object read was #%d. Skipping to next #...",
					ch,
					ch,
					last_entry);
				while((ch = get_next_char (f)) != OBJECT_SEPARATOR);
				error = true;
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
		log_bug("initialise_load_buffer: MALLOC FAILED");
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
		log_bug("Can't handle field size. Aborting db load.");
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

	return s1->compare(s2);
}


void Database::build_player_cache(int player_count)
{
	int cache_entry=0;

	player_cache = new player_cache_struct[player_count];
	//player_cache = (struct player_cache_struct *) calloc(sizeof(struct player_cache_struct), player_count);

	for(int i=0; i<top; i++)
	{
		if(Typeof(i)==TYPE_PLAYER)
		{
			player_cache[cache_entry].state = CACHE_VALID;
			player_cache[cache_entry].name = db[i].get_name();
			player_cache[cache_entry++].player = i;
#ifdef ALIASES
			for(int alias=0; alias<MAX_ALIASES; alias++)
			{
				String alias_name;

				alias_name = db[i].get_alias(alias);
				if(alias_name)
				{
					player_cache[cache_entry].state = CACHE_VALID;
					player_cache[cache_entry].name = alias_name;
					player_cache[cache_entry++].player = i;
				}
			}
#endif
		}
	}
	qsort(player_cache, player_count, sizeof(struct player_cache_struct), compare_player_cache_entries);
}


const dbref
Database::lookup_player (
const	CString&	name)
const

{
//	struct player_cache_struct *pc, key;
	struct player_cache_struct key;

	if(!name)
		return NOTHING;

	/* Check for #number */

	if(*name.c_str()=='#')
	{
		dbref result=atoi(name.c_str()+1);

		if(Typeof(result)==TYPE_PLAYER)
			return result;
		else
			return NOTHING;
	}
// Quick hack, cos I screwed this up.
	for(dbref i = 0; i < db.get_top(); i++)
	{
		if(Typeof(i)==TYPE_PLAYER)
		{
			if(string_compare(name, db[i].get_name()) == 0)
				return i;
#ifdef ALIASES
			for(dbref j = 0; j < 5; j++)
				if(string_compare(name, db[i].get_alias(j)) == 0)
					return i;
#endif
		}
	}
/*
	for(struct changed_player_list_struct *current=changed_player_list; current; current=current->next)
		if(string_compare(current->name, name)==0)
			return current->player;

	key.state=CACHE_VALID;
	key.name=name.c_str();

	if((pc=(struct player_cache_struct *) bsearch(&key, player_cache, player_count, sizeof(struct player_cache_struct), compare_player_cache_entries)))
	{
		if(pc->state==CACHE_INVALID)
			return NOTHING;

		return pc->player;
	}
*/
	return NOTHING;
}


void
Database::add_player_to_cache (
	dbref	player,
const	CString& name)
{
	if(Typeof(player)!=TYPE_PLAYER)
		return;

	struct changed_player_list_struct *entry=(struct changed_player_list_struct *) malloc(sizeof(struct changed_player_list_struct));

	entry->name=strdup(name.c_str());
	entry->player=player;
	entry->prev=NULL;
	entry->next=changed_player_list;
	if(changed_player_list)
		changed_player_list->prev=entry;
	changed_player_list=entry;
}


void Database::remove_player_from_cache(const CString& name)
{
	for(struct changed_player_list_struct *current=changed_player_list; current; )
		if(string_compare(current->name, name)==0)
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
		if(string_compare(player_cache[i].name, name)==0)
			player_cache[i].state=CACHE_INVALID;
}

int
player_cache_struct::compare(const player_cache_struct* other) const
{
	if(other)
	{
		return string_compare(name, other->name);
	}
	return -1;
}


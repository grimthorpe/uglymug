/* static char SCCSid[] = "@(#)unparse.c	1.19\t9/25/95"; */
#include "db.h"

#include "config.h"
#include "interface.h"
#include "context.h"
#include "externs.h"
#include "colour.h"


/*	Name		Mask		Value		Short form */
struct	flag_stuff	flag_list [] =
{
	{"Abode_OK",	FLAG_ABODE_OK,		'h'},
#ifdef ABORT_FUSES
	{"Abort",	FLAG_ABORT,		'a'},
#endif
	{"Apprentice",	FLAG_APPRENTICE,	'A'},
	{"Ashcan",	FLAG_ASHCAN,		'!'},
#ifdef RESTRICTED_BUILDING
	{"Builder",	FLAG_BUILDER,		'B'},
#endif /* RESTRICTED_BUILDING */
	{"CensorAll",	FLAG_CENSORALL,		'\0'},
	{"Censored",	FLAG_CENSORED,		'*'},
	{"CensorPublic",FLAG_CENSORPUBLIC,	'\0'},
	{"Chown_ok",	FLAG_CHOWN_OK,		'c'},
	{"Colour",	FLAG_COLOUR,		'C'},
	{"Connected",	FLAG_CONNECTED,		'\0'},
	{"Dark",	FLAG_DARK,		'D'},
	{"Debug",	FLAG_DEBUG,		'd'},
	{"Eric",	FLAG_ERIC,		'E'},
	{"Fchat",	FLAG_FCHAT,		'\0'},
	{"Fighting",	FLAG_FIGHTING,		'F'},
	{"Haven",	FLAG_HAVEN,		'H'},
	{"Host",	FLAG_HOST,		'\0'},
	{"Inheritable",	FLAG_INHERITABLE,	'I'},
	{"Jump_OK",	FLAG_JUMP_OK,		'j'},
	{"LineNumbers",	FLAG_LINENUMBERS,	'#'},
	{"Link_OK",	FLAG_LINK_OK,		'l'},
	{"Listen",	FLAG_LISTEN,		'L'},
	{"Locked",	FLAG_LOCKED,		'\0'},
	{"NoHuhLogs",	FLAG_NOHUHLOGS,		'\0'},
	{"Number",	FLAG_NUMBER,		'\0'},
	{"Officer",	FLAG_OFFICER,		'O'},
	{"Open",	FLAG_OPEN,		'\0'},
	{"Openable",	FLAG_OPENABLE,		'\0'},
	{"Opaque",	FLAG_OPAQUE,		'\0'},
	{"Prettylook",	FLAG_PRETTYLOOK,	'\0'},
	{"Public",	FLAG_PUBLIC,		'P'},
	{"Readonly",	FLAG_READONLY,		'r'},
        {"Silent",      FLAG_SILENT,		's'},
	{"Sticky",	FLAG_STICKY,		'\0'},
	{"Tom",		FLAG_TOM,		't'},
	{"Tracing",	FLAG_TRACING,		'\0'},
	{"Visible",	FLAG_VISIBLE,		'v'},
	{"Welcomer",	FLAG_WELCOMER,		'w'},
	{"Wizard",	FLAG_WIZARD,		'W'},
	{"Natter",	FLAG_NATTER,		'\0'},
	{"XBuilder",	FLAG_XBUILDER,		'X'},

	{"Male",	FLAG_MALE,		'\0'},
	{"Female",	FLAG_FEMALE,		'\0'},
	{"Neuter",	FLAG_NEUTER,		'\0'},

	{"Plural",	FLAG_ARTICLE_PLURAL,		'\0'},
	{"Vowel",	FLAG_ARTICLE_SINGULAR_VOWEL,	'\0'},
	{"Consonant",	FLAG_ARTICLE_SINGULAR_CONS,	'\0'},
	{"Referenced",	FLAG_REFERENCED,	'\0'},

	{"Backwards",	FLAG_BACKWARDS,		'B'},

	{"DontAnnounce",FLAG_DONTANNOUNCE,	'-'},
	{NULL,		0,		'\0'}
};


static char *
unparse_flags(
dbref	thing)

{
	static	char	buf [100];
	char		*p;
	static	char	type_codes [] = "?0-PNRTECVpadAFtswzb";
	int		type = Typeof (thing);
	int		i;

	p = buf;
	*p++ = ' ';
	*p = type_codes [type];
	if ((Typeof (thing) == TYPE_PLAYER) && Puppet (thing))
		*p = 'p';

	p++;

	/* Print flags if there are any */
	if (thing == GOD_ID)
		*p++ = 'G';

	for (i = 0; flag_list [i].string != NULL; i++)
		if(((db[thing].get_flag(flag_list[i].flag))
			&& (flag_list [i].quick_flag != '\0'))
			/* If they are god, don't display W flag */
			&& (!((flag_list[i].flag == FLAG_WIZARD) &&
			      (thing == GOD_ID))))
			*p++ = flag_list [i].quick_flag;
	*p = '\0';
	return buf;
}


const char *const
unparse_object(
const	context	&c,
dbref		thing)

{
	static char buf[BUFFER_LEN];

	switch(thing)
	{
		case NOTHING:
			return "*NOTHING*";
		case HOME:
			return "*HOME*";
		default:
			if (Typeof (thing) == TYPE_FREE)
			{
				sprintf (buf, "*FREE:#%d*", (int)thing);
				return buf;
			}
			if(!(db[c.get_player ()].get_flag(FLAG_NUMBER))
				&& (Visible(thing) || c.controls_for_read (thing)
					|| can_link_to (c, thing) || Abode (thing) || Link (thing) || Jump (thing)))
			{
				/* show everything */
				sprintf(buf, "%s%s(#%d%s)", getname(thing), COLOUR_REVERT, (int)thing, unparse_flags(thing));
				return buf;
			}
			else
			{
				/* show only the name */
				return (getname (thing));
			}
	}
}


const char *const
unparse_objectandarticle(
const	context	&c,
const	dbref	thing,
	int 	article_type)

{
	static char buf[BUFFER_LEN];

	switch(thing)
	{
		case NOTHING:
			return "*NOTHING*";
		case HOME:
			return "*HOME*";
		default:
			if(Typeof(thing) == TYPE_FREE)
			{
				return "*FREE*";
			}
			if (!((Typeof (thing) == TYPE_ROOM)
			|| (Typeof (thing) == TYPE_EXIT)
			|| (Typeof (thing) == TYPE_THING)
			|| (Typeof (thing) == TYPE_PLAYER)
			|| (Typeof (thing) == TYPE_PUPPET)))
				article_type= ARTICLE_IRRELEVANT;	/* See comment below */

			/*if(!(db[c.get_player ()].get_flag(FLAG_NUMBER) || Visible(thing))
				&& (c.controls_for_read (thing) 
				|| can_link_to (c, thing)))*/
			if(!(db[c.get_player ()].get_flag(FLAG_NUMBER))
				&& (Visible(thing) || c.controls_for_read (thing)
					|| can_link_to (c, thing) || Abode (thing) || Link (thing) || Jump (thing)))
			{
				/* show everything */
				sprintf(buf, "%s%s%s(#%d%s)", getarticle (thing, article_type), getname (thing), COLOUR_REVERT, (int)thing, unparse_flags (thing));
				return buf;
			}
			else
			{
				/* show only the name */
				sprintf (buf, "%s%s", getarticle (thing, article_type), getname (thing));
				return buf;
			}
	}
}

const char *const
unparse_object_inherited (
const	context	&c,
const	dbref	thing)

{
	static	char	buf [BUFFER_LEN];

	switch(thing)
	{
		case NOTHING:
			return "*NOTHING*";
		case HOME:
			return "*HOME*";
		default:
			if(!(db[c.get_player ()].get_flag(FLAG_NUMBER))
				&& (Visible(thing) || c.controls_for_read (thing)
					|| can_link_to (c, thing) || Abode (thing) || Link (thing) || Jump (thing)))
			/*	if(!(db[c.get_player ()].get_flag(FLAG_NUMBER) || Visible(thing))
					&& (c.controls_for_read (thing)
					|| can_link_to (c, thing)))*/
			{
				/* show everything */
				sprintf(buf, "%s%s(#%d%s)", getname_inherited (thing), COLOUR_REVERT, (int)thing, unparse_flags (thing));
				return buf;
			}
			else
			{
				/* show only the name */
				return (getname_inherited (thing));
			}
	}
}


const char *const
unparse_objectandarticle_inherited (
const	context	&c,
const	dbref	thing,
	int	article_type)

{
	static	char	buf [BUFFER_LEN];

	switch(thing)
	{
		case NOTHING:
			return "*NOTHING*";
		case HOME:
			return "*HOME*";
		default:
			if (!((Typeof (thing) == TYPE_ROOM)
			|| (Typeof (thing) == TYPE_EXIT)
			|| (Typeof (thing) == TYPE_THING)
			|| (Typeof (thing) == TYPE_PLAYER)
			|| (Typeof (thing) == TYPE_PUPPET)))
				article_type = ARTICLE_IRRELEVANT;	 /* Just incase we're trying to put an article on
									   something it shouldn't be on */
			if(!(db[c.get_player ()].get_flag(FLAG_NUMBER))
				&& (Visible(thing) || c.controls_for_read (thing)
					|| can_link_to (c, thing) || Abode (thing) || Link (thing) || Jump (thing)))
			/*if (!(db[c.get_player ()].get_flag(FLAG_NUMBER) || Visible(thing)) && (c.controls_for_read (thing) || can_link_to (c, thing)))*/
			{
				/* show everything */
				sprintf(buf, "%s%s%s(#%d%s)", getarticle (thing, article_type), getname_inherited (thing), COLOUR_REVERT, (int)thing, unparse_flags (thing));
				return buf;
			}
			else
			{
				/* show only the name */
				sprintf (buf, "%s%s", getarticle (thing, article_type), getname_inherited (thing));
				return buf;
			}
	}
}

const char *const
unparse_for_return_inherited (
const	context	&c,
const	dbref	thing)

{
	static	char	buf[15];

	switch(thing)
	{
		case NOTHING:
			return "NOTHING";
		case HOME:
			return "home";
		default:
			if (c.controls_for_read (thing)
				|| can_link_to(c, thing))
			{
				sprintf(buf, "#%d", (int)thing);
				return buf;
			}
			else
				return db[thing].get_inherited_name ().c_str();
	}
}


const char *const
unparse_for_return (
const	context	&c,
const	dbref	thing)

{
	static	char	buf[15];

	switch(thing)
	{
		case NOTHING:
			return "NOTHING";
		case HOME:
			return "home";
		default:
			if (c.controls_for_read (thing)
				|| can_link_to(c, thing))
			{
				sprintf(buf, "#%d", (int)thing);
				return buf;
			}
			else
				return db[thing].get_name ().c_str();
	}
}

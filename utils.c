/* static char SCCSid[] = "@(#)utils.c	1.11\t6/14/95"; */
#include "copyright.h"
#include "db.h"
#include "interface.h"
#include "externs.h"

/*
 * remove_first: Remove the first occurence of what in list headed by first
 */

dbref
remove_first (
dbref	first,
dbref	what)

{
	dbref prev;

	/* special case if it's the first one */
	if(first == what)
		return db[first].get_real_next ();
	else
	{
		/* have to find it */
		DOLIST(prev, first)
		{
			if(db[prev].get_real_next () == what)
			{
				db[prev].set_next (db[what].get_real_next ());
				return first;
			}
		}
		return first;
	}
}


/*
 * in_area: Find if thing is in area. Short-cut this by looking at
 *	the thing's locations up the tree.
 */

Boolean
in_area (
dbref	thing,
dbref	area)

{
	for ( ; thing != NOTHING; thing = db[thing].get_location ())
		if (thing == area)
			return (True);
	return (False);
}


Boolean
contains (
dbref	thing,
dbref	container)
{
	if (thing == HOME)
		thing = db[container].get_destination ();

	for ( ; thing != NOTHING; thing = db[thing].get_location ())
		if (thing == container)
			return (True);
	return (False);
}


Boolean
member (
dbref	thing,
dbref	list)

{
	DOLIST(list, list)
	{
		if (Container(list) || Typeof(list) == TYPE_PLAYER)
		{
			if (member (thing, db [list].get_contents ()))
				return (True);
		}
		if(list == thing)
			return (True);
	}

	return (False);
}


const char * 
getarticle (
dbref   thing,
int	article_base)
{
	const char *articles[] = {	"",	"some ",	"an ",		"a ",
					"",	"the ",		"the ",		"the ",
					"",	"Some ",	"An ",		"A ",
					"",	"The ",		"The ",		"The "	};

	if (article_base == 4)
		article_base = 0;
        if ((article_base < 0) || (article_base > 3))
	{
		Trace("BUG: Invalid article base (%d) in getarticle ()\n", article_base);
		article_base = 0;
	}
	return (articles[ConvertArticle (thing) + 4 * article_base]);
}


const char *
getname (
dbref	loc)

{
	const	char	*name;

	switch(loc)
	{
		case NOTHING:
			return "*NOTHING*";
		case HOME:
			return "*HOME*";
		case AMBIGUOUS:
			return "*AMBIGUOUS*";
		default:
			if((loc < 0) || (loc >= db.get_top ()))	/* Just in case */
				return ("*OUTOFBOUNDS*");
			if(Typeof(loc) == TYPE_FREE)
				return("*FREEOBJECT*");
			name = db[loc].get_name ();
			if (name != NULL)
				return (name);
			else
				return ("*NONAME*");
	}
}


const char *
getname_inherited (
dbref	loc)

{
	const	char	*name;

	switch(loc)
	{
		case NOTHING:
			return "*NOTHING*";
		case HOME:
			return "*HOME*";
		case AMBIGUOUS:
			return "*AMBIGUOUS*";
		default:
			if((loc < 0) || (loc >= db.get_top ()))	/* Just in case */
				return ("*OUTOFBOUNDS*");
			if(Typeof(loc) == TYPE_FREE)
				return("*FREEOBJECT*");
			name = db[loc].get_inherited_name ();
			if (name != NULL)
				return (name);
			else
				return ("*NONAME*");
	}
}

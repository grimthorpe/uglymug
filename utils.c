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

bool
in_area (
dbref	thing,
dbref	area)

{
	for ( ; thing != NOTHING; thing = db[thing].get_location ())
		if (thing == area)
			return (true);
	return (false);
}


bool
contains (
dbref	thing,
dbref	container)
{
	if (thing == HOME)
		thing = db[container].get_destination ();

	for ( ; thing != NOTHING; thing = db[thing].get_location ())
		if (thing == container)
			return (true);
	return (false);
}

bool
contains_inherited (
dbref	thing,
dbref	container)
{
	if (thing == HOME)
	{
		return contains(thing, container);
	}
	else if(thing == NOTHING)
	{
		return false;
	}

	dbref contents;
	for (contents = db[container].get_contents(); contents != NOTHING; contents = db[contents].get_next())
	{
		for(dbref obj = contents; obj != NOTHING; obj = db[obj].get_parent())
		{
			if(obj == thing)
			{
				return true;
			}
		}
	}
	return (false);
}


bool
member (
dbref	thing,
dbref	list)

{
	DOLIST(list, list)
	{
		if (Container(list) || Typeof(list) == TYPE_PLAYER)
		{
			if (member (thing, db [list].get_contents ()))
				return (true);
		}
		if(list == thing)
			return (true);
	}

	return (false);
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
			if(db[loc].get_name())
			{
				return db[loc].get_name().c_str();
			}
			else
				return ("*NONAME*");
	}
}


const char *
getname_inherited (
dbref	loc)

{
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
			if(db[loc].get_inherited_name())
			{
				return db[loc].get_inherited_name().c_str();
			}
			else
				return ("*NONAME*");
	}
}

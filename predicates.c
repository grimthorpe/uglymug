/* static char SCCSid[] = "@(#)predicates.c	1.32\t10/17/95"; */
/*
 * predicates.c: Predicates for testing conditions. This lot are
 *	independent of netmud.
 *
 *	Peter Crowther, 22/7/91.
 */

#include "copyright.h"
#include "db.h"
#include "config.h"
#include "externs.h"
#include "context.h"


/* This defines what flags are allowed to be set on each object type.
   Used as the first test in set and as a check in the sanity-check */

unsigned char flag_map[][64] = 
{
/*Type unassigned yet*/
	{0},

/*NO_TYPE*/
	{0},

/*TYPE_FREE*/
	{0},

/*TYPE_PLAYER*/
	{FLAG_APPRENTICE, FLAG_ASHCAN, FLAG_BUILDER, 
	 FLAG_CENSORED, FLAG_CENSORALL, FLAG_CENSORPUBLIC,
	 FLAG_CHOWN_OK, FLAG_COLOUR, FLAG_CONNECTED, FLAG_FCHAT,
	 FLAG_FEMALE, FLAG_FIGHTING, FLAG_HAVEN, FLAG_DEBUG,
	 FLAG_HOST, FLAG_INHERITABLE, FLAG_LISTEN, FLAG_LINENUMBERS,
	 FLAG_MALE, FLAG_NOHUHLOGS, 
	 FLAG_NEUTER, FLAG_NUMBER, FLAG_REFERENCED, FLAG_SILENT, 
	 FLAG_READONLY, FLAG_TRACING, FLAG_VISIBLE, FLAG_WIZARD,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 
	 FLAG_WELCOMER, FLAG_XBUILDER, FLAG_PRETTYLOOK,
	 FLAG_DONTANNOUNCE, 0},

/*TYPE_PUPPET*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_CONNECTED, FLAG_FEMALE, FLAG_FIGHTING,
	 FLAG_DEBUG,
	 FLAG_INHERITABLE, FLAG_LISTEN, FLAG_MALE, FLAG_NEUTER, FLAG_TRACING,
	 FLAG_REFERENCED, FLAG_READONLY, FLAG_VISIBLE,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_ROOM*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_HAVEN,
	 FLAG_INHERITABLE, FLAG_LOCKED, FLAG_SILENT, FLAG_STICKY,
	 FLAG_REFERENCED, FLAG_READONLY, FLAG_VISIBLE,
	 FLAG_WIZARD, FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ABODE_OK, FLAG_LINK_OK, FLAG_JUMP_OK, FLAG_CENSORED, FLAG_PUBLIC,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_THING*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_HAVEN, FLAG_FIGHTING,
	 FLAG_INHERITABLE, FLAG_LOCKED, FLAG_OPAQUE,
	 FLAG_OPEN, FLAG_OPENABLE, FLAG_STICKY,
	 FLAG_REFERENCED, FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_EXIT*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_INHERITABLE,
	 FLAG_LOCKED, FLAG_OPAQUE, FLAG_REFERENCED, FLAG_READONLY,
	 FLAG_VISIBLE, FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_COMMAND*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_INHERITABLE,
	 FLAG_LOCKED, FLAG_OPAQUE, FLAG_SILENT, FLAG_REFERENCED,
	 FLAG_READONLY, FLAG_TRACING, FLAG_VISIBLE,
	 FLAG_WIZARD, FLAG_BACKWARDS,
	 FLAG_ARTICLE_PLURAL, FLAG_ARTICLE_SINGULAR_VOWEL,
	 FLAG_ARTICLE_SINGULAR_CONS, FLAG_ARTICLE_NOUN, 0},

/*TYPE_VARIABLE*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_OPAQUE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_PROPERTY*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_ARRAY*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_OPAQUE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_DICTIONARY*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_DARK, FLAG_OPAQUE,
	 FLAG_READONLY, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_ALARM*/
	{FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_INHERITABLE, FLAG_LOCKED, 
	 FLAG_OPAQUE, FLAG_READONLY, FLAG_REFERENCED, FLAG_VISIBLE,
	 FLAG_WIZARD, 0},

/*TYPE_FUSE*/
	{
#ifdef ABORT_FUSES
	 FLAG_ABORT,
#endif
	 FLAG_ASHCAN, FLAG_CHOWN_OK, FLAG_INHERITABLE, FLAG_LOCKED, 
	 FLAG_OPAQUE, FLAG_READONLY, FLAG_REFERENCED, FLAG_STICKY,
	 FLAG_TOM, FLAG_VISIBLE, FLAG_WIZARD, 0},

/*TYPE_TASK*/
	{0},
/*TYPE_SEMAPHORE*/
	{0},
/*TYPE_WEAPON*/
	{0},
/*TYPE_ARMOUR*/
	{0},
/*TYPE_AMMUNITION*/
	{0}
};


bool
is_flag_allowed(
typeof_type	type,
flag_type	flag)
{
	int current = 0;
	int count = 0;
	do
	{
		if((current = flag_map[type][count]) == flag)
			return(true);
		count++;
	}
	while(current);
	return(false);
}


const bool
can_link_to (
const	context	&c,
const	dbref	where)

{
    return (where >= 0
	   && where < db.get_top ()
	   && (Typeof(where) == TYPE_ROOM || Container(where))
	   && (c.controls_for_write (where)));
}


const bool
context::controls_for_write (
const	dbref	what)
const

{
	return(what >= 0 && what < db.get_top ()
		&& (!Readonly (what))			/* Nothing set READONLY can be changed */
		&& ((get_effective_id() == GOD_ID)							/* GOD controls everything */
			|| (Wizard(player) && (player == get_effective_id()) && (!Wizard (what) || (Typeof(what) != TYPE_PLAYER)))	/* WIZARDs control everything except other WIZARD players */
			|| (Wizard(get_effective_id()) && !Wizard (db[what].get_owner()))	/* Effective WIZARDs control mortals */
			|| (get_effective_id() == db[what].get_owner())				/* Everyone controls themselves */
			|| (db[get_effective_id()].get_build_id() == db[what].get_owner())			/* Controls for their group */
			|| (get_effective_id() == db [db [what].get_owner()].get_controller ())));	/* Players own their puppets and puppets' stuff */
}


const bool
context::controls_for_read (
const	dbref	what)
const

{
	return (what >= 0
		&& what < db.get_top ()
		&& (Wizard(get_effective_id ())						/* Effective WIZARDS can read anything */
			|| (get_effective_id () == db [what].get_owner())		/* Everyone controls themselves */
			|| (get_effective_id () == db [db [what].get_owner()].get_controller ()) /* Everyone controls themselves */
			|| Wizard(player)						/* Real WIZARDS can read anything */
			|| (player == db [what].get_owner())				/* Everyone controls themselves */
			|| (player == db [db [what].get_owner()].get_controller ())	/* Everyone controls themselves */
			|| (db[player].get_build_id() == db[what].get_owner())		/* Controls for their group */
			|| (Visible(what))						/* Everyone can see visible things */
			|| (Apprentice(get_effective_id()))				/* Effective APPRENTICES can read everything */
			|| (Apprentice(player))));					/* APPRENTICES can read everything */
}


const bool
controls_for_read (
const	dbref	/* real_who */,
const	dbref	what,
const	dbref	effective_who)

{
	return (what >= 0
		&& what < db.get_top ()
		&& (Wizard(effective_who)				/* Real and Effective WIZARDS can read anything */
			|| (effective_who == db [what].get_owner())	/* Everyone controls themselves */
			|| (effective_who == db [db [what].get_owner()].get_controller ()) /* Everyone controls themselves */
			|| (Visible(what))				/* Everyone can see visible things */
			|| (Apprentice(effective_who))));		/* Real and Effective APPRENTICES can read everything */
}


const bool
can_link (
const	context	&c,
const	dbref	what)

{
	return((Typeof(what) == TYPE_EXIT && db[what].get_destination() == NOTHING)
		|| c.controls_for_write (what) || Visible(what)
		|| (Apprentice(c.get_player ()) && ((Typeof(what) != TYPE_PLAYER) || !Wizard(what))));
}

/* static char SCCSid[] = "@(#)extract.c	1.3\t7/19/94"; */
#ifdef DEBUG
#include "mnemosyne.h"
#endif
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "db.h"

/* include everything unless specified */
int include_all = 0;

dbref included[NCARGS+1];
dbref excluded[NCARGS+1];

/* translation vector */
dbref *trans;

dbref	db_free_chain,	db_top;

#define DEFAULT_LOCATION (0)
#define DEFAULT_OWNER (1)

/* returns 1 if it should be included in translation vector */
int isok(dbref x)
{
	int i;

	if(x == DEFAULT_OWNER || x == DEFAULT_LOCATION) return 1;

	for(i = 0; included[i] != NOTHING; i++)
	{
		if(included[i] == x)
			return 1; /* always get specific ones */
		if(included[i] == db[x].owner)
			return not_excluded(x);
	}

	/* not in the list, can only get it if include_all is on */
	/* or it's owned by DEFAULT_OWNER */

	return ((include_all || db[x].owner == DEFAULT_OWNER) && not_excluded(x));
}

/* returns 1 if it is not excluded */
int not_excluded(dbref x)
{
	int i;

	/* check that it isn't excluded */

	for(i = 0; excluded[i] != NOTHING; i++)
		if(excluded[i] == x)
			return 0; /* always exclude specifics */

	/* if it's an exit, check that its destination is ok */

	if(Typeof(x) == TYPE_EXIT)
		return isok(db[x].destination);
	else
		return 1;
}

build_trans()
{
	dbref i;
	dbref val = 0;

	if((trans = (dbref *) malloc(sizeof(dbref) * db_top)) == 0)
		abort();

	for(i = 0; i < db_top; i++)
		if(isok(i))
			trans[i] = val++;
		else
			trans[i] = NOTHING;
}

dbref translate(dbref x)
{
    if(x == NOTHING || x == HOME) {
	return(x);
    } else {
	return(trans[x]);
    }
}

/* TRUE_BOOLEXP means throw this argument out */
/* even on OR; it's effectively a null boolexp */
/* NOTE: this doesn't free anything, it just munges it up */
static struct boolexp *translate_boolexp(exp)
struct boolexp *exp;
{
    struct boolexp *s1;
    struct boolexp *s2;

    if(exp == TRUE_BOOLEXP) {
	return TRUE_BOOLEXP;
    } else {
	switch(exp->type) {
	  case BOOLEXP_NOT:
	    s1 = translate_boolexp(exp->sub1);
	    if(s1 == TRUE_BOOLEXP) {
		return TRUE_BOOLEXP;
	    } else {
		exp->sub1 = s1;
		return exp;
	    }
	    /* break; */
	  case BOOLEXP_AND:
	  case BOOLEXP_OR:
	    s1 = translate_boolexp(exp->sub1);
	    s2 = translate_boolexp(exp->sub2);
	    if(s1 == TRUE_BOOLEXP && s2 == TRUE_BOOLEXP) {
		/* nothing left */
		return TRUE_BOOLEXP;
	    } else if(s1 == TRUE_BOOLEXP && s2 != TRUE_BOOLEXP) {
		/* s2 is all that is left */
		return s2;
	    } else if(s1 != TRUE_BOOLEXP && s2 == TRUE_BOOLEXP) {
		/* s1 is all that is left */
		return s1;
	    } else {
		exp->sub1 = s1;
		exp->sub2 = s2;
		return exp;
	    }
	    /* break; */
	  case BOOLEXP_CONST:
	    exp->thing = translate(exp->thing);
	    if(exp->thing == NOTHING) {
		return TRUE_BOOLEXP;
	    } else {
		return exp;
	    }
		case BOOLEXP_FLAG:
			return exp;
	    /* break; */
	  default:
	    abort();		/* bad boolexp type, we lose */
	    return TRUE_BOOLEXP;
	}
    }
}

int ok(dbref x)
{
	if(x == NOTHING || x == HOME)
		return 1;
	else
		return trans[x] != NOTHING;
}

void check_controller(dbref x)
{
	if(Typeof(x) == TYPE_PLAYER &&
		ok(x) && !ok(db[x].extra_data->player_data.controller))
		db[x].extra_data->player_data.controller = NOTHING;
}

void check_owner(dbref x)
{
	if(ok(x) && !ok(db[x].owner))
		db[x].owner = DEFAULT_OWNER;
}

void check_destination(dbref x)
{
	dbref dest;

	if(ok(x) && !ok(dest = db[x].destination))
		switch(Typeof(x))
		{
			case TYPE_THING:
			case TYPE_PLAYER:
				if(ok(db[db[x].owner].destination))
					/* set it to owner's home */
					db[x].destination = db[db[x].owner].destination; /* home */
				else
					/* set it to DEFAULT_LOCATION */
					db[x].destination = DEFAULT_LOCATION; /* home */
				db[x].destination = DEFAULT_LOCATION;
				break;
			case TYPE_EXIT:
				trans[x] = NOTHING;
				/* FALL THROUGH */
			case TYPE_ROOM:
			case TYPE_COMMAND:
			case TYPE_VARIABLE:
			case TYPE_ALARM:
			case TYPE_FUSE:
				db[x].destination = NOTHING;
				break;
		}
}

void check_location(dbref x)
{
	dbref loc;
	dbref newloc;

	if(Typeof(x) != TYPE_ROOM && ok(x) && !ok(loc = db[x].location))
		switch(Typeof(x))
		{
			case TYPE_THING:
			case TYPE_PLAYER:
				/* move it to home or DEFAULT_LOCATION */

				if(ok(db[x].destination))
					newloc = db[x].destination; /* home */
				else
					newloc = DEFAULT_LOCATION;

				db[loc].contents = remove_first(db[loc].contents, x);
				PUSH(x, db[newloc].contents);
				db[x].location = newloc;
				db[x].destination = newloc;
				break;
			case TYPE_EXIT:
			case TYPE_COMMAND:
			case TYPE_VARIABLE:
			case TYPE_ALARM:
			case TYPE_FUSE:
				trans[x] = NOTHING;
				break;
		}
}

void check_next(dbref x)
{
	dbref next;

	if(ok(x) && !ok(next = db[x].next))
		while(!ok(next = db[x].next))
			db[x].next = db[next].next;
}

void check_contents(dbref x)
{
	dbref c;

	if(ok(x) && !ok(c = db[x].contents))
		while(!ok(c = db[x].contents))
			db[x].contents = db[c].next;
}

void check_exits(dbref x)
{
	dbref e;

	if(ok(x) && !ok(e = db[x].exits))
		while(!ok(e = db[x].exits))
			db[x].exits = db[e].next;
}

void check_commands(dbref x)
{
	dbref e;

	if(ok(x) && !ok(e = db[x].commands))
		while(!ok(e = db[x].commands))
			db[x].commands = db[e].next;
}

void check_variables(dbref x)
{
	dbref e;

	if(ok(x) && !ok(e = db[x].variables))
		while(!ok(e = db[x].variables))
			db[x].variables = db[e].next;
}

void check_fuses(dbref x)
{
	dbref e;

	if(ok(x) && !ok(e = db[x].fuses))
		while(!ok(e = db[x].fuses))
			db[x].fuses = db[e].next;
}

char *desc_translate(char *description)
{
	char dbref[10];
	char *p;
	
	p = description;
	while (*p)
	{
	}
}

void do_write(void)
{
	dbref i;
	dbref kludge;
	dbref	db_tmp;

	db_free_chain = -1;

    /* this is braindamaged */
    /* we have to rebuild the translation map */
    /* because part of it may have gotten nuked in check_bad_exits */
	for(i = 0, kludge = 0; i < db_top; i++)
		if(trans[i] != NOTHING)
			trans[i] = kludge++;

	db_tmp = db_top;
	db_top = kludge;

	db_write_header(stdout);

	for(i = 0; i < db_tmp; i++)
		if(ok(i))
		{
			/* translate all object pointers */
			db[i].description = desc_translate(db[i].description);
			db[i].location = translate(db[i].location);
			db[i].destination = translate(db[i].destination);
			db[i].contents = translate(db[i].contents);
			db[i].exits = translate(db[i].exits);
			db[i].next = translate(db[i].next);
			db[i].key = translate_boolexp(db[i].key);
			db[i].owner = translate(db[i].owner);
			db[i].commands = translate(db[i].commands);
			db[i].variables = translate(db[i].variables);
			db[i].fuses = translate(db[i].fuses);
			if (Typeof(i) == TYPE_PLAYER)
				db[i].extra_data->player_data.controller =
					translate(db[i].extra_data->player_data.controller);

			/* write it out */
			printf("#%d\n", translate(i));
			db_write_object(stdout, i);
		}
	puts("***END OF DUMP***");
}

int main(int argc, char **argv)
{
	dbref i;
	int top_in = 0;
	int top_ex = 0;
	char *arg0;

	/* now parse args */
	arg0 = *argv;
	for(argv++, argc--; argc > 0; argv++, argc--)
	{
		i = atol(*argv);
		if(i == 0)
			if(!strcmp(*argv, "all"))
				include_all = 1;
			 else
				fprintf(stderr, "%s: bogus argument %s\n", arg0, *argv);
		else
		if(i < 0)
			excluded[top_ex++] = -i;
		else
			 included[top_in++] = i;
	}

	/* Load database */
	if(db_read(stdin) < 0)
	{
		fputs("Database load failed!\n", stderr);
		exit(1);
	} 
	else
		fputs("Done loading database...\n", stderr);

	/* Exclude free objects */
	for(i = 0; i < db_top; i++)
		if (Typeof(i) == TYPE_FREE || Typeof(i) == NOTYPE)
			excluded[top_ex++] = i;

	/* Terminate */
	included[top_in++] = NOTHING;
	excluded[top_ex++] = NOTHING;

	/* Build translation table */
	build_trans();
	fputs("Done building translation table...\n", stderr);

	/* Scan everything */
	for(i = 0; i < db_top; i++) check_owner(i);
	fputs("Done checking owners...\n", stderr);

	for(i = 0; i < db_top; i++) check_location(i);
	fputs("Done checking locations...\n", stderr);

	for(i = 0; i < db_top; i++) check_destination(i);
	fputs("Done checking destinations...\n", stderr);

	for(i = 0; i < db_top; i++) check_contents(i);
	fputs("Done checking contents...\n", stderr);

	for(i = 0; i < db_top; i++) check_exits(i);
	fputs("Done checking exits...\n", stderr);

	for(i = 0; i < db_top; i++) check_next(i);
	fputs("Done checking next pointers...\n", stderr);

	for(i = 0; i < db_top; i++) check_commands(i);
	fputs("Done checking commands...\n", stderr);

	for(i = 0; i < db_top; i++) check_variables(i);
	fputs("Done checking variables...\n", stderr);

	for(i = 0; i < db_top; i++) check_fuses(i);
	fputs("Done checking fuses...\n", stderr);

	for(i = 0; i < db_top; i++) check_controller(i);
	fputs("Done checking controllers...\n", stderr);

	do_write();
	fputs("Done.\n", stderr);

	exit(0);
}

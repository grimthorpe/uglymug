/* static char SCCSid[] = "@(#)colouring.c	1.6\t7/31/95"; */
/*
 * colouring.c: Make coloured clusters of rooms that can be reached from
 *	each other.
 *
 *	Peter Crowther, University of Manchester, 7/4/92.
 *
 * This code blasts a lot of fields in the database, so you don't
 *	ever want to write it back out afterwards.
 *
 * Storage:
 *	next: Next room/thing on our same-colour list.
 *	contents: Our colour (== pointer to head of our same-colour list).
 */

#include "copyright.h"

#include "externs.h"
#include "db.h"
#include "config.h"
#include "interface.h"
#include "context.h"
#include "objects.h"


/*
 * change_colour: Paint shabby_room and everything its colour to become
 *	the colour of decorated_room.
 */

void
change_colour (
dbref	shabby_room,
dbref	decorated_room)

{
	dbref	chaser;

	/* Trap silly exit destinations */
	if ((shabby_room < 0) || (shabby_room >= db.get_top()) || ((shabby_room = db [shabby_room].get_contents()) == decorated_room))
		return;

	/* Skip to end, painting as we go */
	for (chaser = shabby_room; db [chaser].get_next() != NOTHING; chaser = db[chaser].get_next())
		db [chaser].set_contents(decorated_room);
	db [chaser].set_contents(decorated_room);

	/* Link the newly decorated rooms into the (head of the) decorated room's list */
	db [chaser].set_next(db [decorated_room].get_next());
	db [decorated_room].set_next(shabby_room);
}


/*
 * compute_clusters: Clear out the fields we use, then skip through changing
 *	the colour of everything each room can reach.
 */

void
compute_clusters ()

{
	dbref	room;
	dbref	chaser;

	/* clear out all fields we use */
	for(room = 0; room < db.get_top(); room++)
		if ((Typeof(room) == TYPE_ROOM) || (Typeof (room) == TYPE_THING))
		{
			db [room].set_contents(room);
			db [room].set_next(NOTHING);
		}

	for(room = 0; room < db.get_top(); room++)
		if ((Typeof(room) == TYPE_ROOM) || (Typeof (room) == TYPE_THING))
		{
			/* Everything we can reach goes our colour */
			if ((Typeof (room) == TYPE_ROOM) && (db [room].get_location() != NOTHING))
				change_colour (room, db [db [room].get_location()].get_contents());
			DOLIST (chaser, db [room].get_exits())
				change_colour (db [chaser].get_destination(), db [room].get_contents());
		}
}


/*
 * print_clusters: Find the head of each cluster (the one where its
 *	colour is the same as its ID) and chase down the list of objects
 *	in that cluster.
 *
 * Rooms in the start cluster are ignored, to save on printout.
 */

void
print_clusters (
dbref	start)

{
	int	days;
	dbref	i;
	dbref	chaser;
	char	temp_buffer[BUFFER_LEN];
	time_t	now;

	time(&now);

	for(i = 0; i < db.get_top(); i++)
		if (((Typeof (i) == TYPE_ROOM) || ((Typeof (i) == TYPE_THING) && (Container (i)) && (db [i].get_next() != NOTHING))) && (i == db [i].get_contents()))
//		if (((Typeof (i) == TYPE_ROOM) || ((Typeof (i) == TYPE_THING) && (Container (i)) && (db [i].get_next() != NOTHING))) && (i == db [i].get_contents()) && (i != db [start].get_contents()))
			{
				puts ("CLUSTER_START");
				DOLIST (chaser, i)
				{
					if(db[chaser].get_last_entry_time() == 0)
						days = 9999;
					else
						days = (now - db[chaser].get_last_entry_time()) / 86400;

					sprintf(temp_buffer, "%s", unparse_object (GOD_ID, chaser));
					sprintf(temp_buffer, "%s [%s]", temp_buffer, unparse_object (GOD_ID, db[chaser].get_owner()));
					if(days == 9999)
						sprintf(temp_buffer, "%s (Unknown)", temp_buffer);
					else
						sprintf(temp_buffer, "%s (%d day%s)", temp_buffer, days, PLURAL(days));

					puts (temp_buffer);
				}
				puts ("CLUSTER_END\n");
			}
}


int
main (
int	argc,
char	**argv)

{
	dbref start = 0;

	if(argc >= 2)
		start = atol(argv[2]);
	else
		start = PLAYER_START;

	if(db.read(stdin) < 0)
	{
		fprintf (stderr, "%s: bad input\n", argv[0]);
		exit(5);
	}

	compute_clusters ();
	print_clusters (start);

	exit(0);
}

static char SCCSid[] = "@(#)stats.c	1.4\t7/19/94";
#include "copyright.h"

#include <stdio.h>
#include <strings.h>
#include <malloc.h>

#include "db.h"
#include "config.h"
#include "interface.h"


struct	aux
{
	int	alarms;
	int	commands;
	int	exits;
	int	fuses;
	int	players;
	int	rooms;
	int	things;
	int	variables;
	int	properties;
	int	arrays;
	int	dictionaries;
};


int
main (
int	argc,
char	**argv)

{
	dbref		i;
	struct	aux	*aux;

	if(db_read(stdin) < 0)
	{
		fprintf (stderr, "%s: bad input\n", argv[0]);
		exit(5);
	}

	if ((aux = (struct aux *) calloc (db_top, sizeof (struct aux))) == (struct aux * )NULL)
		exit (1);

	for (i = 0; i < db_top; i++)
		if (db + i != NULL)
			switch (Typeof (i))
			{
				case TYPE_ALARM:
					aux [db [i].owner].alarms++;
					break;
				case TYPE_COMMAND:
					aux [db [i].owner].commands++;
					break;
				case TYPE_EXIT:
					aux [db [i].owner].exits++;
					break;
				case TYPE_FUSE:
					aux [db [i].owner].fuses++;
					break;
				case TYPE_PLAYER:
					aux [Controller (i)].players++;
					break;
				case TYPE_ROOM:
					aux [db [i].owner].rooms++;
					break;
				case TYPE_THING:
					aux [db [i].owner].things++;
					break;
				case TYPE_VARIABLE:
					aux [db [i].owner].variables++;
					break;
				case TYPE_ARRAY:
					aux [db [i].owner].arrays++;
					break;
				case TYPE_DICTIONARY:
					aux [db [i].owner].dictionaries++;
					break;
				case TYPE_PROPERTIES:
					aux [db [i].owner].properties++;
					break;
			}
	for (i = 0; i < db_top; i++)
		if (Typeof (i) == TYPE_PLAYER)
			printf ("%s|%c|%d|%d|%d|%d|%d|%d|%d|%d\n",
				getname (i),
				Builder (i) ? 'B' : 'N',
				aux [i].alarms,
				aux [i].commands,
				aux [i].exits,
				aux [i].fuses,
				aux [i].players,
				aux [i].rooms,
				aux [i].things,
				aux [i].variables,
				aux [i].properties,
				aux [i].arrays,
				aux [i].dictionaries);

	free (aux);

	exit(0);
}

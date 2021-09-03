/* static char SCCSid[] = "@(#)stats.c	1.4\t7/19/94"; */
#include "copyright.h"

#include <iostream>

#include "debug.h"
#include "db.h"
#include "config.h"
#include "interface.h"


struct	Aux
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
	struct	Aux	*aux;

	if(db.read(stdin) < 0)
	{
		Trace( "%s: bad input\n", argv[0]);
		return 5;
	}

	if (!(aux = new Aux [db.get_top ()]))
		return 1;

	for (i = 0; i < db.get_top (); i++)
		if (db + i != NULL)
			switch (Typeof (i))
			{
				case TYPE_ALARM:
					aux [db [i].owner ()].alarms++;
					break;
				case TYPE_COMMAND:
					aux [db [i].owner ()].commands++;
					break;
				case TYPE_EXIT:
					aux [db [i].owner ()].exits++;
					break;
				case TYPE_FUSE:
					aux [db [i].owner ()].fuses++;
					break;
				case TYPE_PLAYER:
					aux [db[i].controller ()].players++;
					break;
				case TYPE_ROOM:
					aux [db [i].owner ()].rooms++;
					break;
				case TYPE_THING:
					aux [db [i].owner ()].things++;
					break;
				case TYPE_VARIABLE:
					aux [db [i].owner ()].variables++;
					break;
				case TYPE_ARRAY:
					aux [db [i].owner ()].arrays++;
					break;
				case TYPE_DICTIONARY:
					aux [db [i].owner ()].dictionaries++;
					break;
				case TYPE_PROPERTY:
					aux [db [i].owner ()].properties++;
					break;
			}
	for (i = 0; i < db.get_top (); i++)
		if (Typeof (i) == TYPE_PLAYER)
			std::cout << db [i].name () << "|"
				<< (Builder (i) ? 'B' : 'N') << "|"
				<< aux [i].alarms << "|"
				<< aux [i].commands << "|"
				<< aux [i].exits << "|"
				<< aux [i].fuses << "|"
				<< aux [i].players << "|"
				<< aux [i].rooms << "|"
				<< aux [i].things << "|"
				<< aux [i].variables << "|"
				<< aux [i].properties << "|"
				<< aux [i].arrays << "|"
				<< aux [i].dictionaries << std::endl;

	delete [] aux;
	return 0;
}

/* static char SCCSid[] = "@(#)log.c	1.5\t7/19/94"; */
#include <stdio.h>
#include "externs.h"
#include "command.h"
#include "context.h"

/*
 *	log.c - Output all system logs
 *	Written by Keith Alexander Garrett
 *	and        Adrian J. StJohn
 */


static char *log_time(void)
{
	static	char	timestr[100];

	time_t		now;
	time(&now);
	strcpy(timestr, ctime(&now));
	*strchr(timestr, '\n') = '\0' ;
	return timestr;
}


void
context::log_recursion (
dbref		command,
const	char	*arg1,
const	char	*arg2)

{
#ifdef LOG_RECURSION
	Trace( "RECURSION: %s(%d) [%s] Limit: %d Command: %s(%d) %s = %s\n",
		getname_inherited (player), player,
		log_time(),
		step_limit,
		getname_inherited (command), command, value_or_empty (arg1), value_or_empty (arg2));
#endif
	set_return_string (recursion_return_string);
	return_status = COMMAND_FAIL;
}

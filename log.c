/* static char SCCSid[] = "@(#)log.c	1.5\t7/19/94"; */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "externs.h"
#include "command.h"
#include "context.h"
#include "config.h"
#include "log.h"
#include "copyright.h"

/*
 *	log.c - Output all system logs
 *	Written by Keith Alexander Garrett
 *	and        Adrian J. StJohn
 *
 *  NEW_LOGGING code
 *  Written by Chisel C. Wright, December 2001
 */


/*
 * If you fancy viewing the logs with | as a separator
 * rather than the 'invisible' ^W characters, try this:
 *
 * bash# perl -n -e 'print join("|", split(/\027/))' < cutfile
 *
 * - or -
 *
 * bash# perl -n -e 'print join("|", split(/\027/))' < cutfile
 *
 * (You'll need to change the 027 to match the (octal) value
 *  in the FIELD_SEPARATOR definition)
 */

#define RECORD_START	'\025'
#define RECORD_END		'\026'
#define FIELD_SEPARATOR '\027'

void log_accept (
	int					descriptor,
	int					port,
	const String&		ip
) {
	/*
	 * Trace( "ACCEPT from |%s|%d| on descriptor |%d\n", convert_addr (&(addr.sin_addr)), ntohs (addr.sin_port), newsock);
	 * e.g.
	 *     ACCEPT from |127.0.0.1|41793| on descriptor |4
	 */

	Trace(	"%cACCEPT%c%d%c%d%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			descriptor,							FIELD_SEPARATOR,
			port,								FIELD_SEPARATOR,
			ip.c_str(),							RECORD_END
	);
}


void log_boot (
			dbref		targetid,
	const	String&	targetname,
			dbref		booterid,
	const	String&	bootername,
	const	String&	message
) {
	/* e.g. BOOT|timestamp|targetid|targetname|booterid|bootername|message */
	Trace(	"%cBOOT%c%d%c%d%c%s%c%d%c%s%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			targetid,							FIELD_SEPARATOR,
			targetname.c_str(),					FIELD_SEPARATOR,
			booterid,							FIELD_SEPARATOR,
			bootername.c_str(),					FIELD_SEPARATOR,
			message.c_str(),					RECORD_END
	 );
}

void log_bug (
	const	char		*fmt,
						...
) {
	/* turn the variable args into a string */
	char	message[BUFFER_LEN];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(message, sizeof(message), fmt, vl);
	va_end(vl);
	
	/*
	 * e.g.
	 *     BUG: Attempt to remove element from list that didn't contain it.
	 */

	Trace(	"%cBUG%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			message,							RECORD_END
	);
}

void log_checkpointing (
	const String&		dumpfile,
	int					epoch
) {
	/*
	 * Trace( "CHECKPOINTING: %s.#%d#\n", dumpfile, epoch);
	 * e.g.
	 *     CHECKPOINTING: ugly.db.new.#1#
	 */

	Trace(	"%cCHECKPOINTING%c%d%c%s.#%d#%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			dumpfile.c_str(),
			epoch,								RECORD_END
	);
}

void log_command (
			dbref			playerid,
	const	String&		playername,
			dbref			effectiveid,
	const	String&		effectivename,
			dbref			locationid,
	const	String&		locationname,
			//bool			compound,
	const	String&		original_command
) {
	/*
	 * e.g. COMMAND|timestamp|playerid|playername|effectiveid|effectivename|locationid|locationname|command
	 */

	Trace(	"%cCOMMAND%c%d%c%d%c%s%c%d%c%s%c%d%c%s%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			playerid,							FIELD_SEPARATOR,
			playername.c_str(),					FIELD_SEPARATOR,
			effectiveid,						FIELD_SEPARATOR,
			effectivename.c_str(),				FIELD_SEPARATOR,
			locationid,							FIELD_SEPARATOR,
			locationname.c_str(),				FIELD_SEPARATOR,
			original_command.c_str(),			RECORD_END
	);
}

void log_connect (
			bool		success,
			int			descriptor,
			dbref		playerid,
	const	String&	playername
) {
	/*
	 * e.g. CONNECT|timestamp|(SUCCESS|FAILURE)|descriptor|playerid|playername
	 */

	Trace(	"%cCONNECT%c%d%c%s%c%d%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			(success ? "SUCCESS" : "FAILURE"),	FIELD_SEPARATOR,
			descriptor,							FIELD_SEPARATOR,
			playerid,							FIELD_SEPARATOR,
			playername.c_str(),					RECORD_END
	);
}

void log_created (
			int			descriptor,
			dbref		player,
	const	String&		playername
) {
	/*
	 *	CREATED|timestamp|descriptor|playerid|playername
	 */

	Trace(	"%cCREATED%c%d%c%d%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			descriptor,							FIELD_SEPARATOR,
			player,								FIELD_SEPARATOR,
			playername.c_str(),					RECORD_END
	);
}


void log_credit (
			dbref		giver,
	const	String&		givername,
			dbref		taker,
	const	String&		takername,
			int			amount,
	const	char*		currency_name
) {
    /*
     * e.g. CREDIT|timestamp|giver|taker|amount|currency
     */

    Trace(	"%cCREDIT%c%d%c%d%c%s%c%d%c%s%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			giver,								FIELD_SEPARATOR,
			givername.c_str(),					FIELD_SEPARATOR,
			taker,								FIELD_SEPARATOR,
			takername.c_str(),					FIELD_SEPARATOR,
			amount,								FIELD_SEPARATOR,
			currency_name,						RECORD_END
	);
}

void log_debug (
	const	char		*fmt,
						...
) {
	/* turn the variable args into a string */
	char	message[BUFFER_LEN];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(message, sizeof(message), fmt, vl);
	va_end(vl);
	
	/*
	 * e.g.
	 *     DEBUG: flurgleburger needs cheese
	 */

	Trace(	"%cDEBUG%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			message,							RECORD_END
	);
}

void log_disconnect (
			dbref		playerid,
	const	String&	playername,
			int			descriptor,
			int			channel,
	const	String&	message,
			bool		has_connected
) {
	/*
	 *  Old Style:
	 *    DISCONNECT descriptor |5|232| player |chisel|9| at |101/07/31 13:31
	 *    DISCONNECT descriptor |5| never connected
	 *
	 *  New Style:
	 *    DISCONNECT|timestamp|playerid|playername|descriptor|channel|message
	 *
	 *  for 'Never Connected' we don't fill playerid, playername, channel
	 *  and we put 'never connected' as the message
	 *  we don't (at the moment) use the message fields for normal QUIT's etc.
	 *  [ possibly store 'quit', 'booted', etc ]
	 */

	if (has_connected) {
		/* player has connected to the game, and quit (by whatever means) */
		Trace(	"%cDISCONNECT%c%d%c%d%c%s%c%d%c%d%c%s%c\n",
				RECORD_START,							FIELD_SEPARATOR,
				time(NULL),								FIELD_SEPARATOR,
				playerid,								FIELD_SEPARATOR,
				playername.c_str(),						FIELD_SEPARATOR,
				descriptor,								FIELD_SEPARATOR,
				channel,								FIELD_SEPARATOR,
				message.c_str(),						RECORD_END
		);
	}
	else {
		/* 'player' connected to the game, but never made it past the login */
		Trace(	"%cDISCONNECT%c%d%c%c%c%d%c%cnever connected%c\n",
				RECORD_START,							FIELD_SEPARATOR,
				time(NULL),								FIELD_SEPARATOR,
				/* playerid, */							FIELD_SEPARATOR,
				/* playername, */						FIELD_SEPARATOR,
				descriptor,								FIELD_SEPARATOR,
				/* channel, */							FIELD_SEPARATOR,
				/* message - fixed */					RECORD_END
		);
	}
}

void log_dumping (
				bool		finished,
	const		char *		fmt,
							...
) {
	/* turn the variable args into a string */
	char	file[BUFFER_LEN];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(file, sizeof(file), fmt, vl);
	va_end(vl);
	
	/*
	 * DUMPING|timestamp|filename
	 */

	Trace(	"%cDUMP%c%d%c%s%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				file,							FIELD_SEPARATOR,
				finished ? "DONE" : "START",	RECORD_END
	);
}

void log_gripe (
			dbref		playerid,
	const String& playername,
			dbref		locationid,
	const String& locationname,
	const String& message
) {
	/*
	 * GRIPE|timestamp|playerid|playername|locationid|locationname|message
	 */

	Trace(	"%cGRIPE%c%d%c%d%c%s%c%d%c%s%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				playerid,						FIELD_SEPARATOR,
				playername.c_str(),		FIELD_SEPARATOR,
				locationid,						FIELD_SEPARATOR,
				locationname.c_str(),	FIELD_SEPARATOR,
				message.c_str(),		RECORD_END
	);
}


void log_hack (
	dbref			hacked,
	const	char		*fmt,
						...
) {
	/* turn the variable args into a string */
	char	message[BUFFER_LEN];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(message, sizeof(message), fmt, vl);
	va_end(vl);
	
	/*
	 * HACK|timestamp|target|message
	 */

	Trace(	"%cHACK%c%d%c%d%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				hacked,						FIELD_SEPARATOR,
				message,						RECORD_END
	);
}

void log_halfquit (
			dbref			playerid,
	const	String&		playername,
			int				descriptor,
			int				channel
) {
	Trace(	"%cHALFQUIT%c%d%c%d%c%s%c%d%c%d%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				playerid,						FIELD_SEPARATOR,
				playername.c_str(),				FIELD_SEPARATOR,
				descriptor,						FIELD_SEPARATOR,
				channel,						RECORD_END
	);
}

void log_huh (
			dbref		playerid,
	const	String&	playername,
			dbref		locationid,
	const	String&	locationname,
			dbref		locationownerid,
			dbref		npcownerid,
	const	String&	command,
	const	String&	message
) {
	/*
	 * HUH|timestamp|playerid|playername|locationid|locationname|locationownerid|npcownerid|command|message
	 */
	Trace(	"%cHUH%c%d%c%d%c%s%c%d%c%s%c%d%c%d%c%s%c%s%c\n",
				RECORD_START,							FIELD_SEPARATOR,
				time(NULL),								FIELD_SEPARATOR,
				playerid,								FIELD_SEPARATOR,
				playername.c_str(),						FIELD_SEPARATOR,
				locationid,								FIELD_SEPARATOR,
				locationname.c_str(),					FIELD_SEPARATOR,
				locationownerid,						FIELD_SEPARATOR,
				npcownerid,								FIELD_SEPARATOR,
				command.c_str(),						FIELD_SEPARATOR,
				message.c_str(),						RECORD_END
	);
}

void log_loading (
	const	String&		filename,
			bool			finished
) {	
	/*
	 * LOADING|timestamp|filename
	 */

	Trace(	"%cLOADING%c%d%c%s%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				filename.c_str(),						FIELD_SEPARATOR,
				finished ? "DONE" : "START",	RECORD_END
	);
}

void log_message (
	const	char		*fmt,
						...
) {
	/* turn the variable args into a string */
	char	message[BUFFER_LEN];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(message, sizeof(message), fmt, vl);
	va_end(vl);
	
	/*
	 * e.g.
	 *     BUG: Attempt to remove element from list that didn't contain it.
	 */

	Trace(	"%cGENERAL%c%d%c%s%c\n",
			RECORD_START,						FIELD_SEPARATOR,
			time(NULL),							FIELD_SEPARATOR,
			message,							RECORD_END
	);
}

void log_note (
			dbref		playerid,
	const	String&	playername,
			dbref		locationid,
	const	String&	locationname,
			dbref		npcownerid,
			bool		showroomid,
	const	String&	message
) {
	/*
	 * NOTE|timestamp|playerid|playername|locationid|locationname|npcownerid|showroomid|message
	 */

	Trace(	"%cNOTE%c%d%c%d%c%s%c%d%c%s%c%d%c%s%c%s%c\n",
				RECORD_START,								FIELD_SEPARATOR,
				time(NULL),									FIELD_SEPARATOR,
				playerid,									FIELD_SEPARATOR,
				playername.c_str(),							FIELD_SEPARATOR,
				locationid,									FIELD_SEPARATOR,
				locationname.c_str(),						FIELD_SEPARATOR,
				npcownerid,									FIELD_SEPARATOR,
				showroomid ? "showid" : "hideid",			FIELD_SEPARATOR,
				message.c_str(),							RECORD_END
	);
}

void log_panic (
	const char *fmt,
				...
) {
	/* turn the variable args into a string */
	char	message[BUFFER_LEN];
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(message, sizeof(message), fmt, vl);
	va_end(vl);
	/*
	 * PANIC|timestamp|message
	 */

	Trace(	"%cPANIC%c%d%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				message,						RECORD_END
	);
}


void log_reconnect (
			dbref			playerid,
	const	String&		playername,
			int				descriptor
) {
	/*
	 * RECONNECT|timestamp|playerid|playername|descriptor
	 */
	Trace(	"%cRECONNECT%c%d%c%d%c%s%c%d%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				playerid,						FIELD_SEPARATOR,
				playername.c_str(),				FIELD_SEPARATOR,
				descriptor,						RECORD_END
	);
}

void log_recursion (
			dbref		playerid,
	const	String&		playername,
			dbref		commandid,
	const	String&		commandname,
	const	String&		arguments
) {
	/*
	 * RECURSION|timestamp|playerid|playername|commandid|commandname|arguments
	 */

	Trace(	"%cRECURSION%c%d%c%d%c%s%c%d%c%s%c%s%c\n",
				RECORD_START,						FIELD_SEPARATOR,
				time(NULL),							FIELD_SEPARATOR,
				playerid,							FIELD_SEPARATOR,
				playername.c_str(),					FIELD_SEPARATOR,
				commandid,							FIELD_SEPARATOR,
				commandname.c_str(),				FIELD_SEPARATOR,
				arguments.c_str(),					RECORD_END
	);
}

void log_shutdown (
			dbref			playerid,
	const	String&		playername,
	const	String&		reason
) {
	/*
	 * SHUTDOWN|timestamp|playerid|playername|reason
	 */

	Trace(	"%cSHUTDOWN%c%d%c%d%c%s%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				playerid,						FIELD_SEPARATOR,
				playername.c_str(),				FIELD_SEPARATOR,
				reason.c_str(),					RECORD_END
	);			
}

void log_wall (
			dbref			playerid,
	const String& playername,
	const String& message
) {
	/*
	 * WALL|timestamp|playerid|playername|message
	 */

	Trace(	"%cWALL%c%d%c%d%c%s%c%s%c\n",
				RECORD_START,					FIELD_SEPARATOR,
				time(NULL),						FIELD_SEPARATOR,
				playerid,						FIELD_SEPARATOR,
				playername.c_str(),						FIELD_SEPARATOR,
				message.c_str(),		RECORD_END
	);			
}



/* 
 * Because I'm a nut-case, I chose to document
 * this file by inserting perl POD codes into a
 * c-comment
 
=head1 NAME

log.c - ugly logging functions

=head1 DESCRIPTION

This file contains functions for writing information to stderr in a
consistent manner, so that we can parse the output with a chunk
of perl modules written specifically for the task


=cut
*/


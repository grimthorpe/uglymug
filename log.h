/*
 *
 * log.h: header file for things that need to know about
 * logging functions
 *
 *		Chisel Wright, December 2001
 */

#ifndef _LOG_H
#define _LOG_H

/* config.h tells us if we are using NEW_LOGGING or not */
#include "config.h"

	/*
	 * prototypes for NEW_LOGGING functions
	 */

	/* use log_message for 'generic' messages */
	void		log_message			(const char *, ...);

	void		log_accept			(int, int, const String&);
	void		log_boot			(dbref, const String&, dbref, const String&, const String&);
	void		log_bug				(const char *, ...);
	void		log_checkpointing	(const String&, int);
	void		log_command			(dbref, const String&, dbref, const String&, dbref, const String&, const String&);
	void		log_connect			(bool, int, dbref, const String&);
	void		log_created			(int, dbref, const String&);
	void		log_debug			(const char *, ...);
	void		log_disconnect		(dbref, const String&, int, int, const String&, bool);
	void		log_dumping			(bool, const char *, ...);
	void		log_gripe			(dbref, const String&, dbref, const String&, const String&);
	void		log_hack			(dbref, const char *, ...);
	void		log_halfquit		(dbref, const String&, int, int);
	void		log_huh				(dbref, const String&, dbref, const String&, dbref, dbref, const String&, const String&);
	void		log_loading			(const String&, bool);
	void		log_note			(dbref, const String&, dbref, const String&, dbref, bool, const String&);
	void		log_panic			(const char *, ...);
	void		log_reconnect		(dbref, const String&);
	void		log_reconnect		(dbref, const String&, int);
	// recursion: playerid, playername, commandid, commandname, arguments
	void		log_recursion		(dbref, const String&, dbref, const String&, const String&);
	void		log_shutdown		(dbref, const String&, const String&);
	void		log_wall			(dbref, const String&, const String&);

#endif /* _LOG_H */

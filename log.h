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

	void		log_accept			(int, int, const CString&);
	void		log_bug				(const char *, ...);
	void		log_checkpointing	(const CString&, int);
	void		log_command			(dbref, const CString&, dbref, const CString&, dbref, const CString&, const CString&);
	void		log_connect			(bool, int, dbref, const CString&);
	void		log_created			(int, dbref, const CString&);
	void		log_debug			(const char *, ...);
	void		log_disconnect		(dbref, const CString&, int, int, const CString&, bool);
	void		log_dumping			(bool, const char *, ...);
	void		log_gripe			(dbref, const CString&, dbref, const CString&, const CString&);
	void		log_hack			(const char *, ...);
	void		log_halfquit		(dbref, const CString&, int, int);
	void		log_huh				(dbref, const CString&, dbref, const CString&, dbref, dbref, const CString&, const CString&);
	void		log_loading			(const CString&, bool);
	void		log_note			(dbref, const CString&, dbref, const CString&, dbref, bool, const CString&);
	void		log_panic			(const char *, ...);
	void		log_reconnect		(dbref, const CString&);
	void		log_reconnect		(dbref, const CString&, int);
	void		log_recursion		(dbref, const CString&, int, const CString&, const CString&);
	void		log_shutdown		(dbref, const CString&, const CString&);
	void		log_wall			(dbref, const CString&, const CString&);

#endif /* _LOG_H */

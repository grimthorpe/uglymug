/*
 * command.h: Header for all files that make use of compound commands.
 *
 *	Peter Crowther, 31/8/90.
 */

#ifndef	__COMMAND_H
#define	__COMMAND_H

#ifndef	__DB_H
#include "db.h"
#endif	/* __DB_H */
#include "mudstring.h"


extern	const	String	empty_string;
extern	const	String	error_return_string;
extern	const	String	ok_return_string;
extern	const	String	recursion_return_string;
extern	const	String	unset_return_string;
extern	const	String	permission_denied;
extern		char	scratch_buffer [];


/*  static const unsigned int MAX_COMMAND_LEN=2048; */
#define MAX_COMMAND_LEN		2048 
#define	MAX_COMMAND_DEPTH	16
#define MAX_NESTING		10

#define	value_or_empty(str)	(((str) == NULL) ? empty_string.c_str() : (str))


enum	Command_next
{
	NORMAL_NEXT,
	IF_NEXT,
	ELSEIF_NEXT,
	ELSE_NEXT,
	ENDIF_NEXT,
	FOR_NEXT,
	WITH_NEXT,
	END_NEXT
};

extern	Command_next	what_is_next	(const char *);

#endif	/* __COMMAND_H */

/*
 * alarm.h: Header file for things that need to know about pending
 *	structures and similar delights.
 *
 *	Peter Crowther, 14/8/94.
 */


#ifndef	_ALARM_H
#define	_ALARM_H

#ifndef	_MATCH_H
#include "match.h"
#endif	/* _MATCH_H */

#pragma interface

class	Pending
{
    private:
	Pending(const Pending&); // DUMMY
	Pending& operator=(const Pending&); // DUMMY

	Pending			*previous;
	Pending			*next;
	dbref			object;
    protected:
	virtual	bool		operator<	(Pending &rhs)	{ return (object < rhs.object); }
				Pending		(dbref object);
    public:
	void			insert_into	(Pending **list);
	Pending			*remove_from	(Pending **list);
	void			remove_object	(Pending **list, dbref candidate);
	Pending			*get_next	()		{ return (next); }
	dbref			get_object	()		{ return (object); }
};


class	Pending_alarm
: public Pending
{
    private:
	time_t			time_to_execute;
	bool			operator<	(Pending &rhs)	{ return (time_to_execute < (*(Pending_alarm*) &rhs).time_to_execute); }
    public:
				Pending_alarm	(dbref object, time_t firing_time);
	time_t			get_time_to_execute ()		{ return (time_to_execute); }
};


class	Pending_fuse
: public Pending
{
    private:
	bool			success;
	String			command;
	String			arg1;
	String			arg2;
	Matcher			matcher;	/* Copied */
	bool			operator<	(Pending &)	{ return (true); }
    public:
				Pending_fuse	(dbref object, bool success, const String& cmd, const String& a1, const String& a2, const Matcher &matcher);
	virtual			~Pending_fuse	();
	void			fire		(context &c);
};

#endif	/* !_ALARM_H */

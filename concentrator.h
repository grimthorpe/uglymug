#ifndef CONCENTRATE_H_INCLUDED
#define CONCENTRATE_H_INCLUDED

enum conc_message_type { CONC_CONNECT, CONC_DATA, CONC_DISCONNECT };

struct conc_message
{
	enum conc_message_type type;
	short channel;
	long len;
};

#define	UGLY_PORT	6239

#endif


/*
 * cgitelnet - cgi gateway for telnet
 * Copyright (C) 1999, 2000  Roberto Arturo Tena Sánchez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Roberto Arturo Tena Sánchez  <arturo at users.sourceforge.net>
 */



/*
 * Includes
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "cgitelnet_config.h"
#include "cgi.h"



/*
 * Global variables and types
 */

#define MSG_INIT  1
#define KEY_T_MAX INT_MAX
key_t   key;
int     qid     = -1;

typedef struct {
	long mtype;
	char buff[1];
} msg;



/*
 * Prototypes
 */

void show_error (char *s);
void show_login_page (void);
void term_app (int dummy);



/*
 * Implementation
 */


#define MIN(a,b) (((a)>(b)) ? (b) : (a))


/* key must not be 0, because IPC_PRIVATE == 0 */
#define MAKE_KEY(key) \
	{ key = 1 + (int) (((float)KEY_T_MAX) * rand() / (RAND_MAX + 1.0)); }


void
show_error (char *s)
{
	printf ("<h1>Error</h1><p>\n");
	printf (s);
}


void
show_login_page (void)
{
	printf ("\
<html><head><title>Telnet login</title></head><body>\n\
<!-- This site uses cgitelnet: http://cgitelnet.sourceforge.net/ -->\n\
<h1>Telnet login</h1>\n\
<p>\n\
<form action=\"cgitelnet.cgi\" method=\"get\">\n\
Server: <input name=\"server\" type=\"text\" size=\"40\"><br>\n\
Port: <input name=\"port\" type=\"text\" value=\"23\" size=\"6\"><br>\n\
<p>\n\
<input type=\"submit\" value=\"    Login    \">\n\
</form>\n\
<p>\n\
<hr>\n\
<div align=\"right\">This site uses \
<a href=\"http://cgitelnet.sourceforge.net/\">cgitelnet</a>.</div>\n\
</body></html>\n\
");
}


void
show_frameset_page (void)
{
	printf ("\
<html><head><title>Telnet</title></head>\n\
<!-- This site uses cgitelnet: http://cgitelnet.sourceforge.net/ -->\n\
<frameset rows=\"*,65\" border=\"3\">\n\
\t<frame src=\"cgitelnet_msg.cgi?id=%d\" name=\"cgitelnet_msg\" scrolling=\"auto\">\
</frame>\n\
\t<frame src=\"cgitelnet_input.cgi?id=%d\" name=\"cgitelnet_input\" scrolling=\"auto\">\
</frame>\n\
</frameset>\n\
<noframes>This site require a frames-capable browser.</noframes>\n\
</html>\n\
", key, key);
}

void
term_app (int dummy)
{
	if (qid > 0) {
		msgctl (qid, IPC_RMID, 0);
	}
}


int
main (void)
{
	int      key_count;
	s_cgi *  cgi_vars;
	msg *    msg_server;
	msg *    msg_port;
	char *   server;
	char *   port;
	cgitelnet_configuration config;

	cgiHeader ();

	if (cgitelnet_config (&config)) {
		show_error ("Invalid configuration file");
		exit (1);
	}

	/* read values of the HTML form */
	cgi_vars = cgiInit ();
	server = cgiGetValue (cgi_vars, "server");
	if (server == NULL || server[0] == 0) {
		cgiFree (cgi_vars);
		show_login_page ();
		exit (0);
	}
	port = cgiGetValue (cgi_vars, "port");
	if (port == NULL || port[0] == 0) {
		cgiFree (cgi_vars);
		show_login_page ();
		exit (0);
	}
	msg_server = malloc (config.buffer_size + sizeof (long));
	if (msg_server == NULL) {
		show_error ("Not enough resources");
		exit (1); /* error */
	}
	msg_port = malloc (config.buffer_size + sizeof (long));
	if (msg_port == NULL) {
		show_error ("Not enough resources");
		free (msg_server);
		exit (1); /* error */
	}
	memcpy (msg_server->buff, server, MIN (config.buffer_size - 1, strlen(server)+1));
	memcpy (msg_port->buff,   port,   MIN (config.buffer_size - 1, strlen(port)+1));
	msg_server->buff[config.buffer_size - 1] = 0;
	msg_port->buff[config.buffer_size - 1]   = 0;
	cgiFree (cgi_vars);

	signal (SIGINT,  term_app);
	signal (SIGILL,  term_app);
	signal (SIGABRT, term_app);
	signal (SIGFPE,  term_app);
	signal (SIGSEGV, term_app);
	signal (SIGTERM, term_app);

	/* generate message queue key */
	srand (time (NULL));
	MAKE_KEY (key);
	key_count++;
	while (1) {
		qid = msgget (key, IPC_CREAT | IPC_EXCL | 0610);
		if (qid >= 0)
			break;
		else if (qid < 0) {
			if (errno == ENOMEM || errno == ENOSPC) {
				/* not enough resources */
				show_error ("Not enough resources");
				exit (1); /* error */
			} else {
				MAKE_KEY (key);
				key_count++;
				if (key_count > config.max_id_tries) {
					/* couldn't generate key */
					exit (2); /* error */
				}
			}
		}

	}

	/* write values of the form in the message queue */
	msg_server->mtype = MSG_INIT;
	msg_port->mtype   = MSG_INIT;
	msgsnd (qid, msg_server, strlen (msg_server->buff) + 1, 0);
	msgsnd (qid, msg_port,   strlen (msg_port->buff)   + 1, 0);
	free (msg_server);
	free (msg_port);

	show_frameset_page ();

	exit (0);
}


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

/* XXX FIXME TODO [Enter] button, [Send Enter] checkbox */


/*
 * Includes
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "cgitelnet_config.h"
/* XXX */
int config_fixed_font = 1;
int config_break_lines = 1;
int config_column_width = 80;
char* config_foreground_color = "#000000";
char* config_background_color = "#C0C0C0";
char config_escape = '';
char config_escape_replace = ' ';
int config_max_id_tries = 500;
int config_buffer_size = 19600;
int config_scroll_timeout = 50;
int config_scroll_by = 300;
#include "cgi.h"



/*
 * Global variables and types
 */

#define MSG_INPUT    2
key_t   key;
/*int     with_enter = 1;*/
cgitelnet_configuration config;

typedef struct {
	long mtype;
	char buff[1];
} msg;



/*
 * Prototypes
 */

void show_error (char *s);
void show_input_page (void);



/*
 * Implementation
 */


#define MIN(a,b) (((a)>(b)) ? (b) : (a))


void
show_error (char *s)
{
	printf ("<h1>Error</h1><p>\n");
	printf (s);
}


void
show_input_page (void)
{
	printf ("\
<html><head><title></title>\n\
<script>\n\
var win = window.parent.frames[\"cgitelnet_msg\"];\n\
var last = -1;	/* last vertical position */\n\
var running;\n\
function scrollWindowDown ()\n\
{\n\
	if (win.pageYOffset >= last) {\n\
		win.scrollBy (0, %d);\n\
		last = win.pageYOffset;\n\
		/* call ourselves again in a few milliseconds */\n\
		setTimeout (\"scrollWindowDown ()\", %d);\n\
		document.input_form.input.focus(); \
	} else /* win.pageYOffset < last */ {\n\
		running = 0; \n\
	}\n\
}\n\
</script>\n\
</head><body onLoad='\n\
scrollWindowDown (); \
running = 1; \
document.input_form.input.focus(); \
return true; \
'>\n\
<!-- This site uses cgitelnet: http://cgitelnet.sourceforge.net/ -->\n\
<table border=\"0\" cellspacing=\"0\" cellpadding=\"2\">\n\
<tr><td>\n\
<form name=\"input_form\" action=\"cgitelnet_input.cgi\" method=\"post\">\n\
<!-- XXX FIXME --><!-- form action=\"cgitest/listall\" method=\"post\" -->\n\
<input name=\"id\" type=\"hidden\" value=\"%d\">\n\
<input name=\"input\" type=\"text\" size=\"50\">\
<input name=\"submit_input\" type=\"submit\" value=\"Send\"> &nbsp; \
<input name=\"submit_enter\" type=\"submit\" value=\"Send Enter\">\
</form>\n\
</td><td>\n\
<form onSubmit=\"if (!running) { last=-1; scrollWindowDown (); running=1; } return false;\">\n\
<input type=\"submit\" value=\"Scroll\">\n\
</form>\n\
</td></tr></table>\n\
</body></html>\n\
", config.scroll_by, config.scroll_timeout, key /*, (with_enter ? "checked" : "")*/);

/*<with Enter:<input type=\"checkbox\" name=\"with_enter\" %s>\n\*/
}


int
main (void)
{
	s_cgi *  cgi_vars;
	msg *    msg_input;
	char *   input;
	char *   skey;
	char *   submit;
	/*char *   swith_enter;*/
	int      qid;

	cgiHeader ();

	if (cgitelnet_config (&config)) {
		show_error ("Invalid configuration file");
		exit (1);
	}

	/* read CGI values, which come from frameset and the HTML form */
	cgi_vars = cgiInit ();
	skey = cgiGetValue (cgi_vars, "id");
	if (skey != NULL)
		key = atoi (skey);
	else
		key = -1;
	if (skey == NULL || skey[0] == 0 || key < 0) {
		cgiFree (cgi_vars);
		show_error ("Fatal error");
		exit (1); /* error */
	}
	msg_input = malloc (config.buffer_size + sizeof (long));
	if (msg_input == NULL) {
		show_error ("Not enough resources");
		exit (1); /* error */
	}
	submit = cgiGetValue (cgi_vars, "submit_enter");
	if (submit != NULL && !strcmp ("Send Enter", submit))
		memcpy (msg_input->buff, "\n", 2);
	else {
		input = cgiGetValue (cgi_vars, "input");
		if (input == NULL || input[0] == 0) {
			cgiFree (cgi_vars);
			show_input_page ();
			free (msg_input);
			exit (0);
		}
		memcpy (msg_input->buff, input, MIN (config.buffer_size - 1,
						    strlen (input) + 1));
		msg_input->buff[config.buffer_size - 1] = 0;
		/*swith_enter = cgiGetValue (cgi_vars, "with_enter");
		with_enter = (swith_enter != NULL
			      && !strcmp ("on", swith_enter));
		if (with_enter) {*/
			if (strlen (msg_input->buff) == config.buffer_size - 1)
				msg_input->buff[config.buffer_size - 2] = '\n';
			else
				strcat (msg_input->buff, "\n");
		/*}*/
	}
	cgiFree (cgi_vars);

	/* get the message queue id */
	qid = msgget (key, 0);
	if (qid < 0) {
		show_error ("Fatal error");
		free (msg_input);
		exit (1); /* error */
	}

	/* write input in the message queue */
	msg_input->mtype = MSG_INPUT;
	msgsnd (qid, msg_input, strlen (msg_input->buff) + 1, 0);
	free (msg_input);

	/* regenerate input page */
	show_input_page ();

	exit (0);
}


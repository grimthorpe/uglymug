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
 * Headers
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "cgitelnet_config.h"
/* XXX */
int config_fixed_font = 1;
int config_break_lines = 1;
int config_column_width = 80;
char* config_foreground_color = "#000000";
char* config_background_color = "#E0C0C0";
char config_escape = '';
char config_escape_replace = ' ';
int config_max_id_tries = 500;
int config_buffer_size = 19600;
int config_scroll_timeout = 50;
/* JPK 
   This needs to be global so that do_write_raw_str can change it
*/
time_t  last_browser_verify     = 0;
/* JPK
   This is the maximum number of seconds until we will write something
   to the html output.  I believe that 30 seconds is a common timeout
   by browsers, so it is set to 25 which gives us 5 seconds leeway,
   but reduces teh output by an order of magnitude
*/
#define BROWSER_VERIFY_TIMEOUT 25

/* JPK
   This is how much it waits at the end of every loop.  It is measured
   in microseconds.  The number should be LESS than 1000000.
   1000 gives a delay of a millisecond, which should be sufficiently
   small to make it imperceptible to the user, but save a LOT of CPU
   cycles.  Practical trials support this.  It may well be able to
   be increased with no adverse affects too.
*/
#define USLEEPTIME 1000


#include "cgi.h"



/*
 * Global variables and types
 */

#define MSG_INIT    1
#define MSG_INPUT   2
#define FD_STDIN    0
#define FD_STDOUT   1
#define FD_STDERR   2
int     FD_DEBUG_HTML;
int     FD_DEBUG_TXT;
int     pid_telnet;
int     qid       = -1;
cgitelnet_configuration config;

typedef struct {
	long mtype;
	char buff[1];
} msg;



/*
 * Prototypes
 */

void do_write_raw_str (char *s);
void do_restore (void);
void do_fgcolor (char *color);
void do_bgcolor (char *color);
void do_write (unsigned char c);
void do_start (void);
void term_app (int dummy);
/* to avoid warnings when compiling with "-ansi" */
/* int kill (int pid, int sig); */
void show_error (char *s);



/*
 * Implementation
 */


void
do_write_raw_str (char *s)
{
	write (FD_STDOUT, s, strlen (s));
	write (FD_DEBUG_HTML, s, strlen (s));
/* JPK
   Every time we write out some html we are causing the browser to be
   aware of us, so we don't need to artificially send a comment to keep
   the connection alive
*/
	last_browser_verify=time(NULL);
}


void
do_restore (void)
{
	do_write_raw_str ("<font color=\"");
	do_write_raw_str (config.foreground_color);
	do_write_raw_str ("\"><font class=\"BGDEFAULT\">");
}


void
do_fgcolor (char *color)
{
	do_write_raw_str ("<font color=\"");
	do_write_raw_str (color);
	do_write_raw_str ("\">");
}


void
do_bgcolor (char *color)
{
	do_write_raw_str ("<font class=\"bg");
	do_write_raw_str (color);
	do_write_raw_str ("\">");
}


void
do_write (unsigned char c)
{
	char s[2];
	static int ccount = 0;

	s[1] = 0;
	s[0] = c;

	switch (c) {
	case '\n':
		ccount = 0;
		do_write_raw_str (s);
		break;
	case '':
		/* do not write it */
		return;
	case '<':
		do_write_raw_str ("&lt;");
		break;
	case '>':
		do_write_raw_str ("&gt;");
		break;
	case '\t': {
			   /* FIXME */
			   int i;
			   for (i = 0; i < 8 - (ccount % 8); i++)
				   do_write (' ');
		   }
		break;
	default:
		do_write_raw_str (s);
		break;
	}

	if (config.break_lines && (++ccount) > config.column_width) {
		do_write_raw_str ("\n");
		ccount = 0;
	}
}


void
do_start (void)
{
	do_write_raw_str ("\
<html><head><title></title>\n\
<style>\n\
.BGDEFAULT { background: ");
	do_write_raw_str (config.background_color);
	do_write_raw_str ("\
; }\n\
.BGBLACK { background: black; }\n\
.BGRED { background: red; }\n\
.BGGREEN { background: green; }\n\
.BGYELLOW { background: yellow; }\n\
.BGBLUE { background: blue; }\n\
.BGMAGENTA { background: magenta; }\n\
.BGCYAN { background: cyan; }\n\
.BGWHITE { background: white; }\n\
</style>\n\
</head>\n\
<SCRIPT LANGUAGE=\"JavaScript\"><!--\n
function myScroll() {\n
    window.scrollBy(0,100)\n
    setTimeout('myScroll()',1000); // scrolls every 1000 miliseconds\n
}\n
\n
if (document.layers || document.all)\n
    myScroll()\n
//--></SCRIPT>\n
<body text=\"");
	do_write_raw_str (config.foreground_color);
	do_write_raw_str ("\" bgcolor=\"");
	do_write_raw_str (config.background_color);
	do_write_raw_str ("\">\n");
	if (config.fixed_font)
		do_write_raw_str ("<pre>");
}


void
term_app (int dummy)
{
	/* kill telnet */
	kill (pid_telnet, SIGTERM);

	/* delete message queue */
	if (qid > 0) {
		msgctl (qid, IPC_RMID, 0);
		qid = -1;
	}
}


void
show_error (char *s)
{
	do_write_raw_str ("<h1>Error</h1><p>\n");
	do_write_raw_str (s);
}


int
main (void)
{
	s_cgi *  cgi_vars;
	msg *    msg_input;
	int      pipefd1[2];
	int      pipefd2[2];
	char *   server;
	char *   port;
	char *   skey;
	key_t    key;

	cgiHeader ();
	fflush (stdout);

	if (cgitelnet_config (&config)) {
		show_error ("Invalid configuration file");
		exit (1);
	}

	server = malloc (config.buffer_size);
	if (server == NULL) {
		show_error ("Not enough resources");
		exit (1); /* error */
	}
	port = malloc (config.buffer_size);
	if (port == NULL) {
		show_error ("Not enough resources");
		free (server);
		exit (1); /* error */
	}

	/* FIXME */
#ifdef CGITELNET_DEBUG
	FD_DEBUG_HTML = creat ("/tmp/debug.html", 0777);
	FD_DEBUG_TXT  = creat ("/tmp/debug.txt",  0777);
#endif

	do_start ();

	/* read CGI values, which come from frameset */
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
	
	/* get the message queue id */
	qid = msgget (key, 0);
	if (qid < 0) {
		show_error ("Fatal error");
		exit (1); /* error */
	}

	/* read server from the message queue */
	msg_input = malloc (config.buffer_size + sizeof (long));
	if (msg_input == NULL) {
		show_error ("Not enough resources");
		exit (1); /* error */
	}
	if (msgrcv (qid, msg_input, config.buffer_size, MSG_INIT, MSG_NOERROR) < 0) {
		show_error ("Fatal error");
		free (msg_input);
		free (server);
		free (port);
		exit (1); /* error */
	}
	memcpy (server, msg_input->buff, config.buffer_size);
	server [config.buffer_size - 1] = 0;
	/* read port from the message queue */
	if (msgrcv (qid, msg_input, config.buffer_size, MSG_INIT, MSG_NOERROR) < 0) {
		show_error ("Fatal error");
		free (msg_input);
		free (server);
		free (port);
		exit (1); /* error */
	}
	memcpy (port, msg_input->buff, config.buffer_size);
	port [config.buffer_size - 1] = 0;

	/* create telnet process */
	if (pipe (pipefd1) < 0) {
		perror ("Error creating pipe 1");
		free (msg_input);
		free (server);
		free (port);
		exit (1);
	}
	if (pipe (pipefd2) < 0) {
		perror ("Error creating pipe 2");
		free (msg_input);
		free (server);
		free (port);
		exit (1);
	}
	pid_telnet = fork ();
	if (pid_telnet == 0) /* children */ {
		dup2 (pipefd1[0], FD_STDIN);
		dup2 (pipefd2[1], FD_STDOUT);
		dup2 (pipefd2[1], FD_STDERR);
		close (pipefd1[0]);
		close (pipefd1[1]);
		close (pipefd2[0]);
		close (pipefd2[1]);
		/* execlp ("telnet", "telnet", server, port, NULL); */
		execlp ("telnet", "telnet", "uglymug.org.uk", "6239", NULL);
		exit (2); /* shouldn't reach here */

	} else /* parent */ {
		/* control telnet process */
		char *  buff;
		int     telnet_read;
		int     telnet_write;
		int     nbytes, i;
		int     fd_flags;
		int     ansi_state;
		char   *ansi_color;
		time_t  last_flush              = 0;
		int     flushed                 = 0;

		buff = malloc (config.buffer_size);
		if (buff == NULL) {
			show_error ("Not enough resources");
			free (msg_input);
			free (server);
			free (port);
			exit (1); /* error */
		}

		signal (SIGINT,  term_app);
		signal (SIGILL,  term_app);
		signal (SIGABRT, term_app);
		signal (SIGFPE,  term_app);
		signal (SIGSEGV, term_app);
		signal (SIGTERM, term_app);

		telnet_read = pipefd2[0];
		telnet_write = pipefd1[1];
		close (pipefd1[0]);
		close (pipefd2[1]);

		/* non-blocking telnet_read */
		fd_flags = fcntl (telnet_read, F_GETFL, 0);
		fd_flags |= O_NONBLOCK;
		fcntl(telnet_read, F_SETFL, fd_flags);

		/* pool */
		ansi_state = 0;
		while (1) {
			int telnet_status;
			int pid;

			/* read from message queue and write to telnet */
			/* read from message queue */
			/*nbytes = read (FD_STDIN, buff, config.buffer_size);*/
			nbytes = msgrcv (qid, msg_input, config.buffer_size, MSG_INPUT,
					 IPC_NOWAIT | MSG_NOERROR);
			nbytes--; /* don't take the trailing zero! */
			if (nbytes < 0) {
				if (errno != ENOMSG) {
					show_error ("Fatal error");
					free (msg_input);
					free (server);
					free (port);
					free (buff);
					term_app (0);
					exit (1); /* error */
				} /* ignore if did not read bytes */
			} else if (nbytes > 0) {
				memcpy (buff, msg_input->buff, config.buffer_size);
				/* write to telnet */
				for (i = 0; i < nbytes; i++)
					if (buff[i] == config.escape)
					   buff[i] = config.escape_replace;
				write (telnet_write, buff, nbytes);

				/* FIXME */
				/*
				do_write_raw_str ("<b><tt>[");
				do_write_raw_str (buff);
				printf ("(%d)", nbytes);
				fflush (stdout);
				do_write_raw_str ("]</tt></b>");*/

			} /* ignore if did not read bytes */
 
			/* verify if telnet process is still there */
			pid = waitpid (pid_telnet, &telnet_status, WNOHANG);
			if (pid == pid_telnet) {
				/* telnet process ended */
				do_write_raw_str ("telnet ended\n");
				break;
			} else if (pid < 0) {
				break; /* error */
			}

			/* verify if browser is still there */
			/* XXX */

/* JPK
   We don't want to do this EVERY iteration.  Rather than do it every
   n iterations the way to do it is every x seconds.
   However we do not need to do it if something else has been written
   in the last BROWSER_VERIFY_TIMEOUT seconds, so modify 
   do_write_raw_str to reset the last_browser_verify rather than just
   doing it here.
*/
		if (time(NULL) - last_browser_verify >= BROWSER_VERIFY_TIMEOUT)
		{
			do_write_raw_str ("<!-- -->");
		}

			/* read from telnet and write to output */
			nbytes = read (telnet_read, buff, config.buffer_size);
			if (nbytes < 0) {
				if (errno != EAGAIN) {
					break;
				} else {
				       	/* did not read bytes */
					/* wait 1 second from the last flush */
					if (!flushed
					    && time (NULL) - last_flush >= 1) {
						last_flush = time (NULL);
						do_write_raw_str
							("</pre>\n<pre>");
						flushed = 1;
					}
				}
			} else {
#ifdef CGITELNET_DEBUG
				write (FD_DEBUG_TXT, buff, nbytes);
#endif

				flushed = 0;
				for (i = 0; i < nbytes; i++) {
					switch (ansi_state) {
					case 0: /* wait for an ansi code */
						if (buff[i] == 27)
							ansi_state = 1;
						else {
							do_write (buff [i]);
							ansi_state = 0;
						}
						break;
					case 1: /* wait for [ */
						if (buff[i] == '[')
							ansi_state = 2;
						else {
							do_write (buff[i]);
							ansi_state = 0;
						}
						break;
					case 2: /* wait for first char */
						switch (buff[i]) {
						case '0': ansi_state = 3; break;
						case '3': ansi_state = 4; break;
						case '4': ansi_state = 5; break;
						default:  ansi_state = 6; break;
						}
						break;
					case 3: /* wait for m -> do restore */
						if (buff[i] == 'm') {
							do_restore ();
							ansi_state = 0;
						} else
							ansi_state = 6;
						break;
					case 4: /* wait for fgcolor */
						switch (buff[i]) {
						case '0': ansi_color = "black";
							  break;
						case '1': ansi_color = "red";
							  break;
						case '2': ansi_color = "green";
							  break;
						case '3': ansi_color = "yellow";
							  break;
						case '4': ansi_color = "blue";
							  break;
						case '5': ansi_color ="magenta";
							  break;
						case '6': ansi_color = "cyan";
							  break;
						case '7': ansi_color = "white";
							  break;
						default:
							  ansi_color = NULL;
						}
						if (ansi_color == NULL)
							ansi_state = 6;
						else
							ansi_state = 7;
						break;
					case 5: /* wait for bgcolor */
						switch (buff[i]) {
						case '0': ansi_color = "black";
							  break;
						case '1': ansi_color = "red";
							  break;
						case '2': ansi_color = "green";
							  break;
						case '3': ansi_color = "yellow";
							  break;
						case '4': ansi_color = "blue";
							  break;
						case '5': ansi_color ="magenta";
							  break;
						case '6': ansi_color = "cyan";
							  break;
						case '7': ansi_color = "white";
							  break;
						default:
							  ansi_color = NULL;
						}
						if (ansi_color == NULL)
							ansi_state = 6;
						else
							ansi_state = 8;
						break;
					case 6: /* search end of ansi code */
						if (buff[i] == 'm')
							ansi_state = 0;
						break;
					case 7: /* wait for m -> do_fgcolor */
						switch (buff[i]) {
						case 'm':
							do_fgcolor (ansi_color);
							ansi_state = 0;
							break;
						case ';':
							do_fgcolor (ansi_color);
							ansi_state = 2;
							break;
						default:
							ansi_state = 6;
							break;
						}
						break;
					case 8: /* wait for m -> do_bgcolor */
						switch (buff[i]) {
						case 'm':
							do_bgcolor (ansi_color);
							ansi_state = 0;
							break;
						case ';':
							do_bgcolor (ansi_color);
							ansi_state = 2;
							break;
						default:
							ansi_state = 6;
							break;
						}
						break;
					default:
						do_write_raw_str
							("fatal error\n");
						free (msg_input);
						free (server);
						free (port);
						free (buff);
						term_app (0);
						exit (3);
					}
				}
			}
		/* JPK 
		   Ok, don't want to just loop as far as we can here
		   Solution, sleep for a bit.  Should use itimer, but
		   usleep is so much easier!
		*/
		/* Pause for 1000th of a second, this should be significant
		   for the computer, but not introduce a lag
		*/
		usleep(USLEEPTIME);
		}
	}

	free (msg_input);
	free (server);
	free (port);
	term_app (0);
	exit (0);
}


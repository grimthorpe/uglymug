/*
 * Port redirector for UglyMUG
 * by Ian Chard  3-DEC-2003
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/telnet.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#ifndef INADDR_NONE
#define	INADDR_NONE	0xffffffff
#endif

#define	MAX_IDLE_TIME	21600	/* seconds */

const char *error_message="\nSorry, unable to redirect your connection.\nPlease try connecting using port 6239.\n\n";
const char *down_message="\nSorry, UglyMUG is down at the moment.\nPlease try later.\n\n";
const char *timeout_message="\nConnection timed out by redirector.\nTo avoid this in future, use the @spam feature.\n\n";
const char telopt_preamble[]={ IAC, SB, TELOPT_SNDLOC };
const char telopt_postamble[]={ IAC, SE };

struct sockaddr_in target;
int listener;


void reaper(int sig)
{
	signal(SIGCHLD, reaper);
	while(wait3(NULL, WNOHANG, NULL)>0);
}


void send_error_and_close(int s, const char *msg)
{
	/* We've already encountered an error, so if we get another one, we don't
	   really care. */

	send(s, msg, strlen(msg), 0);
	shutdown(s, 2);
	close(2);
}


pid_t process_connection(void)
{
	struct sockaddr_in addr;
	int sizeof_addr=sizeof(addr);
	char *address, buf[1024];
	int opt=1, in, out, fds, len;
	pid_t pid;
	fd_set fdset;
	struct timeval tv;
	extern int errno;

	if((pid=fork())<0)
	{
		perror("fork");
		return 0;
	}
	else if(pid!=0)
		/* We're the parent, so return immediately */
		return pid;

	/* Beyond this point, there's no-one we can tell if function calls fail. */

	if((in=accept(listener, (struct sockaddr *) &addr, &sizeof_addr))<0)
		exit(1);
	if((out=socket(PF_INET, SOCK_STREAM, 0))<0)
	{
		send_error_and_close(in, error_message);
		exit(1);
	}

	if(connect(out, (struct sockaddr *) &target, sizeof(target))<0)
	{
		send_error_and_close(in, errno==ECONNREFUSED? down_message:error_message);
		exit(1);
	}

	/* Send our location before the client has a chance to send anything */

	send(out, telopt_preamble, sizeof(telopt_preamble), 0);
	address=inet_ntoa(addr.sin_addr);
	send(out, address, strlen(address), 0);
	send(out, telopt_postamble, sizeof(telopt_postamble), 0);

	setsockopt(in, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

	for(;;)
	{
		FD_ZERO(&fdset);
		FD_SET(in, &fdset);
		FD_SET(out, &fdset);
		tv.tv_sec=MAX_IDLE_TIME;
		tv.tv_usec=0;

		if((fds=select((in>out? in:out)+1, &fdset, NULL, NULL, &tv))<0)
			exit(1);

		if(fds==0)
		{
			send_error_and_close(in, timeout_message);
			exit(0);
		}

		/* Deal with data coming from clients... */

		if(FD_ISSET(in, &fdset))
		{
			len=read(in, buf, sizeof(buf));
			if(len<=0)
				exit(0);

			if(send(out, buf, len, 0)<0)
				exit(0);
		}

		/* ...and from the server. */

		if(FD_ISSET(out, &fdset))
		{
			len=read(out, buf, sizeof(buf));
			if(len<=0)
				exit(0);

			if(send(in, buf, len, 0)<0)
				exit(0);
		}
	}

	/* We should never get here, but we call exit() instead of returning
	   so that we don't end up with more than one process as the master. */

	exit(1);
}


int main(int argc, char *argv[])
{
	short port;
	struct hostent *h;
	fd_set fdset;
	struct sockaddr_in addr;
	int opt=1;
	extern int errno;

	if(argc!=4)
	{
		fprintf(stderr, "usage: %s <listen port> <target host> <target port>\n"
				"e.g.   %s 23 localhost 6239\n", argv[0], argv[0]);
		return 1;
	}

	port=htons(atoi(argv[1]));

	if((target.sin_addr.s_addr=inet_addr(argv[2]))==INADDR_NONE)
	{
		if((h=gethostbyname(argv[2])))
			memcpy(&target.sin_addr.s_addr, h->h_addr_list[0], 4);
		else
		{
			fprintf(stderr, "%s: %s: unknown host\n", argv[0], argv[2]);
			return 1;
		}
	}

	target.sin_family=AF_INET;
	target.sin_port=htons(atoi(argv[3]));

	if((listener=socket(PF_INET, SOCK_STREAM, 0))<0)
	{
		perror("socket");
		return 1;
	}

	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0)
	{
		perror("setsockopt");
		return 1;
	}

	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=INADDR_ANY;
	addr.sin_port=htons(atoi(argv[1]));

	if(bind(listener, (struct sockaddr *) &addr, sizeof(addr))<0)
	{
		perror("bind");
		return 1;
	}

	if(listen(listener, 8)<0)
	{
		perror("listen");
		return 1;
	}

	signal(SIGCHLD, reaper);

	for(;;)
	{
		FD_ZERO(&fdset);
		FD_SET(listener, &fdset);

		if(select(listener+1, &fdset, NULL, NULL, NULL)<0)
		{
			if(errno==EINTR)
				continue;
			else
			{
				perror("select");
				return 1;
			}
		}

		if(!process_connection())
		{
			fprintf(stderr, "%s: fork failed. we're toast.\n", argv[0]);
			return 1;
		}
	}

	return 0;
}

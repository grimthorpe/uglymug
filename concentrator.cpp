#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "concentrator.h"
#include "config.h"

const char *concentrator_connect_message = "\nConnecting to UglyMug via Flup's Concentrator (tm)...\n\n";

struct user
{
	int channel;
	struct user *next, **prev;
} *user_list=NULL;


void remove_descriptor(int nuke)
{
	struct user *current;

	for(current=user_list; current; current=current->next)
	{
		if(current->channel==nuke)
		{
			*current->prev=current->next;
			if(current->next)
				current->next->prev=current->prev;

			free(current);
			break;
		}
	}
}


int main(int argc, char *argv[])
{
	int server, maxd, ugly;
	socklen_t	addr_len;
	struct sockaddr_in addr;
	struct hostent *h;
	fd_set input_set, output_set;
	struct conc_message msg;
	char buf[BUFFER_LEN];
	int opt;

	if(argc!=2)
	{
		fprintf(stderr, "usage:  %s <uglymug's address>\n", argv[0]);
		return 1;
	}

	if((addr.sin_addr.s_addr=inet_addr(argv[1]))==-1)
	{
		if((h=gethostbyname(argv[1]))==NULL)
		{
			fprintf(stderr, "%s:  unknown host '%s'\n", argv[0], argv[1]);
			return 1;
		}
		addr.sin_addr.s_addr=(h->h_addr_list[0][0]<<24 | h->h_addr_list[0][1]<<16 | h->h_addr_list[0][2]<<8 | h->h_addr_list[0][3]);
	}
	addr_len=sizeof(addr);

	ugly=socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	opt=1;
	setsockopt(ugly, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(CONC_PORT);
	if(connect(ugly, (struct sockaddr *) &addr, sizeof(addr))<0)
	{
		perror("connect");
		return 1;
	}

	server=socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	maxd=server+1;
	addr.sin_port=htons(TINYPORT);
	addr.sin_addr.s_addr=INADDR_ANY;
	if(bind(server, (struct sockaddr *) &addr, sizeof(addr))<0)
	{
		perror("bind");
		return 1;
	}
	if(listen(server, 5)<0)
	{
		perror("listen");
		return 1;
	}

	for(;;)
	{
		struct user *current;

		FD_ZERO(&input_set);
		FD_ZERO(&output_set);

		FD_SET(ugly, &input_set);
		FD_SET(server, &input_set);
		for(current=user_list; current; current=current->next)
			FD_SET(current->channel, &input_set);

		while(!select(maxd, &input_set, NULL, NULL, NULL));

		/* First check for data from Ugly */
		if(FD_ISSET(ugly, &input_set))
		{
			static char buf[512];

			if(read(ugly, &msg, sizeof(msg))<1)
			{
				shutdown(ugly,2);
				printf("Read error from Ugly!\n");
				return 1;
			}
			switch(msg.type)
			{
			case CONC_DATA:
				read(ugly, buf, msg.len);
				write(msg.channel, buf, msg.len);
				break;

			case CONC_DISCONNECT:
				shutdown(msg.channel, 2);
				remove_descriptor(msg.channel);
				break;

			default:
				fprintf(stderr, "Invalid message type %d\n", msg.type);
				break;
			}
		}

		if(FD_ISSET(server, &input_set))
		{
			/* New connection */

			struct user *newuser;

			newuser=(struct user *) malloc(sizeof(struct user));

			newuser->channel=accept(server, (struct sockaddr *) &addr, &addr_len);
			if(newuser->channel>=maxd)
				maxd=newuser->channel+1;

			msg.channel=newuser->channel;
			msg.type=CONC_CONNECT;
			msg.len=0;
			write(ugly, &msg, sizeof(msg));
			write(newuser->channel, concentrator_connect_message, strlen(concentrator_connect_message));

			if(user_list)
				user_list->prev=&newuser->next;

			newuser->next=user_list;
			newuser->prev=&user_list;
			user_list=newuser;
		}

		/* Now check for input from the users */
		for(current=user_list; current; current=current->next)
		{
			if(FD_ISSET(current->channel, &input_set))
			{
				int length;

				msg.channel=current->channel;
				if((length=read(current->channel, buf, BUFFER_LEN))<1)
				{
					/* Disconnect */

					msg.type=CONC_DISCONNECT;
					msg.len=0;
					write(ugly, &msg, sizeof(msg));
					shutdown(current->channel, 2);
					remove_descriptor(current->channel);
				}
				msg.type=CONC_DATA;
				msg.len=length;
				write(ugly, &msg, sizeof(msg));
				write(ugly, buf, length);
			}
		}
	}
}

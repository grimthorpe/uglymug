/* socket bouncer, by orabidoo  12 Feb '95 
   using code from mark@cairo.anu.edu.au's general purpose telnet server.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef _AIX
#include <sys/select.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/wait.h>

#define    QLEN           5
#define    DEFAULT_PORT   1523

char sbuf[16384], cbuf[16384];
extern int errno;
extern char *sys_errlist[];

void sigchld() {
  signal(SIGCHLD, sigchld);
  while(waitpid(0, (int *)0, WNOHANG|WUNTRACED)>=0);
}

void communicate(int sfd, int cfd) {
    char *chead, *ctail, *shead, *stail;
    int num, nfd, spos, cpos;

    extern int errno;
    fd_set rd, wr;

    struct itimerval itime;

    itime.it_interval.tv_sec=0;
    itime.it_interval.tv_usec=0;
    itime.it_value.tv_sec=21600;
    itime.it_value.tv_usec=0;
    setitimer(ITIMER_REAL,&itime,NULL);
    /* arbitrary connection time limit: 6 hours (in case the client hangs) */

    chead=ctail=cbuf;
    cpos=0;
    shead=stail=sbuf;
    spos=0;
    while (1) {
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        if (spos<sizeof(sbuf)-1) 
	    FD_SET(sfd, &rd);
        if (ctail>chead)
	    FD_SET(sfd, &wr);
        if (cpos<sizeof(cbuf)-1)
	    FD_SET(cfd, &rd);
        if (stail>shead)
	    FD_SET(cfd, &wr);
        nfd=select(256, &rd, &wr, 0, 0);
        if (nfd<=0) continue;
        if (FD_ISSET(sfd, &rd)) {
            num=read(sfd,stail,sizeof(sbuf)-spos);
            if ((num==-1) && (errno != EWOULDBLOCK)) return;
            if (num==0) {
#ifdef FNDELAY
		fcntl(sfd,F_SETFL,fcntl(sfd,F_GETFL,0)&~FNDELAY);
		fcntl(cfd,F_SETFL,fcntl(cfd,F_GETFL,0)&~FNDELAY);
#endif
		if (ctail!=chead) write(sfd,chead,ctail-chead);
		if (stail!=shead) write(cfd,shead,stail-shead);
		write(cfd,chead,0);
		return;
	    }
	    
            if (num>0) {
                spos+=num;
                stail+=num;
                if (!--nfd) continue;
            }
        }
        if (FD_ISSET(cfd, &rd)) {
            num=read(cfd,ctail,sizeof(cbuf)-cpos);
            if ((num==-1) && (errno != EWOULDBLOCK)) return;
            if (num==0) {
#ifdef FNDELAY
		fcntl(sfd,F_SETFL,fcntl(sfd,F_GETFL,0)&~FNDELAY);
		fcntl(cfd,F_SETFL,fcntl(cfd,F_GETFL,0)&~FNDELAY);
#endif
		if (ctail!=chead) write(sfd,chead,ctail-chead);
		if (stail!=shead) write(cfd,shead,stail-shead);
		write(sfd,chead,0);
		return;
	    }
		
            if (num>0) {
                cpos+=num;
                ctail+=num;
                if (!--nfd) continue;
            }
        }
        if (FD_ISSET(sfd, &wr)) {
            num=write(sfd,chead,ctail-chead);
            if ((num==-1) && (errno != EWOULDBLOCK)) return;
            if (num>0) {
                chead += num;
                if (chead==ctail) {
                    chead=ctail=cbuf;
                    cpos=0;
                }
                if (!--nfd) continue;
            }
        }
        if (FD_ISSET(cfd, &wr)) {
            num=write(cfd,shead,stail-shead);
            if ((num==-1) && (errno != EWOULDBLOCK)) return;
            if (num>0) {
                shead += num;
                if (shead==stail) {
                    shead=stail=sbuf;
                    spos=0;
                }
                if (!--nfd) continue;
            }
        }
    }
}

int main(int argc,char *argv[]) {
    int srv_fd, rem_fd, len, cl_fd, on=1;
    int myport=DEFAULT_PORT, remoteport;
    struct sockaddr_in rem_addr, srv_addr, cl_addr;
    char *myname;
    struct hostent *hp;

    myname=argv[0];
    if (argc==5) {
	if (strcmp(argv[1],"-p")==0) {
	    if ((myport=atoi(argv[2]))==0) {
		fprintf(stderr,"Bad port number.\n");
		exit(-1);
	    }
	    argv+=2;
	    argc-=2;
	} else {
	    fprintf(stderr,"Use: %s [-p localport] machine port \n",myname);
	    exit(-1);
	}
    }
    if (argc!=3) {
	fprintf(stderr,"Use: %s [-p localport] machine port \n",myname);
	exit(-1);
    }
    if ((remoteport=atoi(argv[2]))<=0) {
	fprintf(stderr, "Bad remote port number.\n");
	exit(-1);
    }

    memset((char *) &rem_addr, 0, sizeof(rem_addr));
    memset((char *) &srv_addr, 0, sizeof(srv_addr));
    memset((char *) &cl_addr, 0, sizeof(cl_addr));

    cl_addr.sin_family=AF_INET;
    cl_addr.sin_port=htons(remoteport);
    if ((hp=gethostbyname(argv[1]))==NULL) {
	cl_addr.sin_addr.s_addr=inet_addr(argv[1]);
	if (cl_addr.sin_addr.s_addr==-1) {
	    fprintf(stderr, "Unknown host.\n");
	    exit(-1);
	}
    } else
	cl_addr.sin_addr=*(struct in_addr *)(hp->h_addr_list[0]);

    srv_addr.sin_family=AF_INET;
    srv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    srv_addr.sin_port=htons(myport);
    srv_fd=socket(PF_INET,SOCK_STREAM,0);
    if (bind(srv_fd,&srv_addr,sizeof(srv_addr))==-1) {
	perror("bind");
        exit(-1);
    }
    listen(srv_fd,QLEN);

    signal(SIGCHLD, sigchld);
    printf("Ready to bounce connections from port %i to %s on port %i\n",
	   myport, argv[1], remoteport);
    close(0); close(1); close(2);
    chdir("/");
#ifdef TIOCNOTTY
    if ((rem_fd=open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(rem_fd, TIOCNOTTY, (char *)0);
        close(rem_fd);
    }
#endif
    if (fork()) exit(0);
    while (1) {
	len=sizeof(rem_addr);
	rem_fd=accept(srv_fd,&rem_addr,&len);
	if (rem_fd<0) {
	  if (errno==EINTR) continue;
	  exit(-1);
        }
	switch(fork()) {
	  case -1:
	    /* we're in the background.. no-one to complain to */
	    break;
	  case 0:                           /* child process */
	    close(rem_fd);
	    break;
	  default:			    /* parent process */
	    close(srv_fd);                  /* close original socket */
	    if ((cl_fd=socket(PF_INET, SOCK_STREAM, 0))<0) {
		close(rem_fd);
		exit(-1);
	    }
	    if (connect(cl_fd, (struct sockaddr *)&cl_addr, 
	                sizeof(cl_addr))<0) {
		close(rem_fd);
		exit(-1);
	    }
	    
	    setsockopt(cl_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
	    setsockopt(rem_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

	    communicate(cl_fd, rem_fd);
	    close(rem_fd);
	    exit(0);
	}
    }
}


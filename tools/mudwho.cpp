#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#define	BUFFER_LEN	1024
#define	OUTFILE		".plan"

int main(int argc,char *argv[])
{
	int com;
	struct sockaddr_in addr;
	struct hostent *host;
	char c;
	FILE *s,*fp;
	char scratch_buffer [BUFFER_LEN];
	//extern char *sys_errlist[];

	addr.sin_family=AF_INET;
	addr.sin_port=6239;
	addr.sin_addr.s_addr=INADDR_LOOPBACK;

	fp=fopen(OUTFILE, "w");

	com=socket(PF_INET,SOCK_STREAM,0);
	if(connect(com,(struct sockaddr *)&addr,sizeof(addr))<0)
	{
		printf("Failed to connect to Uglymug:  %s\n", sys_errlist[errno]);
		exit(1);
	}
	else if(errno==ECONNREFUSED)
	{
		fprintf(fp, "\n\nSorry, UglyMug is down at the moment.\n\n");
		return 0;
	}

	fprintf(fp,"\nList of current mud users:\n\n");
	s=fdopen(com,"w");

	fprintf(s,"WHO\nQUIT\n");
	fflush(s);

	s=fdopen(com,"r");

	while(strstr(scratch_buffer, "UNCONNECTED", 11) == NULL) 
	{
		fgets(scratch_buffer, BUFFER_LEN, s);
	}

	while(!feof(s))
	{
		c=fgetc(s);
		fputc(c,fp);
	}
	shutdown(com,2);
	fclose(s);
	fclose(fp);

	return 0;
}

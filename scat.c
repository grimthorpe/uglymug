/** \file scat.c
 * scat - by Ian Chard
 *
 * This program works just like 'cat' with a few differences.
 * Firstly, the standard output cannot be used as the command's output.
 *
 * The command will copy standard input to a file until it receives a
 * certain signal (defined by CHOPSIG).  At this point it will close the
 * output file, and start writing to a new file with a different name.
 *
 * The last parameter on the command line will be used as a template
 * for the file names; for example "wibble" will result in files called
 * "wibble.000", "wibble.001" etc.  The maximum is "wibble.999" in which
 * case the program will wrap back to 000 again.
 *
 */

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#define	CHOPSIG		SIGUSR1		/* Signal caught by command */
#define	usage()		fprintf(stderr,"Usage: %s <output file> <cut file>\n",argv[0])
#define PIDFILE         "scat.pid"

char *outputfile;
char *cutfile;
void handler (int);
FILE *out=NULL;

struct sigaction handle;

int main(int argc, char *argv[])
{
char l[1024];
FILE *pidfile=NULL;
int tmp;


	handle.sa_handler = handler;
	handle.sa_flags = SA_RESTART;
	/* First things first;: set up the signal handler */
	sigaction(CHOPSIG, &handle, NULL);

	/* output process id */
	if((pidfile=fopen(PIDFILE,"w"))==NULL)
	{
		fprintf(stderr,"scat: can't create process file '%s' (%s)",PIDFILE, strerror (errno));
		exit(errno);
	}
	tmp=getpid();
	fprintf(pidfile, "%d\n", tmp );
	fclose(pidfile);

	/* Now interpret the arguments */
	if(argc!=3)
	{
		usage();
		exit(0);
	}

	/* Allocate memory for the filename and copy it */
	outputfile = (char*)strdup(argv[1]);
	cutfile = (char*)strdup(argv[2]);

	/* Produce first file */
	handler(0);

	/* Let's go! */
	while(fgets(l, sizeof(l), stdin) != NULL)
	{
		fputs(l, out);	
	}
	fflush(out);
	handler(0);
	fclose(out);
	return 0;
}


void handler(int)
{
	char command[1024];

	/* Close the last file (if any), open the next one */
	if(out!=NULL)
	{
		fclose(out);
		sprintf(command, "cat %s >> %s", outputfile, cutfile);
		system(command);
		unlink(outputfile);
	}

	if((out=fopen(outputfile,"a"))==NULL)
	{
		fprintf(stderr,"scat: can't create new output file '%s' (%s)",outputfile, strerror (errno));
		exit(errno);
	}
	sigaction(CHOPSIG, &handle, NULL);
}

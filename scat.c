/* static char SCCSid[] = "@(#)scat.c	1.5\t6/9/95"; */
/*
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

#define	CHOPSIG		SIGUSR1		/* Signal caught by command */
#define	usage()		fprintf(stderr,"Usage: %s <output file> <cut file>\n",argv[0])
#define PIDFILE         "/usr/home/uglymug/ugly/logs/scat.pid"

char *outputfile;
char *cutfile;
void handler();
FILE *out=NULL;
//extern char *sys_errlist[];

int main(argc,argv)
int argc;
char *argv[];
{
char l[1024];
FILE *pidfile=NULL;

	/* First things first;: set up the signal handler */
	if((int)signal(CHOPSIG, handler) < 0)
	{
		perror("signal() failed");
		exit(errno);
	}

	/* output process id */
	if((pidfile=fopen(PIDFILE,"w"))==NULL)
	{
		fprintf(stderr,"scat: can't create process file '%s' (%s)",PIDFILE,sys_errlist[errno]);
		exit(errno);
	}
	fprintf(pidfile, "%d\n", getpid() );
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
	handler();

	/* Let's go! */
	while(fgets(l, sizeof(l), stdin) != NULL)
	{
		fputs(l, out);	
	}
	fflush(out);
	handler();
	fclose(out);
	return 0;
}


void handler()
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
		fprintf(stderr,"scat: can't create new output file '%s' (%s)",outputfile,sys_errlist[errno]);
		exit(errno);
	}
}

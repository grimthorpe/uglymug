/* static char SCCSid[] = "@(#)start_tinytalk.c	1.2\t7/19/94"; */
/*
 * start_tinytalk.c: Start the tinytalk program on spec3, port 4201.
 *
 *	Peter Crowther, 25/10/90.
 */

#include <stdio.h>


main (argc, argv, envp)
int	argc;
char	**argv;
char	**envp;

{
	static	char	*my_argv [] = {"tinytalk", "spec0", "4201", NULL};

	execve ("/usr/local/bin/tinytalk", my_argv, envp);
	perror ("Couldn't exec tinytalk");
	exit (1);
}

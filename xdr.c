static char SCCSid[] = "@(#)xdr.c	1.2\t7/19/94";
/* This isn't true XDR, it's just an attempt at a transfer format that
   will be compatible between most hosts.  Used by the concentrator.

					 -- Ian Chard, 28/2/94
*/


char *int_to_xdr(unsigned int val)
{
	static char data[4];

	data[0]=val<<24 & 0xff;
	data[1]=val<<16 & 0xff;
	data[2]=val<<8 & 0xff;
	data[3]=val & 0xff;

	return data;
}


int xdr_to_int(char *msg)
{
	return (msg[0]>>24) + (msg[1]>>16) + (msg[2]>>8) + msg[3];
}


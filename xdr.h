#define	XDR_MESSAGE_LENGTH	4	/* Length of an int in bytes */

char *int_to_xdr(unsigned int val);
int xdr_to_int(char *msg);


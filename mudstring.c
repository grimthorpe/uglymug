#include "mudstring.h"

#ifndef OLD_STRING

StringBuffer StringBuffer::EMPTYBUFFER;
int StringBuffer::CreationCount = 0;

StringBuffer*
StringBuffer::NewBuffer(const char* ptr, unsigned int len = 0)
{
	if(!ptr || !*ptr)
	{
		return &EMPTYBUFFER;
	}

	CreationCount++;
	if(!len)
	{
		return new StringBuffer(ptr);
	}
	return new StringBuffer(ptr, len);
}
#endif

String NULLSTRING;
CString NULLCSTRING;

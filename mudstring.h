#include "copyright.h"
#ifndef _MUDSTRING_H
#define _MUDSTRING_H
//#pragma interface
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class	String;
class	CString
{
private:
	const char* buf;
	unsigned int len;

public:
	CString()
	{
		buf = 0;
		len = 0;
	}
	CString(const char* str)
	{
		buf = str;
		if(buf)
		{
			len = strlen(buf);
		}
		else
		{
			len = 0;
		}
	}
	CString(const CString& str)
	{
		buf = str.buf;
		len = str.len;
	}
	CString(const String& str);
	~CString() {}

	CString& operator=(const CString& str)
	{
		buf = str.buf;
		len = str.len;
		return *this;
	}
		operator bool()		const { return len != 0; }
	const char*	c_str()		const { return buf?buf:""; }
	unsigned int	length()	const { return len; }
};

class String
{
private:
	char* buf;
	unsigned int len;

	void init()
	{
		buf = 0;
		len = 0;
	}
protected:
	void copy(const char* str)
	{
		if(str != buf)
		{
			clear();
			if(str)
			{
				buf = strdup(str);
				len = strlen(buf);
			}
		}
	}
	void copy(const char* str, unsigned int slen)
	{
		if(str != buf)
		{
			clear();
			if(str)
			{
				buf = strdup(str);
				len = slen;
			}
		}
	}
public:
	String()
	{
		init();
	}
	String(const char* str)
	{
		init();
		copy(str);
	}
	String(const String& str)
	{
		init();
		copy(str.buf, str.len);
	}
	String(const CString& str)
	{
		init();
		if(str)
			copy(str.c_str(), str.length());
	}
	~String()
	{
		clear();
	}
	void clear()
	{
		if(buf)
		{
			free(buf);
		}
		init();
	}

	const char*	c_str()		const { return (buf)?buf:""; }
		operator bool()		const { return len != 0; }
	unsigned int	length()	const { return len; }

	String& operator=(const char* str)
	{
		copy(str);
		return *this;
	}
	String& operator=(const String& str)
	{
		if(str)
			copy(str.c_str(), str.length());
		else
			clear();
		return *this;
	}
	String& operator=(const CString& str)
	{
		if(str)
			copy(str.c_str(), str.length());
		else
			clear();
		return *this;
	}

	String& operator+=(const CString& str)
	{
		unsigned int newlen = len + str.length();
		char* newbuf = (char*)malloc(newlen+1);
		if(!newbuf)
		{
			return *this;
		}
		strcpy(newbuf, buf);
		strcat(newbuf, str.c_str());
		free(buf);
		buf = newbuf;
		len = newlen;

		return *this;
	}
	String operator+(const CString& str)
	{
		return String(*this)+= str;
	}
};

inline CString::CString(const String& str)
{
	buf = str.c_str();
	len = str.length();
}

extern String NULLSTRING;
extern CString NULLCSTRING;

#endif /* _MUDSTRING_H */

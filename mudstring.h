#include "copyright.h"
#ifndef _MUDSTRING_H
#define _MUDSTRING_H
//#pragma interface
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class	String;
class	CString;

class	CString
{
private:
	const char* buf;
	unsigned int len;

		operator int()	const;
		bool operator==(const CString& str);
		bool operator==(int);
	void empty()
	{
		buf = 0;
		len = 0;
	}
public:
	CString()
	{
		empty();
	}
	CString(const char* str)
	{
		if(str && *str)
		{
			buf = str;
			len = strlen(buf);
		}
		else
		{
			empty();
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
	CString& operator=(const char* str)
	{
		if(str && *str)
		{
			buf = str;
			len = strlen(str);
		}
		else
		{
			empty();
		}
		return *this;
	}
	CString& operator=(const String& str);

		operator bool()		const { return len != 0; }
	const char*	c_str()		const { return buf?buf:""; }
	unsigned int	length()	const { return len; }

	// Strict equivalence
	bool operator==(const CString& str) const
	{
		if(str.buf == buf)
		{
			return true;
		}
		return false;
	}
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
	operator int() const;
	bool operator==(const String& str);
	bool operator==(int);
protected:
	void copy(const char* str)
	{
		if(str != buf)
		{
			empty();
			if(str && *str)
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
			empty();
			if(str && *str)
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
		if(str)
			copy(str);
	}
	String(const String& str)
	{
		init();
		if(str)
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
		empty();
	}
	void empty()
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
		if(str && *str)
		{
			copy(str);
		}
		else
		{
			empty();
		}
		return *this;
	}
	String& operator=(const String& str)
	{
		if(str)
		{
			copy(str.c_str(), str.length());
		}
		else
		{
			empty();
		}
		return *this;
	}
	String& operator=(const CString& str)
	{
		if(str)
		{
			copy(str.c_str(), str.length());
		}
		else
		{
			empty();
		}
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
};

inline CString::CString(const String& str)
{
	if(str)
	{
		buf = str.c_str();
		len = str.length();
	}
	else
	{
		empty();
	}
}
inline CString& CString::operator=(const String& str)
{
	if(str)
	{
		buf = str.c_str();
		len = str.length();
	}
	else
	{
		empty();
	}
	return *this;
}

extern String NULLSTRING;
extern CString NULLCSTRING;

#endif /* _MUDSTRING_H */

#include "copyright.h"
#ifndef _MUDSTRING_H
#define _MUDSTRING_H
//#pragma interface
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Define OLD_STRING whilst Adrian sorts out bugs in the new string code.
 */

//#define OLD_STRING

class	String;
typedef String CString;
//class	CString;

extern String NULLSTRING;
extern CString NULLCSTRING;


#ifdef CSTRING
class	CString
{
private:
	const char* buf;
	unsigned int len;

		operator int()	const;
		bool operator==(const CString& str);
		bool operator==(int);
	void empty();
public:
	CString();
	CString(const char* str);
	CString(const CString& str);
	CString(const String& str);
	~CString();

	CString& operator=(const CString& str);
	CString& operator=(const char* str);
	CString& operator=(const String& str);

		operator bool()		const { return len != 0; }
	const char*	c_str()		const { return buf?buf:""; }
	unsigned int	length()	const { return len; }

	// Strict equivalence
	bool operator==(const CString& str) const;
};
#endif

#ifdef OLD_STRING

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

#else

#define STRINGBUFFER_GROWSIZE	64

class StringBuffer
{
	char*		_buf;
	unsigned int	_len;
	unsigned int	_capacity;
	int		_ref;

	void init();
	void resize(unsigned int newsize, bool copy=true);
	~StringBuffer(); // Private so that nobody can delete this. Use the reference counting!
	StringBuffer& operator=(const StringBuffer&);
	StringBuffer(const StringBuffer&);

	StringBuffer(unsigned int capacity = 0);
	StringBuffer(const char* str);
	StringBuffer(const char* str, unsigned int len);
	static StringBuffer EMPTYBUFFER;

public:
	static StringBuffer* NewBuffer(const char*str, unsigned int len = 0);

	void assign(const char* str, int len);
	void ref();
	void unref();
	int		refcount()	const	{ return _ref; }
	const char*	c_str()		const	{ return _buf?_buf:""; }
	unsigned int	length()	const	{ return _len; }
};

extern StringBuffer EMPTYBUFFER;

class	String
{
private:
	StringBuffer*	_buffer;

	operator int() const;
	bool operator==(const String& str);
	bool operator==(int);
public:
	~String();
	String(const char* str = 0);
	String(const String& str);
	String(StringBuffer* buf);
	String& operator=(const String& cstr);
	String& operator=(const char* cstr);
	const char*	c_str()		const	{ return _buffer->c_str(); }
	unsigned int	length()	const	{ return _buffer->length(); }
	operator bool()			const	{ return _buffer->length() > 0; }
#ifdef CSTRING
	String(const CString& str);
	String& operator=(const CString& cstr);
#endif
};

#endif // OLD_STRING

#ifdef OLD_STRING
#ifdef CSTRING
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

#endif // CSTRING
#endif // OLD_STRING
#endif /* _MUDSTRING_H */

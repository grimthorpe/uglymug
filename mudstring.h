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

#define OLD_STRING

class	String;
class	CString;

extern String NULLSTRING;
extern CString NULLCSTRING;


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
	int		_ref;
	char*		_buf;
	unsigned int	_len;
	unsigned int	_capacity;

	void init()
	{
		_capacity = 0;
		_len = 0;
		_buf = 0;
		_ref = 0;
	}
	void resize(unsigned int newsize, bool copy=true)
	{
		if(newsize <= _capacity)
			return;
		_capacity = newsize + 2;
		//while(newsize > _capacity)
		//	_capacity += STRINGBUFFER_GROWSIZE;
		char* tmp = (char*)malloc(_capacity+2); // Allow for slight overrun (JIC)
		if(_buf && copy)
		{
			// We use memcpy because the regions will not overlap.
			memcpy(tmp, _buf, _len); // Copy the \0 terminator
			tmp[_len+1] = 0;
		}
		if(_buf)
			free(_buf);
		_buf = tmp;
	}
	static int CreationCount;
	~StringBuffer() // Private so that nobody can delete this. Use the reference counting!
	{
		if(_buf)
			free(_buf);
		CreationCount--;
	}
	StringBuffer& operator=(const StringBuffer&);
	StringBuffer(const StringBuffer&);

	StringBuffer(unsigned int capacity = 0)
	{
		init();
		resize(capacity);
	}
	StringBuffer(const char* str)
	{
		init();
		if(str && *str)
		{
			assign(str, strlen(str));
		}
	}
	StringBuffer(const char* str, unsigned int len)
	{
		init();
		if(str && *str)
		{
			assign(str, len);
		}
	}
	static StringBuffer EMPTYBUFFER;
public:
	static StringBuffer* NewBuffer(const char*str, unsigned int len = 0);

	void assign(const char* str, int len)
	{
		resize(len, false);
		if(str)
		{
			memcpy(_buf, str, len);
			_len = len;
		}
		else
		{
			_len = 0;
		}
		if(_buf)
			_buf[len] = 0; // NULL terminate.
	}
	void ref()
	{
		if(this)
			_ref++;
	}
	void unref()
	{
		if((this) && ((--_ref) == 0))
		{
			delete const_cast<StringBuffer*>(this);
		}
	}
	int		refcount()	const	{ return _ref; }
	const char*	c_str()		const	{ return _buf?_buf:""; }
	unsigned int	length()	const	{ return _len; }
};

extern StringBuffer EMPTYBUFFER;

class	String
{
public:
	StringBuffer*	_buffer;

	operator int() const;
	bool operator==(const String& str);
	bool operator==(int);
public:
	~String()
	{
		if(_buffer)
			_buffer->unref();
	}
	String(const char* str = 0)
	{
		_buffer = StringBuffer::NewBuffer(str);
		_buffer->ref();
	}
	String(const String& str)
	{
		_buffer=str._buffer;
		_buffer->ref();
	}
	String(const CString& str)
	{
		_buffer = StringBuffer::NewBuffer(str.c_str(), str.length());
		_buffer->ref();
	}
	String(StringBuffer* buf)
	{
		_buffer = buf;
		_buffer->ref();
	}
	String& operator=(const String& cstr)
	{
		if(cstr._buffer != _buffer)
		{
			_buffer->unref();
			_buffer = cstr._buffer;
			_buffer->ref();
		}
		return *this;
	}
	String& operator=(const CString& cstr)
	{
		if(_buffer->refcount () != 1)
		{
			_buffer->unref();
			_buffer = StringBuffer::NewBuffer(cstr.c_str(), cstr.length());
			_buffer->ref();
		}
		else
			_buffer->assign(cstr.c_str(), cstr.length());
		return *this;
	}
	String& operator=(const char* cstr)
	{
		if(_buffer->refcount () != 1)
		{
			_buffer->unref();
			_buffer = StringBuffer::NewBuffer(cstr);
			_buffer->ref();
		}
		else if(cstr)
		{
			_buffer->assign(cstr, strlen(cstr));
		}
		else
		{
			_buffer->assign(0, 0);
		}
		return *this;
	}
	const char*	c_str()		const	{ return _buffer->c_str(); }
	unsigned int	length()	const	{ return _buffer->length(); }
	operator bool()			const	{ return _buffer->length() > 0; }
};

#endif // OLD_STRING

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


#endif /* _MUDSTRING_H */
